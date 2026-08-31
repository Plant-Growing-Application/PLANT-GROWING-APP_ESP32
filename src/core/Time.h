#pragma once

// Zaman tipleri — TASK-004
//
// ÜÇ AYRI TİP, BİLİNÇLİ OLARAK BİRBİRİNE DÖNÜŞMEZ:
//
//   Millis        monotonik zaman damgası (açılıştan beri geçen süre)
//   Duration      süre farkı
//   EpochSeconds  duvar saati (Unix epoch)
//
// NEDEN AYRI: SNTP senkronizasyonu duvar saatini GERİYE alabilir. "Pompa 3
// saattir çalışıyor" hesabı duvar saatiyle yapılırsa maxRunTime koruması
// (TASK-029) sessizce bozulur ve pompa korumasız kalır. Tip düzeyinde ayrım,
// bu karışıklığı derleme hatasına dönüştürür (CODING_STANDARDS Z4).
//
// TAŞMA GÜVENLİĞİ (Z5): millis() ~49.7 günde taşar; sistem aylarca çalışacaktır.
// Bu yüzden Millis üzerinde < ve > operatörleri BİLİNÇLİ OLARAK TANIMLANMAMIŞTIR.
// İki zaman damgasını doğrudan karşılaştırmak derlenmez; her zaman fark alınır.

#include <stdint.h>
#include <type_traits>

namespace core {

// ---------------------------------------------------------------------------
// Duration — süre farkı (milisaniye)
// ---------------------------------------------------------------------------
struct Duration
{
    uint32_t ms;

    constexpr bool operator<(Duration o) const { return ms < o.ms; }
    constexpr bool operator>(Duration o) const { return ms > o.ms; }
    constexpr bool operator<=(Duration o) const { return ms <= o.ms; }
    constexpr bool operator>=(Duration o) const { return ms >= o.ms; }
    constexpr bool operator==(Duration o) const { return ms == o.ms; }
    constexpr bool operator!=(Duration o) const { return ms != o.ms; }
};

constexpr Duration millisecs(uint32_t v) { return Duration{v}; }
constexpr Duration seconds(uint32_t v) { return Duration{v * 1000u}; }
constexpr Duration minutes(uint32_t v) { return Duration{v * 60u * 1000u}; }
constexpr Duration hours(uint32_t v) { return Duration{v * 60u * 60u * 1000u}; }

// ---------------------------------------------------------------------------
// Millis — monotonik zaman damgası
//
// Karşılaştırma operatörleri KASITLI OLARAK YOK. Taşma nedeniyle iki damganın
// doğrudan karşılaştırılması yanlış sonuç verir; her zaman elapsed()/hasElapsed()
// kullanılmalıdır.
// ---------------------------------------------------------------------------
struct Millis
{
    uint32_t v;

    constexpr bool operator==(Millis o) const { return v == o.v; }
    constexpr bool operator!=(Millis o) const { return v != o.v; }
};

/// İki damga arasında geçen süre.
///
/// Unsigned çıkarma taşmada doğal olarak sarar: `now` taşmış ve `since`
/// taşmamış olsa bile sonuç doğrudur. Bu, 49.7 günlük millis() döngüsünün
/// güvenle aşılmasını sağlar.
constexpr Duration elapsed(Millis now, Millis since)
{
    return Duration{static_cast<uint32_t>(now.v - since.v)};
}

/// `since` anından bu yana en az `d` kadar süre geçti mi?
constexpr bool hasElapsed(Millis now, Millis since, Duration d)
{
    return elapsed(now, since).ms >= d.ms;
}

/// Bir zaman damgasına süre ekler (zaman aşımı hedefi hesaplamak için).
constexpr Millis operator+(Millis t, Duration d)
{
    return Millis{static_cast<uint32_t>(t.v + d.ms)};
}

// ---------------------------------------------------------------------------
// EpochSeconds — duvar saati (Unix epoch, saniye)
//
// YALNIZCA gösterim ve çizelge kuralları için kullanılır (TASK-056).
// Süre ölçümü ve zaman aşımı için ASLA kullanılmaz — Millis kullanılır.
//
// int64: 2038 sorunundan etkilenmez.
// ---------------------------------------------------------------------------
struct EpochSeconds
{
    int64_t s;

    constexpr bool operator<(EpochSeconds o) const { return s < o.s; }
    constexpr bool operator>(EpochSeconds o) const { return s > o.s; }
    constexpr bool operator==(EpochSeconds o) const { return s == o.s; }
    constexpr bool operator!=(EpochSeconds o) const { return s != o.s; }
};

/// Zamanın hiç senkronize edilmediğini gösteren değer.
constexpr EpochSeconds EPOCH_INVALID = EpochSeconds{0};

constexpr bool isTimeValid(EpochSeconds t) { return t.s > 0; }

// ---------------------------------------------------------------------------
// Derleme zamanı doğrulama
// ---------------------------------------------------------------------------

static_assert(std::is_trivially_copyable<Millis>::value, "Millis POD olmali");
static_assert(std::is_trivially_copyable<Duration>::value, "Duration POD olmali");
static_assert(std::is_trivially_copyable<EpochSeconds>::value, "EpochSeconds POD olmali");
static_assert(sizeof(Millis) == 4, "Millis 4 bayt olmali");
static_assert(sizeof(Duration) == 4, "Duration 4 bayt olmali");

// --- Taşma davranışı: derleme zamanında kanıtlanır -------------------------
//
// millis() UINT32_MAX'a yakınken taşar. Aşağıdaki kontroller, fark alma
// yönteminin taşma sınırında doğru çalıştığını gösterir.

// Taşmadan hemen önce → hemen sonra: gerçek fark 10 ms
static_assert(elapsed(Millis{5u}, Millis{0xFFFFFFFBu}).ms == 10u,
              "elapsed() tasma sinirinda dogru calismali");

// Tam taşma noktası
static_assert(elapsed(Millis{0u}, Millis{0xFFFFFFFFu}).ms == 1u,
              "elapsed() tam tasmada dogru calismali");

// Normal aralık
static_assert(elapsed(Millis{1000u}, Millis{250u}).ms == 750u,
              "elapsed() normal aralikta dogru calismali");

// Zaman aşımı kontrolü taşma sınırında
static_assert(hasElapsed(Millis{5u}, Millis{0xFFFFFFFBu}, millisecs(10)),
              "hasElapsed() tasma sinirinda dogru calismali");
static_assert(!hasElapsed(Millis{5u}, Millis{0xFFFFFFFBu}, millisecs(11)),
              "hasElapsed() tasma sinirinda erken tetiklenmemeli");

// Süre yardımcıları
static_assert(seconds(2).ms == 2000u, "seconds() donusumu");
static_assert(minutes(1).ms == 60000u, "minutes() donusumu");
static_assert(hours(1).ms == 3600000u, "hours() donusumu");

} // namespace core
