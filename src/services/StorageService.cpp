#include "services/StorageService.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "services/ConfigService.h"
#include "services/HistoryStore.h"

namespace services {
namespace storage {
namespace {

using core::ErrCode;
using core::Millis;

QueueHandle_t g_queue    = nullptr;
uint32_t      g_dropped  = 0;
uint32_t      g_done     = 0;
Millis        g_lastSample{0};
bool          g_ready    = false;

/// Örnek periyodu. Kapasite hesabı buna dayanır: 60 sn → 14,2 gün.
constexpr uint32_t SAMPLE_PERIOD_MS = 60000u;

/// Config yazması için birleştirme (debounce) süresi.
///
/// Kullanıcı bir formu kaydettiğinde arka arkaya birkaç `PUT` gelir ve her
/// biri config'i kirletir. Her değişiklikte flash'a yazmak gereksiz aşınma
/// üretir; 2 saniye bekleyip HEPSİNİ TEK yazmada birleştiriyoruz.
constexpr uint32_t CONFIG_DEBOUNCE_MS = 2000u;

Millis g_dirtySince{0};
bool   g_dirtySeen = false;

void handle(const WriteRequest& r)
{
    switch (r.kind)
    {
        case WriteKind::CONFIG_PERSIST:
        {
            const ErrCode e = services::config::persist();
            if (e != ErrCode::OK)
            {
                core::diag::raise(e);
            }
            break;
        }

        case WriteKind::HISTORY_SAMPLE:
        {
            core::SystemState snap{};
            (void)core::state::snapshot(snap);
            (void)history::append(snap, snap.time.valid != 0u);
            break;
        }

        case WriteKind::NONE:
        default:
            break;
    }
    ++g_done;
}

} // namespace

core::ErrCode begin()
{
    g_queue = xQueueCreate(QUEUE_LEN, sizeof(WriteRequest));
    if (g_queue == nullptr)
    {
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::SYS_LOW_HEAP, 0,
                        "depolama kuyrugu olusturulamadi");
        return ErrCode::SYS_LOW_HEAP;
    }

    // Dosya sistemi yoksa `history::begin()` başarısız olur ama servis
    // çalışmaya devam eder: config yazması NVS'e gider, geçmiş devre dışı
    // kalır (P4 — fail-degraded).
    (void)history::begin();

    g_ready = true;
    return ErrCode::OK;
}

bool post(WriteKind kind, uint32_t param)
{
    if (!g_ready || g_queue == nullptr) { return false; }

    // ── ÖNCELİK: geçmiş düşürülür, config asla ─────────────────────────────
    // Geçmiş isteği yalnızca kuyrukta REZERV slot dışında yer varsa kabul
    // edilir. Böylece bir örnek seli config yazmasını dışarıda bırakamaz.
    if (kind == WriteKind::HISTORY_SAMPLE)
    {
        if (uxQueueMessagesWaiting(g_queue) >= HISTORY_MAX_USE)
        {
            ++g_dropped;
            return false;
        }
    }

    WriteRequest r{};
    r.kind  = kind;
    r.param = param;

    // Timeout 0: çağıran ASLA beklemez (`app_core`, AsyncTCP, `ui`).
    if (xQueueSend(g_queue, &r, 0) != pdTRUE)
    {
        ++g_dropped;
        if (kind == WriteKind::CONFIG_PERSIST)
        {
            // Config kaybı sessiz geçmemeli — kullanıcı ayarını yeniden
            // girmek zorunda kalır ve bunu fark etmesi zordur.
            core::diag::raise(ErrCode::STORAGE_WRITE_FAILED);
            core::diag::log(core::LogLevel::ERROR, ErrCode::STORAGE_WRITE_FAILED, 0,
                            "config yazma istegi kuyruga alinamadi");
        }
        return false;
    }
    return true;
}

void tick(Millis now)
{
    if (!g_ready) { return; }

    // ── KİRLİ CONFIG'İ YAZ ─────────────────────────────────────────────────
    // `ConfigService` güncellemeleri yalnızca RAM'i değiştirip config'i
    // KİRLİ işaretliyor; yazmayı kimse tetiklemiyordu. Sonuç: web'den
    // yapılan her ayar değişikliği yeniden başlatmada KAYBOLUYORDU.
    // `isDirty()` API'sinin hiç çağrılmıyor olması bu hatanın işaretiydi.
    if (services::config::isDirty())
    {
        if (!g_dirtySeen)
        {
            g_dirtySeen  = true;
            g_dirtySince = now;
        }
        else if (core::hasElapsed(now, g_dirtySince, core::millisecs(CONFIG_DEBOUNCE_MS)))
        {
            g_dirtySeen = false;
            (void)post(WriteKind::CONFIG_PERSIST);
        }
    }
    else
    {
        g_dirtySeen = false;
    }

    // Periyodik örnek isteğini BURADA üretiyoruz: `app_core`'un 60 sn
    // sayması, güvenlik döngüsüne bir sorumluluk daha eklemek olurdu.
    // Bu aynı zamanda `history::append()`'in yalnızca bu task'tan
    // çağrıldığını garanti eder.
    if (core::hasElapsed(now, g_lastSample, core::millisecs(SAMPLE_PERIOD_MS)))
    {
        g_lastSample = now;

        // §16.3: heap kritikse GEÇMİŞ YAZIMI DURAKLATILIR. Geçmiş verisi
        // kaybı en az zararlı olandır; grafikte bir boşluk, çökmeden iyidir.
        core::SystemState s{};
        (void)core::state::snapshot(s);
        if (s.system.freeHeapBytes >= core::LOW_HEAP_BYTES)
        {
            (void)post(WriteKind::HISTORY_SAMPLE);
        }
    }

    // Kuyruk boşken BLOKLAR (CPU harcamaz) ama en fazla `WAKE_TIMEOUT_MS`
    // sonra döner ki çağıran heartbeat besleyebilsin.
    WriteRequest r{};
    if (xQueueReceive(g_queue, &r, pdMS_TO_TICKS(WAKE_TIMEOUT_MS)) == pdTRUE)
    {
        handle(r);

        // Kuyrukta biriken varsa aynı uyanışta tüket — ama sınırlı sayıda,
        // yoksa heartbeat gecikir.
        uint8_t burst = 0;
        while (burst < 4u && xQueueReceive(g_queue, &r, 0) == pdTRUE)
        {
            handle(r);
            ++burst;
        }
    }
}

uint32_t droppedRequests()   { return g_dropped; }
uint32_t processedRequests() { return g_done; }

} // namespace storage
} // namespace services
