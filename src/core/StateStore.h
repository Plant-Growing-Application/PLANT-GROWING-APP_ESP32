#pragma once

// Merkezî durum deposu — TASK-007
//
// Mevcut sistemdeki korumasız global değişken erişimini (REQUIREMENTS Kritik
// Problem 2) yapısal olarak imkânsız kılar. `currentIP`, `currentMAC`,
// `Sensor.WaterFlow` gibi global'lerin yerini alır.
//
// DESEN: tek mutex + tam snapshot kopyası (ARCHITECTURE §4.3)
//
//   YAZAR:   publishX()  →  kilidi al → alt-state'i kopyala → versiyonu artır → bırak
//   OKUYUCU: snapshot()  →  kilidi al → 312 baytı kopyala → bırak → KİLİTSİZ çalış
//
// Okuyucu kilidi tutarak iş yapmaz: kopyayı alır ve serbest kalır. Kritik bölge
// yalnızca `memcpy` süresi kadardır (~1 µs).
//
// TEK YAZAR KURALI (P1): her alt-state'in bir sahip task'ı vardır
// (`StateOwnership.md`). Kural çalışma zamanında doğrulanır: ihlal REDDEDİLMEZ,
// loglanır ve sayılır — programlama hatası görünür olmalı ama sistemi bozmamalı.
//
// ISR'DEN ERİŞİM YASAKTIR.

#include <stdint.h>

#include "ErrorCodes.h"
#include "SystemState.h"

namespace core {

/// State bölümleri — tek yazar takibi bu granülerlikte yapılır.
enum class StateSection : uint8_t
{
    SYSTEM     = 0,
    NETWORK    = 1,
    SENSORS    = 2,
    ACTUATORS  = 3,
    SAFETY     = 4,
    AUTOMATION = 5,
    TIME       = 6,
    COUNT      = 7,
};

/// StateStore çalışma istatistikleri — TASK-043 (/api/diagnostics) ve
/// TASK-062 (profilleme) tarafından okunur.
struct StateStoreStats
{
    uint32_t publishCount;       ///< toplam yayınlama
    uint32_t snapshotCount;      ///< toplam snapshot alma
    uint32_t lockTimeouts;       ///< kilit alınamayan çağrı sayısı
    uint32_t ownershipViolations;///< tek yazar kuralı ihlali sayısı
    uint32_t maxLockHoldUs;      ///< görülen en uzun kritik bölge (µs)
};

namespace state {

/// Depoyu hazırlar; state sıfırlanır, versiyon 0'dan başlar.
/// Boot'un erken aşamalarında çağrılmalıdır (ARCHITECTURE §7.1 Aşama 2).
///
/// @return ErrCode::OK veya SYS_BOOT_STAGE_FAILED (mutex oluşturulamadı)
ErrCode begin();

// --- Yayınlama (yalnızca sahip task) ---------------------------------------
//
// Her fonksiyon ilgili alt-state'i tamamen değiştirir ve versiyonu artırır.
// Kısmi güncelleme yoktur: sahip task alt-state'in tamamını üretir.
//
// @return ErrCode::OK veya SYS_BOOT_STAGE_FAILED (kilit zaman aşımı)

ErrCode publishSystem(const SystemStatus& v);
ErrCode publishNetwork(const NetworkStatus& v);
ErrCode publishSensors(const SensorsStatus& v);
ErrCode publishActuators(const ActuatorsStatus& v);
ErrCode publishSafety(const SafetyStatus& v);
ErrCode publishAutomation(const AutomationStatus& v);
ErrCode publishTime(const TimeStatus& v);

// --- Okuma ------------------------------------------------------------------

/// Tutarlı bir anlık görüntü alır.
///
/// Kopya ÇAĞIRANIN tamponuna yazılır — 312 baytlık bir yapıyı değer olarak
/// döndürmek `ui` (3.5 KB) ve `app_core` (4 KB) yığınlarını gereksiz zorlar.
///
/// Kilit alınamazsa `out` DEĞİŞTİRİLMEZ; çağıran bir önceki kopyasıyla
/// devam edebilir.
ErrCode snapshot(SystemState& out);

/// Mevcut versiyon numarası. Değişim tespiti için kullanılır (ARCHITECTURE §4.2):
/// web gereksiz WS trafiği üretmez, UI ekranı gereksiz yeniden çizmez.
uint32_t version();

/// `a` versiyonu `b`'den yeni mi? Taşma güvenli.
///
/// Doğrudan `a > b` karşılaştırması taşma sınırında yanlış sonuç verir;
/// `Millis` ile aynı yaklaşım kullanılır (işaretli fark).
constexpr bool isNewerThan(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b) > 0;
}

// --- Teşhis ------------------------------------------------------------------

/// Çalışma istatistiklerini kopyalar.
void stats(StateStoreStats& out);

/// Bir bölümün sahibi olarak kaydedilmiş task'ı temizler.
/// Yalnızca test ve yeniden yapılandırma içindir.
void clearOwner(StateSection section);

/// State'i, versiyonu ve istatistikleri sıfırlar.
void reset();

} // namespace state
} // namespace core
