#include "TaskRegistry.h"

#include <Arduino.h>

#include <atomic>

namespace core {
namespace taskreg {
namespace {

constexpr uint8_t SLOTS = static_cast<uint8_t>(TaskId::COUNT);

// Slot başına atomik alanlar. Her task YALNIZCA kendi slotuna yazar; okuyucu
// (app_core) tüm slotları okur. Bu desen kilit gerektirmez.
//
// Alanlar bağımsız atomikler olduğu için okuyucu, aynı slotun alanlarını
// mikrosaniyeler arayla farklı döngülerden görebilir. Bu KABUL EDİLEBİLİR:
// izleme kararı `lastBeatAt` üzerinden verilir, diğer alanlar teşhis amaçlıdır.
std::atomic<uint32_t> g_lastBeatMs[SLOTS];
std::atomic<uint32_t> g_beatCount[SLOTS];
std::atomic<uint32_t> g_maxLoopUs[SLOTS];
std::atomic<uint32_t> g_overrunCount[SLOTS];
std::atomic<uint16_t> g_minFreeStack[SLOTS];
std::atomic<uint8_t>  g_class[SLOTS];
std::atomic<uint8_t>  g_registered[SLOTS];

inline bool validIndex(TaskId id, uint8_t& idx)
{
    idx = static_cast<uint8_t>(id);
    return idx < SLOTS;
}

} // namespace

void begin()
{
    for (uint8_t i = 0; i < SLOTS; ++i)
    {
        g_lastBeatMs[i].store(0, std::memory_order_relaxed);
        g_beatCount[i].store(0, std::memory_order_relaxed);
        g_maxLoopUs[i].store(0, std::memory_order_relaxed);
        g_overrunCount[i].store(0, std::memory_order_relaxed);
        g_minFreeStack[i].store(0xFFFFu, std::memory_order_relaxed);
        g_class[i].store(0, std::memory_order_relaxed);
        g_registered[i].store(0, std::memory_order_relaxed);
    }
}

void registerSelf(TaskId id, TaskClass cls)
{
    uint8_t idx;
    if (!validIndex(id, idx))
    {
        return;
    }
    g_class[idx].store(static_cast<uint8_t>(cls), std::memory_order_relaxed);
    g_lastBeatMs[idx].store(millis(), std::memory_order_relaxed);
    // Kayıt bayrağı EN SON: okuyucu bayrağı gördüğünde diğer alanlar hazırdır.
    g_registered[idx].store(1, std::memory_order_release);
}

void beat(TaskId id, uint32_t loopUs, bool overran)
{
    uint8_t idx;
    if (!validIndex(id, idx))
    {
        return;
    }

    g_lastBeatMs[idx].store(millis(), std::memory_order_relaxed);
    g_beatCount[idx].fetch_add(1, std::memory_order_relaxed);

    if (loopUs > g_maxLoopUs[idx].load(std::memory_order_relaxed))
    {
        g_maxLoopUs[idx].store(loopUs, std::memory_order_relaxed);
    }
    if (overran)
    {
        g_overrunCount[idx].fetch_add(1, std::memory_order_relaxed);
    }
}

void updateStack(TaskId id, uint16_t freeBytes)
{
    uint8_t idx;
    if (!validIndex(id, idx))
    {
        return;
    }
    if (freeBytes < g_minFreeStack[idx].load(std::memory_order_relaxed))
    {
        g_minFreeStack[idx].store(freeBytes, std::memory_order_relaxed);
    }
}

void health(TaskId id, TaskHealth& out)
{
    uint8_t idx;
    if (!validIndex(id, idx))
    {
        out.registered = 0;
        return;
    }

    out.registered        = g_registered[idx].load(std::memory_order_acquire);
    out.lastBeatAt        = Millis{g_lastBeatMs[idx].load(std::memory_order_relaxed)};
    out.beatCount         = g_beatCount[idx].load(std::memory_order_relaxed);
    out.maxLoopUs         = g_maxLoopUs[idx].load(std::memory_order_relaxed);
    out.overrunCount      = g_overrunCount[idx].load(std::memory_order_relaxed);
    out.minFreeStackBytes = g_minFreeStack[idx].load(std::memory_order_relaxed);
    out.taskClass         = static_cast<TaskClass>(g_class[idx].load(std::memory_order_relaxed));
}

Duration sinceLastBeat(TaskId id, Millis now)
{
    uint8_t idx;
    if (!validIndex(id, idx))
    {
        return Duration{0};
    }
    // `elapsed()` unsigned çıkarma kullanır: millis() taşmasında doğru çalışır.
    return elapsed(now, Millis{g_lastBeatMs[idx].load(std::memory_order_relaxed)});
}

uint8_t registeredCount()
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < SLOTS; ++i)
    {
        if (g_registered[i].load(std::memory_order_acquire) != 0)
        {
            ++n;
        }
    }
    return n;
}

} // namespace taskreg
} // namespace core
