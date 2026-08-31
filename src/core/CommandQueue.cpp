#include "CommandQueue.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

namespace core {
namespace cmdq {
namespace {

QueueHandle_t     g_queue = nullptr;
StaticQueue_t     g_queueStruct;
uint8_t           g_queueStorage[COMMAND_QUEUE_CAPACITY * sizeof(Command)];
std::atomic<uint32_t> g_rejected{0};

// --- Acil durdurma: kuyruktan bağımsız, kilitsiz yol --------------------------
//
// Kuyruk tamamen dolu olsa bile acil durdurma ulaşmalıdır (ARCHITECTURE §12.3).
// Bu yüzden ayrı bir atomik bayrak kullanılır; hiçbir kilit alınmaz.
std::atomic<bool>    g_estopPending{false};
std::atomic<uint8_t> g_estopSource{0};
std::atomic<uint16_t> g_estopReason{0};

} // namespace

ErrCode begin()
{
    if (g_queue != nullptr)
    {
        return ErrCode::OK;
    }

    // Statik ayırma: heap kullanılmaz (CODING_STANDARDS §5).
    g_queue = xQueueCreateStatic(COMMAND_QUEUE_CAPACITY, sizeof(Command), g_queueStorage,
                                 &g_queueStruct);
    if (g_queue == nullptr)
    {
        return ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    g_rejected.store(0, std::memory_order_relaxed);
    g_estopPending.store(false, std::memory_order_relaxed);
    return ErrCode::OK;
}

CommandResult post(const Command& cmd)
{
    if (g_queue == nullptr || cmd.type == CommandType::NONE)
    {
        return CommandResult::REJECTED_INVALID;
    }

    Command toSend = cmd;
    if (toSend.issuedAt.v == 0u)
    {
        toSend.issuedAt = Millis{millis()};
    }

    // Zaman aşımı SIFIR: bu çağrı AsyncTCP callback bağlamından gelebilir ve
    // bloklaması tüm web arayüzünü dondururdu (ARCHITECTURE §14.6).
    if (xQueueSend(g_queue, &toSend, 0) != pdTRUE)
    {
        // Kuyruk dolu. En eski komut DÜŞÜRÜLMEZ — kullanıcının ilk komutunu
        // sessizce yutmak kabul edilemez. Yeni komut reddedilir ve çağıran
        // kullanıcıya "meşgul" bildirir.
        g_rejected.fetch_add(1, std::memory_order_relaxed);
        return CommandResult::BUSY;
    }

    // Kuyruğa alındı — UYGULANDIĞI anlamına gelmez. Uygulama sonucu
    // `app_core` tarafından üretilir ve state üzerinden bildirilir.
    return CommandResult::ACCEPTED;
}

bool receive(Command& out)
{
    if (g_queue == nullptr)
    {
        return false;
    }
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

uint32_t rejectedCount()
{
    return g_rejected.load(std::memory_order_relaxed);
}

void postEmergencyStop(CommandSource source, ErrCode reason)
{
    // Sıra önemli: önce neden ve kaynak yazılır, EN SON bayrak set edilir.
    // Böylece okuyucu bayrağı gördüğünde neden alanları hazırdır.
    g_estopSource.store(static_cast<uint8_t>(source), std::memory_order_relaxed);
    g_estopReason.store(static_cast<uint16_t>(reason), std::memory_order_relaxed);
    g_estopPending.store(true, std::memory_order_release);
}

bool takeEmergencyStop(CommandSource& outSource, ErrCode& outReason)
{
    if (!g_estopPending.exchange(false, std::memory_order_acquire))
    {
        return false;
    }
    outSource = static_cast<CommandSource>(g_estopSource.load(std::memory_order_relaxed));
    outReason = static_cast<ErrCode>(g_estopReason.load(std::memory_order_relaxed));
    return true;
}

bool emergencyStopPending()
{
    return g_estopPending.load(std::memory_order_acquire);
}

void reset()
{
    if (g_queue != nullptr)
    {
        xQueueReset(g_queue);
    }
    g_rejected.store(0, std::memory_order_relaxed);
    g_estopPending.store(false, std::memory_order_relaxed);
}

} // namespace cmdq
} // namespace core
