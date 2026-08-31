#pragma once

// Otomasyon kural modeli — TASK-054
//
// ── KURALLAR VERİ, KOD DEĞİL ────────────────────────────────────────────────
//   Kod:  kural motoru (sabit, denetlenebilir)
//   Veri: kural tanımları (config'te, kullanıcı düzenler)
//
// Bu ayrım sayesinde "her 2 saatte 15 dk sula" ile "EC 1.0'ın altına düşünce
// besin ver" AYNI motorla çalışır ve yeni bir profil eklemek firmware
// güncellemesi gerektirmez.
//
// ── OTOMASYONUN BİLMEDİĞİ ŞEYLER (ARCHITECTURE §11.4) ───────────────────────
// Bu yapıda `minRunMs`, `cooldownMs`, güvenlik eşiği YOKTUR ve eklenmeyecek.
// Kural yalnızca "şu aktüatörün açık olmasını istiyorum" der:
//   · kısıtlar   → `ActuatorManager` (TASK-029)
//   · kilitler   → `SafetyMonitor`   (TASK-030)
//   · pin/polarite → `BoardPins`     (TASK-002)
//
// Kural motoru karmaşıklaşsa bile güvenlik mantığı sabit kalır.
//
// ── NEDEN `core/` İÇİNDE ─────────────────────────────────────────────────────
// Kural kümesi `Config`'in parçasıdır ve `Config` `core/`'dadır. Modeli
// `domain/models/` altında tutmak D5 ihlali olurdu: "core/ hiçbir katmana
// bağımlı değildir". Model burada; DEĞERLENDİRME `domain/` içinde.
//
// ── MAKRO ÇAKIŞMA NOTU (ISSUE-009) ──────────────────────────────────────────
// `RISING` ve `FALLING` Arduino kesme modlarıdır; kullanılmadı.

#include <stdint.h>

#include "SystemState.h"
#include "Time.h"

namespace core {

/// Kural sayısı SABİT. Dinamik liste heap kullanır ve NVS blob'una sığmaz.
constexpr uint8_t MAX_RULES = 8;

enum class RuleKind : uint8_t
{
    INACTIVE        = 0,  ///< boş slot
    THRESHOLD       = 1,  ///< sensör değeri + histerezis
    SCHEDULE_WINDOW = 2,  ///< gün içi saat aralığı (22:00–02:00 dahil)
    SCHEDULE_CYCLE  = 3,  ///< periyodik çevrim (N sn açık / P sn periyot)
};

/// Bir otomasyon kuralı. POD, 28 bayt.
///
/// **Düz yapı, `union` DEĞİL:** union 12 bayt/kural kazandırırdı ama yanlış
/// `kind` ile yanlış alanı okumak SESSİZ bir hata olurdu. 8 kural için
/// ~96 baytlık israf, o hata sınıfını tamamen ortadan kaldırmanın karşılığında
/// ucuzdur.
struct Rule
{
    // --- Ortak ---
    RuleKind         kind;
    ActuatorId target;
    uint8_t          enabled;
    uint8_t          priority;   ///< büyük olan çakışmayı kazanır

    /// Kuralın bu süreden sık tetiklenmesini engeller. Histerezisten AYRI
    /// bir koruma katmanıdır: histerezis değer gürültüsüne karşı, bu ise
    /// hızlı salınıma karşı.
    uint16_t minTriggerIntervalS;

    // --- THRESHOLD ---
    SensorId sensor;
    uint8_t        reserved;

    /// Yön İKİ EŞİKTEN TÜRETİLİR; ayrı bir "üstünde/altında" bayrağı YOKTUR.
    ///
    ///   onThreshold < offThreshold → değer onThreshold ALTINA düşünce AÇ,
    ///                                offThreshold'a yükselince KAPAT
    ///                                (örn. EC düştü → besin ver)
    ///   onThreshold > offThreshold → değer onThreshold ÜSTÜNE çıkınca AÇ,
    ///                                offThreshold'a düşünce KAPAT
    ///                                (örn. sıcaklık yükseldi → fan)
    ///
    /// Ayrı bir bayrak eşiklerle ÇELİŞEBİLİRDİ ve çelişki doğrulanmadan
    /// geçerse kural tersine çalışırdı. Alan var olmadığı için çelişki
    /// imkânsız.
    float onThreshold;
    float offThreshold;

    // --- SCHEDULE_WINDOW --- (gün içi dakika, 0–1439)
    uint16_t startMin;
    uint16_t endMin;

    // --- SCHEDULE_CYCLE ---
    uint16_t cycleOnS;      ///< çevrimin açık kalacağı süre
    uint16_t cyclePeriodS;  ///< çevrimin toplam periyodu
};

/// Kural kümesi. Kimlik = dizideki indeks (kararlı; API ve loglarda kullanılır).
struct RuleSet
{
    Rule    rules[MAX_RULES];
    uint8_t count;
    uint8_t reserved[3];
};

/// Bir kuralın çalışma durumu — **yayınlanmaz**, motorun içinde tutulur.
struct RuleRuntime
{
    Millis lastTriggerAt;
    Millis lastEvalAt;
    uint8_t      active;      ///< histerezis: şu an "açık" tarafında mıyız
    uint8_t      everRan;
    uint8_t      suspect;     ///< sensör kalitesi bozuk, sayaç işliyor
    uint8_t      reserved;
    Millis suspectSince;

    void reset()
    {
        lastTriggerAt = Millis{0};
        lastEvalAt    = Millis{0};
        active        = 0;
        everRan       = 0;
        suspect       = 0;
        suspectSince  = Millis{0};
    }
};

/// Bir kuralın ürettiği istek.
struct RuleVerdict
{
    ActuatorId target;
    uint8_t          wantOn;
    uint8_t          applies;   ///< 0 = bu kural şu an bir şey söylemiyor
    uint8_t          priority;
};

constexpr RuleVerdict noVerdict()
{
    return RuleVerdict{ActuatorId::NONE, 0u, 0u, 0u};
}

// --- Derleme zamanı doğrulama ----------------------------------------------

// Ölçülen değerler (tahmin değil): Rule 24, RuleSet 196, Config 588.
// Sınırlar ölçümün hemen üstünde tutuluyor ki bir alan eklendiğinde
// derleme UYARSIN ve NVS blob sınırı sessizce aşılmasın.
static_assert(sizeof(Rule) <= 24, "Rule buyudu — Config NVS blob sinirini kontrol et");
static_assert(sizeof(RuleSet) <= 200, "RuleSet buyudu");
static_assert(MAX_RULES == 8, "kural sayisi degistiyse Config boyutu yeniden olculmeli");

} // namespace core
