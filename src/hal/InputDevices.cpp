#include "InputDevices.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

#include "core/BoardPins.h"

namespace hal {
namespace input {
namespace {

using core::ErrCode;

constexpr uint8_t QUEUE_CAPACITY = 16;

QueueHandle_t     g_queue = nullptr;
StaticQueue_t     g_queueStruct;
uint8_t           g_queueStorage[QUEUE_CAPACITY * sizeof(InputEvent)];
std::atomic<uint32_t> g_dropped{0};

uint8_t g_stepsPerDetent = 4;

// --- Encoder durumu (ISR tarafından güncellenir) ----------------------------
volatile uint8_t g_encState    = 0;
volatile int8_t  g_encSteps    = 0;  ///< detente henüz dönüşmemiş adımlar
volatile bool    g_ready       = false;

/// Quadrature geçiş tablosu.
///
/// İndeks = (öncekiDurum << 2) | yeniDurum. Değer: +1 / -1 / 0 (geçersiz).
///
/// NEDEN TABLO: mevcut sistemde ISR içinde uzun bir `if/else if` zinciri vardı.
/// Tablo hem daha kısa hem sabit süreli — ISR'de olması gereken budur.
const int8_t kQuadTable[16] = {
    0, -1, +1,  0,
   +1,  0,  0, -1,
   -1,  0,  0, +1,
    0, +1, -1,  0,
};

/// Kuyruğa ISR-güvenli olay koyar.
inline void pushFromIsr(InputEventType type, ButtonId btn)
{
    InputEvent ev;
    ev.type        = type;
    ev.button      = btn;
    ev.reserved[0] = 0;
    ev.reserved[1] = 0;

    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(g_queue, &ev, &woken) != pdTRUE)
    {
        // Kuyruk dolu: olay düşürülür ve SAYILIR. Sessiz kayıp yok.
        // (ISR'den log çağrılamaz — CODING_STANDARDS §6.)
        g_dropped.fetch_add(1, std::memory_order_relaxed);
    }
    if (woken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

/// Encoder kesme işleyicisi.
///
/// YALNIZCA quadrature durumunu ilerletir ve bir detent tamamlandığında olay
/// üretir. Log, dinamik ayırma, bloklama YOK (CODING_STANDARDS §6).
void IRAM_ATTR encoderIsr()
{
    if (!g_ready)
    {
        return;
    }

    const uint8_t a  = static_cast<uint8_t>(digitalRead(board::ENCODER_A));
    const uint8_t b  = static_cast<uint8_t>(digitalRead(board::ENCODER_B));
    const uint8_t st = static_cast<uint8_t>((a << 1) | b);

    const int8_t delta = kQuadTable[((g_encState & 0x03u) << 2) | st];
    g_encState         = st;

    if (delta == 0)
    {
        return;  // geçersiz geçiş (gürültü) — yok sayılır
    }

    g_encSteps = static_cast<int8_t>(g_encSteps + delta);

    // Detent normalizasyonu TAMSAYI aritmetiğiyle. Mevcut sistemdeki
    // `1.5` (double) değeri int sayaçla karşılaştırılıyordu.
    const int8_t threshold = static_cast<int8_t>(g_stepsPerDetent);

    if (g_encSteps >= threshold)
    {
        g_encSteps = static_cast<int8_t>(g_encSteps - threshold);
        pushFromIsr(InputEventType::ENCODER_CW, ButtonId::COUNT);
    }
    else if (g_encSteps <= -threshold)
    {
        g_encSteps = static_cast<int8_t>(g_encSteps + threshold);
        pushFromIsr(InputEventType::ENCODER_CCW, ButtonId::COUNT);
    }
}

// --- Buton durumu (task bağlamında yorumlanır) -------------------------------
//
// Butonlar ISR yerine `poll()` içinde örneklenir: debounce ve uzun/kısa basış
// ayrımı ZAMAN gerektirir ve bunu ISR'de yapmak kuralı ihlal ederdi.

struct ButtonState
{
    uint8_t  pin;
    bool     lastStable;
    bool     lastRaw;
    uint32_t lastChangeMs;
    uint32_t pressedAtMs;
    bool     longFired;
};

ButtonState g_buttons[static_cast<uint8_t>(ButtonId::COUNT)] = {};

/// Butonları örnekler ve olay üretir. `poll()` içinden çağrılır.
void sampleButtons()
{
    const uint32_t now = millis();

    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::COUNT); ++i)
    {
        ButtonState& b = g_buttons[i];

        // Pull-up'lı giriş: basılı = LOW.
        const bool raw = (digitalRead(b.pin) == LOW);

        if (raw != b.lastRaw)
        {
            b.lastRaw       = raw;
            b.lastChangeMs  = now;
            continue;  // debounce penceresi başladı
        }

        // Kararlı hâle geldi mi?
        if ((now - b.lastChangeMs) < DEBOUNCE_MS)
        {
            continue;
        }

        if (raw != b.lastStable)
        {
            b.lastStable = raw;
            if (raw)
            {
                b.pressedAtMs = now;
                b.longFired   = false;
            }
            else if (!b.longFired)
            {
                // Bırakıldı ve uzun basış tetiklenmemişti → kısa basış.
                InputEvent ev{InputEventType::BUTTON_SHORT, static_cast<ButtonId>(i), {0, 0}};
                if (xQueueSend(g_queue, &ev, 0) != pdTRUE)
                {
                    g_dropped.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        else if (raw && !b.longFired && (now - b.pressedAtMs) >= LONG_PRESS_MS)
        {
            // Basılı tutuluyor ve eşik aşıldı → uzun basış (bırakılmayı beklemez).
            b.longFired = true;
            InputEvent ev{InputEventType::BUTTON_LONG, static_cast<ButtonId>(i), {0, 0}};
            if (xQueueSend(g_queue, &ev, 0) != pdTRUE)
            {
                g_dropped.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

} // namespace

core::ErrCode begin(uint8_t stepsPerDetent)
{
    if (stepsPerDetent == 0u)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    g_stepsPerDetent = stepsPerDetent;

    if (g_queue == nullptr)
    {
        g_queue = xQueueCreateStatic(QUEUE_CAPACITY, sizeof(InputEvent), g_queueStorage,
                                     &g_queueStruct);
        if (g_queue == nullptr)
        {
            return ErrCode::SYS_BOOT_STAGE_FAILED;
        }
    }

    pinMode(board::ENCODER_A, INPUT_PULLUP);
    pinMode(board::ENCODER_B, INPUT_PULLUP);

    g_buttons[static_cast<uint8_t>(ButtonId::ENCODER_PUSH)].pin = board::ENCODER_PUSH;
    g_buttons[static_cast<uint8_t>(ButtonId::BACK)].pin         = board::BUTTON_BACK;

    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::COUNT); ++i)
    {
        pinMode(g_buttons[i].pin, INPUT_PULLUP);
        g_buttons[i].lastStable   = false;
        g_buttons[i].lastRaw      = false;
        g_buttons[i].lastChangeMs = millis();
        g_buttons[i].longFired    = false;
    }

    g_encState = static_cast<uint8_t>((digitalRead(board::ENCODER_A) << 1) |
                                      digitalRead(board::ENCODER_B));
    g_encSteps = 0;
    g_ready    = true;

    attachInterrupt(digitalPinToInterrupt(board::ENCODER_A), encoderIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(board::ENCODER_B), encoderIsr, CHANGE);

    return ErrCode::OK;
}

bool poll(InputEvent& out)
{
    if (g_queue == nullptr)
    {
        return false;
    }
    sampleButtons();
    return xQueueReceive(g_queue, &out, 0) == pdTRUE;
}

uint8_t pending()
{
    if (g_queue == nullptr)
    {
        return 0;
    }
    return static_cast<uint8_t>(uxQueueMessagesWaiting(g_queue));
}

uint32_t droppedEvents()
{
    return g_dropped.load(std::memory_order_relaxed);
}

} // namespace input
} // namespace hal
