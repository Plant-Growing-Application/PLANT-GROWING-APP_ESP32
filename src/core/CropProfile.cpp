#include "CropProfile.h"

#include <string.h>

namespace core {
namespace {

// ---------------------------------------------------------------------------
// KATALOG
//
// Kaynaklar ve sapma payı: docs/CROP_PROFILES.md
//
// ── OKUMA ANAHTARI ─────────────────────────────────────────────────────────
//   ph / ec / waterTemp / airTemp / humidity  → hedef bantlar
//   lightMinutesPerDay                        → fotoperiyot (dakika)
//   irrigationOnS / irrigationPeriodS         → sulama çevrimi
//   aerationOnS / aerationPeriodS             → havalandırma çevrimi
//   durationDays                              → dönem uzunluğu (0 = son)
//
// ── EC NEDEN DÖNEME GÖRE DEĞİŞİYOR ─────────────────────────────────────────
// Bu tablonun asıl varlık nedeni budur. Çileği domates EC'sinde çalıştırmak
// yaprak büyütür ve MEYVE TUTUMUNU ÖLDÜRÜR; aynı ürün içinde de fide dönemi
// meyve dönemiyle aynı çözeltiyi kaldıramaz. Tek bir "ortalama" değer, iki
// dönemde birden yanlış olurdu.
//
// ── İKİ DÖNEMLİ ÜRÜNLERDE SON İKİ SATIR ────────────────────────────────────
// Marul ve fesleğende çiçeklenme/meyve dönemi YOKTUR (`stageCount = 2`).
// Kullanılmayan satırlar SIFIRLANMAZ, son geçerli dönemin kopyasıdır:
// sıfırlanmış bir satır `ph{0,0}`, `ec{0,0}` demek olurdu ve bir hata sonucu
// oraya erişilirse EC eşiği 0'a kurulup dozaj pompası hiç durmazdı. Kopya,
// aynı hatayı zararsız kılar.
// ---------------------------------------------------------------------------
constexpr CropProfile kCatalog[CROP_CATALOG_COUNT] = {
    // ---------------------------------------------------------------- ÇİLEK
    {CropId::STRAWBERRY, 4, 2, 0, "strawberry", "Çilek",
     {
         // fide
         {{5.5f, 6.2f}, {0.8f, 1.2f}, {18.0f, 22.0f}, {18.0f, 24.0f}, {65.0f, 80.0f},
          600, 60, 1800, 900, 1800, 21},
         // gelişme
         {{5.5f, 6.2f}, {1.2f, 1.4f}, {18.0f, 22.0f}, {18.0f, 24.0f}, {65.0f, 80.0f},
          720, 120, 1800, 900, 1800, 30},
         // çiçeklenme — potasyum ağırlıklı besleme, EC belirgin yükselir
         {{5.5f, 6.2f}, {1.6f, 2.0f}, {18.0f, 22.0f}, {18.0f, 24.0f}, {60.0f, 75.0f},
          840, 180, 1800, 1200, 1800, 21},
         // meyve
         {{5.5f, 6.2f}, {1.8f, 2.2f}, {18.0f, 22.0f}, {18.0f, 24.0f}, {60.0f, 75.0f},
          840, 180, 1200, 1200, 1800, 0},
     }},

    // -------------------------------------------------------------- DOMATES
    {CropId::TOMATO, 4, 3, 0, "tomato", "Domates",
     {
         {{5.5f, 6.3f}, {1.0f, 1.5f}, {20.0f, 24.0f}, {21.0f, 27.0f}, {60.0f, 75.0f},
          840, 90, 1800, 900, 1800, 21},
         {{5.5f, 6.3f}, {1.8f, 2.5f}, {20.0f, 24.0f}, {21.0f, 27.0f}, {60.0f, 70.0f},
          960, 150, 1800, 1200, 1800, 30},
         {{5.5f, 6.3f}, {2.2f, 3.0f}, {20.0f, 24.0f}, {21.0f, 27.0f}, {60.0f, 70.0f},
          960, 210, 1800, 1200, 1800, 25},
         {{5.5f, 6.3f}, {2.5f, 3.5f}, {20.0f, 24.0f}, {21.0f, 27.0f}, {55.0f, 70.0f},
          960, 240, 1200, 1200, 1800, 0},
     }},

    // ---------------------------------------------------------------- BİBER
    {CropId::PEPPER, 4, 3, 0, "pepper", "Biber",
     {
         {{5.8f, 6.3f}, {1.0f, 1.4f}, {20.0f, 25.0f}, {21.0f, 28.0f}, {60.0f, 75.0f},
          840, 90, 1800, 900, 1800, 25},
         {{5.8f, 6.3f}, {1.8f, 2.2f}, {20.0f, 25.0f}, {21.0f, 28.0f}, {60.0f, 75.0f},
          840, 150, 1800, 1200, 1800, 35},
         {{5.8f, 6.3f}, {2.0f, 2.5f}, {20.0f, 25.0f}, {21.0f, 28.0f}, {60.0f, 75.0f},
          960, 180, 1800, 1200, 1800, 25},
         {{5.8f, 6.3f}, {2.5f, 3.0f}, {20.0f, 25.0f}, {21.0f, 28.0f}, {55.0f, 70.0f},
          960, 240, 1500, 1200, 1800, 0},
     }},

    // ------------------------------------------------------------ SALATALIK
    {CropId::CUCUMBER, 4, 2, 0, "cucumber", "Salatalık",
     {
         {{5.5f, 6.0f}, {1.0f, 1.4f}, {20.0f, 24.0f}, {22.0f, 28.0f}, {65.0f, 80.0f},
          720, 90, 1800, 900, 1800, 14},
         {{5.5f, 6.0f}, {1.7f, 2.0f}, {20.0f, 24.0f}, {22.0f, 28.0f}, {65.0f, 80.0f},
          840, 150, 1800, 1200, 1800, 21},
         {{5.5f, 6.0f}, {1.8f, 2.2f}, {20.0f, 24.0f}, {22.0f, 28.0f}, {60.0f, 75.0f},
          840, 210, 1500, 1200, 1800, 14},
         {{5.5f, 6.0f}, {2.0f, 2.5f}, {20.0f, 24.0f}, {22.0f, 28.0f}, {60.0f, 75.0f},
          900, 240, 1200, 1200, 1800, 0},
     }},

    // ---------------------------------------------------------------- MARUL
    // İki dönem: yapraklı üründe çiçeklenme İSTENMEZ (acılaşır, "sapa kalkar").
    {CropId::LETTUCE, 2, 1, 0, "lettuce", "Marul",
     {
         {{5.5f, 6.5f}, {0.6f, 0.9f}, {18.0f, 22.0f}, {15.0f, 22.0f}, {50.0f, 70.0f},
          720, 120, 1800, 900, 1800, 14},
         {{5.5f, 6.5f}, {0.8f, 1.2f}, {18.0f, 22.0f}, {15.0f, 22.0f}, {50.0f, 70.0f},
          840, 240, 1800, 900, 1800, 0},
         // kullanılmayan — son geçerli dönemin kopyası (yukarıdaki gerekçe)
         {{5.5f, 6.5f}, {0.8f, 1.2f}, {18.0f, 22.0f}, {15.0f, 22.0f}, {50.0f, 70.0f},
          840, 240, 1800, 900, 1800, 0},
         {{5.5f, 6.5f}, {0.8f, 1.2f}, {18.0f, 22.0f}, {15.0f, 22.0f}, {50.0f, 70.0f},
          840, 240, 1800, 900, 1800, 0},
     }},

    // ------------------------------------------------------------- FESLEĞEN
    {CropId::BASIL, 2, 1, 0, "basil", "Fesleğen",
     {
         {{5.5f, 6.0f}, {0.6f, 1.0f}, {20.0f, 24.0f}, {20.0f, 27.0f}, {55.0f, 70.0f},
          720, 120, 1800, 900, 1800, 18},
         {{5.5f, 6.0f}, {1.0f, 1.6f}, {20.0f, 24.0f}, {20.0f, 27.0f}, {55.0f, 70.0f},
          840, 240, 1800, 900, 1800, 0},
         {{5.5f, 6.0f}, {1.0f, 1.6f}, {20.0f, 24.0f}, {20.0f, 27.0f}, {55.0f, 70.0f},
          840, 240, 1800, 900, 1800, 0},
         {{5.5f, 6.0f}, {1.0f, 1.6f}, {20.0f, 24.0f}, {20.0f, 27.0f}, {55.0f, 70.0f},
          840, 240, 1800, 900, 1800, 0},
     }},
};

/// Üretilen kuralların önceliği.
///
/// Hepsi AYNI: her kural FARKLI bir aktüatörü hedefler, dolayısıyla
/// `validateRules`'un "aynı hedef + aynı öncelik" çakışma kontrolü tetiklenmez.
/// Farklı öncelikler vermek, aralarında olmayan bir sıralama varmış izlenimi
/// yaratırdı.
constexpr uint8_t GENERATED_PRIORITY = 10;

/// Eşik kurallarının en sık tetiklenme aralığı (saniye).
///
/// Isıtıcı için histerezis zaten salınımı engeller; bu, sensör gürültüsünün
/// bandı hızla geçtiği durumlara karşı ikinci katmandır.
constexpr uint16_t HEATER_MIN_INTERVAL_S = 60;

/// Dozaj için çok daha uzun: besinin hazneye karışması dakikalar sürer.
constexpr uint16_t DOSING_MIN_INTERVAL_S = 300;

/// Çevrimin açık kalma süresini yoğunluğa göre ölçekler.
///
/// Sonuç HER ZAMAN geçerli bir çevrim üretir: `validateRule` `cycleOnS > 0` ve
/// `cycleOnS < cyclePeriodS` ister. Kırpma burada yapılır ki geçersiz bir kural
/// hiç doğmasın — doğsaydı, `updateRules` tüm kümeyi reddeder ve kullanıcı
/// "çilek profili uygulanamadı" hatasını alırdı.
uint16_t scaleOnSeconds(uint16_t baseOnS, Intensity intensity, uint16_t periodS)
{
    uint32_t scaled = baseOnS;
    if (intensity == Intensity::SPARSE)
    {
        scaled = (scaled * 70u) / 100u;
    }
    else if (intensity == Intensity::ABUNDANT)
    {
        scaled = (scaled * 140u) / 100u;
    }

    if (scaled > MAX_GENERATED_ON_S)
    {
        scaled = MAX_GENERATED_ON_S;
    }

    // Periyodun altında kalmalı. Periyot patolojik biçimde kısaysa bile
    // en az 1 saniyelik geçerli bir çevrim üretilir.
    const uint32_t ceiling = (periodS > 10u) ? static_cast<uint32_t>(periodS - 10u) : 1u;
    if (scaled > ceiling)
    {
        scaled = ceiling;
    }
    if (scaled == 0u)
    {
        scaled = 1u;
    }
    return static_cast<uint16_t>(scaled);
}

/// Boş bir slotu ortak alanlarla doldurur.
void initRule(Rule& r, RuleKind kind, ActuatorId target)
{
    r                     = Rule{};
    r.kind                = kind;
    r.target              = target;
    r.enabled             = 1u;
    r.priority            = GENERATED_PRIORITY;
    r.minTriggerIntervalS = 0u;
    r.sensor              = SensorId::NONE;
}

inline bool enabledAt(const bool* flags, ActuatorId id)
{
    return flags != nullptr && flags[static_cast<uint8_t>(id)];
}

inline bool sensorReady(const bool* flags, SensorId id)
{
    return flags != nullptr && flags[static_cast<uint8_t>(id)];
}

} // namespace

// ---------------------------------------------------------------------------
// Katalog erişimi
// ---------------------------------------------------------------------------

uint8_t cropCount()
{
    return CROP_CATALOG_COUNT;
}

const CropProfile* cropAt(uint8_t index)
{
    return (index < CROP_CATALOG_COUNT) ? &kCatalog[index] : nullptr;
}

const CropProfile* cropById(CropId id)
{
    for (uint8_t i = 0; i < CROP_CATALOG_COUNT; ++i)
    {
        if (kCatalog[i].id == id)
        {
            return &kCatalog[i];
        }
    }
    return nullptr;
}

const CropProfile* cropByKey(const char* key)
{
    if (key == nullptr || key[0] == '\0')
    {
        return nullptr;
    }
    for (uint8_t i = 0; i < CROP_CATALOG_COUNT; ++i)
    {
        if (strcmp(kCatalog[i].key, key) == 0)
        {
            return &kCatalog[i];
        }
    }
    return nullptr;
}

const char* cropKeyOf(CropId id)
{
    if (id == CropId::NONE)   { return "none"; }
    if (id == CropId::CUSTOM) { return "custom"; }

    const CropProfile* p = cropById(id);
    return (p != nullptr) ? p->key : "none";
}

bool cropIdFromKey(const char* key, CropId& out)
{
    if (key == nullptr) { return false; }

    if (strcmp(key, "none") == 0)   { out = CropId::NONE;   return true; }
    if (strcmp(key, "custom") == 0) { out = CropId::CUSTOM; return true; }

    const CropProfile* p = cropByKey(key);
    if (p == nullptr) { return false; }

    out = p->id;
    return true;
}

const char* stageKeyOf(GrowthStage s)
{
    switch (s)
    {
        case GrowthStage::SEEDLING:   return "seedling";
        case GrowthStage::VEGETATIVE: return "vegetative";
        case GrowthStage::FLOWERING:  return "flowering";
        case GrowthStage::FRUITING:   return "fruiting";
        default:                      return "seedling";
    }
}

bool stageFromKey(const char* key, GrowthStage& out)
{
    if (key == nullptr) { return false; }

    if (strcmp(key, "seedling") == 0)   { out = GrowthStage::SEEDLING;   return true; }
    if (strcmp(key, "vegetative") == 0) { out = GrowthStage::VEGETATIVE; return true; }
    if (strcmp(key, "flowering") == 0)  { out = GrowthStage::FLOWERING;  return true; }
    if (strcmp(key, "fruiting") == 0)   { out = GrowthStage::FRUITING;   return true; }
    return false;
}

bool stageValidFor(const CropProfile& p, GrowthStage s)
{
    return static_cast<uint8_t>(s) < p.stageCount;
}

// ---------------------------------------------------------------------------
// Kural üretimi
// ---------------------------------------------------------------------------

uint8_t buildCropRules(const CropProfile& p, GrowthStage stage, Intensity intensity,
                       const bool* actuatorEnabled, const bool* sensorEnabled, RuleSet& out)
{
    // Sıfırlanmış küme: yazılmayan her slot INACTIVE kalır. Önceki profilin
    // bir kuralının yeni kümede yaşamaya devam etmesi imkânsız.
    out = RuleSet{};
    for (uint8_t i = 0; i < MAX_RULES; ++i)
    {
        out.rules[i].kind   = RuleKind::INACTIVE;
        out.rules[i].target = ActuatorId::NONE;
    }

    // Geçersiz dönem istendiyse profilin son geçerli dönemine düşülür.
    // Sessiz değil: çağıran `stageValidFor()` ile önceden sorabilir; burada
    // yalnızca dizinin dışını okumamayı garanti ediyoruz.
    uint8_t si = static_cast<uint8_t>(stage);
    if (si >= p.stageCount)
    {
        si = static_cast<uint8_t>(p.stageCount > 0u ? p.stageCount - 1u : 0u);
    }
    const CropStage& st = p.stages[si];

    uint8_t n = 0;

    // --- 1) Sulama çevrimi -------------------------------------------------
    if (enabledAt(actuatorEnabled, ActuatorId::WATER_PUMP))
    {
        Rule& r = out.rules[n++];
        initRule(r, RuleKind::SCHEDULE_CYCLE, ActuatorId::WATER_PUMP);
        r.cyclePeriodS = st.irrigationPeriodS;
        r.cycleOnS     = scaleOnSeconds(st.irrigationOnS, intensity, st.irrigationPeriodS);
    }

    // --- 2) Havalandırma çevrimi -------------------------------------------
    //
    // Yoğunluk ölçeklemesi UYGULANMAZ: "bol sula" isteyen kullanıcı daha çok
    // su ister, daha çok hava değil. Havalandırma ihtiyacı kök kütlesine
    // bağlıdır ve sulama tercihiyle ilgisizdir.
    if (enabledAt(actuatorEnabled, ActuatorId::AIR_PUMP))
    {
        Rule& r = out.rules[n++];
        initRule(r, RuleKind::SCHEDULE_CYCLE, ActuatorId::AIR_PUMP);
        r.cyclePeriodS = st.aerationPeriodS;
        r.cycleOnS     = scaleOnSeconds(st.aerationOnS, Intensity::NORMAL, st.aerationPeriodS);
    }

    // --- 3) Işık penceresi -------------------------------------------------
    if (enabledAt(actuatorEnabled, ActuatorId::GROW_LIGHT) && st.lightMinutesPerDay > 0u &&
        st.lightMinutesPerDay < 1440u)
    {
        Rule& r = out.rules[n++];
        initRule(r, RuleKind::SCHEDULE_WINDOW, ActuatorId::GROW_LIGHT);
        r.startMin = LIGHT_WINDOW_START_MIN;
        r.endMin   = static_cast<uint16_t>((LIGHT_WINDOW_START_MIN + st.lightMinutesPerDay) %
                                           1440u);
    }

    // --- 4) Isıtıcı eşiği --------------------------------------------------
    //
    // Yön İKİ EŞİKTEN türer (Rule.h): `onThreshold < offThreshold` olduğu için
    // "değer onThreshold ALTINA düşünce AÇ, offThreshold'a yükselince KAPAT".
    // Isıtma için doğru yön budur ve ayrı bir bayrak olmadığı için ters
    // kurulması imkânsızdır.
    if (enabledAt(actuatorEnabled, ActuatorId::HEATER) &&
        sensorReady(sensorEnabled, SensorId::WATER_TEMP))
    {
        Rule& r = out.rules[n++];
        initRule(r, RuleKind::THRESHOLD, ActuatorId::HEATER);
        r.sensor              = SensorId::WATER_TEMP;
        r.onThreshold         = st.waterTemp.min;
        r.offThreshold        = st.waterTemp.min + HEATER_HYSTERESIS_C;
        r.minTriggerIntervalS = HEATER_MIN_INTERVAL_S;
    }

    // --- 5) Besin dozajı ---------------------------------------------------
    if (enabledAt(actuatorEnabled, ActuatorId::NUTRIENT_PUMP) &&
        sensorReady(sensorEnabled, SensorId::EC))
    {
        Rule& r = out.rules[n++];
        initRule(r, RuleKind::THRESHOLD, ActuatorId::NUTRIENT_PUMP);
        r.sensor              = SensorId::EC;
        r.onThreshold         = st.ec.min;
        r.offThreshold        = st.ec.min + EC_DOSE_BAND;
        r.minTriggerIntervalS = DOSING_MIN_INTERVAL_S;
    }

    out.count = n;
    return n;
}

GrowthStage stageForDay(const CropProfile& p, uint32_t daysSincePlanting)
{
    uint32_t cumulative = 0;

    for (uint8_t i = 0; i < p.stageCount; ++i)
    {
        const uint16_t d = p.stages[i].durationDays;

        // 0 = süresiz son dönem. Buraya ulaşıldıysa dönem budur.
        if (d == 0u)
        {
            return static_cast<GrowthStage>(i);
        }

        cumulative += d;
        if (daysSincePlanting < cumulative)
        {
            return static_cast<GrowthStage>(i);
        }
    }

    // Tablodaki toplam süre aşıldı: son geçerli dönemde kal. Ürün ömrünü
    // doldurmuş olabilir ama dönemi GERİ almak veya tanımsız bir döneme
    // geçmek daha kötü olurdu.
    return static_cast<GrowthStage>(p.stageCount > 0u ? p.stageCount - 1u : 0u);
}

} // namespace core
