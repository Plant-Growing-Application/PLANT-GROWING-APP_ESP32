#include "ConfigValidation.h"

#include <string.h>

namespace core {

// ---------------------------------------------------------------------------
// Güvenli varsayılanlar
//
// İLKE: bir varsayılan belirsizse GÜVENLİ olan seçilir. Cihaz ilk açıldığında
// kendiliğinden sulamamalıdır — kurulum sırasında pompa kuru çalışabilir.
// ---------------------------------------------------------------------------
void loadDefaults(Config& out)
{
    memset(&out, 0, sizeof(out));

    out.magic         = CONFIG_MAGIC;
    out.schemaVersion = CONFIG_SCHEMA_VERSION;

    // --- network: DHCP, kayıtlı ağ yok → AP kurulum moduna düşülür ---
    out.network.ipMode = IpMode::DHCP;
    out.network.ssid.clear();

    // --- sensors: hepsi devre dışı; donanım doğrulanınca açılır ---
    for (uint8_t i = 0; i < MAX_SENSORS; ++i)
    {
        out.sensors[i].enabled         = 0;
        out.sensors[i].offset          = 0.0f;
        out.sensors[i].scale           = 1.0f;
        out.sensors[i].validRange      = Range<float>{-1000.0f, 1000.0f};
        out.sensors[i].maxChangePerSec = 0.0f;  // 0 = sınır kapalı
        out.sensors[i].filterStrength  = 4;
    }

    // GUVENLIK SENSORLERI VARSAYILAN OLARAK ACIK.
    //
    // Neden: kapali birakilirsa `requireLevelSensor` korumasi sessizce devre
    // disi kalirdi — pompa seviye korumasi OLMADAN calisabilirdi. Sensor
    // fiziksel olarak takili degilse FAULT okunur ve pompa KILITLENIR;
    // bu fail-safe davranistir (ARCHITECTURE §9.5, §12.2) ve dogru olandir.
    out.sensors[static_cast<uint8_t>(SensorId::WATER_LEVEL)].enabled = 1;
    out.sensors[static_cast<uint8_t>(SensorId::WATER_FLOW)].enabled  = 1;

    // Su seviyesi güvenlik sensörüdür: FİLTRESİZ.
    // Pompa çalışırken seviye hızla düşebilir; filtre gecikmesi kabul edilemez
    // (TASK-023, TASK-026).
    out.sensors[static_cast<uint8_t>(SensorId::WATER_LEVEL)].filterStrength = 0;

    // ── I2C ORTAM SENSÖRLERİ: GERÇEKÇİ GEÇERLİ ARALIK (TASK-066) ────────────
    //
    // Genel varsayılan `{-1000, +1000}` ışık sensörü için SESSİZ BİR ARIZADIR:
    // güneşli bir serada 20 000 lüks olağandır ve her okuma OUT_OF_RANGE
    // damgalanıp kullanılamaz hâle gelirdi. Aralıklar sensörlerin veri sayfası
    // sınırlarından alınmıştır — "makul" değil, ÖLÇÜLEBİLİR sınırlar.
    out.sensors[static_cast<uint8_t>(SensorId::HUMIDITY)].validRange =
        Range<float>{0.0f, 100.0f};                       // bağıl nem, %
    out.sensors[static_cast<uint8_t>(SensorId::AMBIENT_TEMP)].validRange =
        Range<float>{-40.0f, 85.0f};                      // AHT20 çalışma aralığı
    out.sensors[static_cast<uint8_t>(SensorId::LIGHT)].validRange =
        Range<float>{0.0f, 120000.0f};                    // BH1750 üst ölçüm sınırı

    // --- actuators ---
    //
    // VARSAYILANLAR ROLE GÖRE (TASK-066). Tek tip varsayılan bırakmak sessiz
    // bir arıza üretirdi: 5 dakikalık `maxRunMs` ile büyütme ışığı, 16 saatlik
    // ışık penceresi kuralına rağmen her 5 dakikada bir zorla kapanırdı ve
    // kullanıcı nedenini kural ekranında ararken bulamazdı.
    for (uint8_t i = 0; i < MAX_ACTUATORS; ++i)
    {
        out.actuators[i].minRunMs   = 5u * 1000u;        // kısa çevrim koruması
        out.actuators[i].maxRunMs   = 5u * 60u * 1000u;  // KISA varsayılan: 5 dk
        out.actuators[i].cooldownMs = 30u * 1000u;
        out.actuators[i].relayIndex = i;
        out.actuators[i].enabled    = 0;
    }

    // Büyütme ışığı: bir fotoperiyot boyunca kesintisiz yanar.
    {
        ActuatorConfig& a = out.actuators[static_cast<uint8_t>(ActuatorId::GROW_LIGHT)];
        a.minRunMs   = 60u * 1000u;               // röle kısa çevrimini engelle
        a.maxRunMs   = 18u * 60u * 60u * 1000u;   // en uzun fotoperiyot
        a.cooldownMs = 60u * 1000u;
    }

    // Isıtıcı: soğuk bir hazneyi ısıtmak saatler sürebilir, ama sonsuza kadar
    // değil. Kısa çevrim rölenin ve ısıtıcının ömrünü kısaltır.
    {
        ActuatorConfig& a = out.actuators[static_cast<uint8_t>(ActuatorId::HEATER)];
        a.minRunMs   = 60u * 1000u;
        a.maxRunMs   = 4u * 60u * 60u * 1000u;
        a.cooldownMs = 3u * 60u * 1000u;
    }

    // Besin dozaj pompası: saniyeler basar, sonra KARIŞMAYI BEKLER.
    //
    // `cooldownMs` burada bir konfor ayarı değil, AŞIRI GÜBRELEME KORUMASIDIR.
    // Dozajdan sonra EC'nin yükselmesi haznenin karışmasını gerektirir; bekleme
    // olmasaydı kural "EC hâlâ düşük" görüp arka arkaya dozaj yapar ve çözeltiyi
    // bitkiyi yakacak seviyeye çıkarırdı.
    {
        ActuatorConfig& a = out.actuators[static_cast<uint8_t>(ActuatorId::NUTRIENT_PUMP)];
        a.minRunMs   = 2u * 1000u;
        a.maxRunMs   = 60u * 1000u;
        a.cooldownMs = 10u * 60u * 1000u;  // karışma payı
    }

    // Yalnızca su ve hava pompası varsayılan olarak AÇIK. Işık, ısıtıcı ve
    // dozaj pompası kapalı doğar: kullanıcı o röleyi fiilen kablolamış olmak
    // zorunda değildir ve olmayan bir donanımı sürmeye çalışmak, arayüzde
    // "açtım ama bir şey olmadı" olarak görünürdü (P7).
    out.actuators[static_cast<uint8_t>(ActuatorId::WATER_PUMP)].enabled = 1;
    out.actuators[static_cast<uint8_t>(ActuatorId::AIR_PUMP)].enabled   = 1;

    // --- safety: koruyucu tarafta ---
    out.safety.flowVerifyDelayMs    = 10u * 1000u;  // boru dolma payı
    out.safety.flowMinRate          = 0.5f;         // L/dk
    out.safety.maxRuntimeGraceMs    = 5u * 1000u;
    out.safety.maxRuntimeViolations = 3;
    out.safety.requireLevelSensor   = 1;  // sensör yoksa pompa KİLİTLİ (fail-safe)

    // --- automation: MANUAL — kendiliğinden sulama YOK ---
    out.automation.mode             = AutomationMode::MANUAL;
    out.automation.manualOverrideMs = 15u * 60u * 1000u;

    // --- crop: ürün SEÇİLMEMİŞ ---
    //
    // Varsayılan olarak bir ürün seçmek (örneğin marulu "kolay" diye) cihazı
    // kullanıcının yetiştirmediği bir bitkinin parametreleriyle çalıştırırdı.
    // Seçimsiz başlamak, kurulum sihirbazının soracağı ilk soruyu anlamlı
    // kılar ve hiçbir kural üretilmemesini garanti eder.
    out.crop.plantedAtEpoch = 0;
    out.crop.crop           = CropId::NONE;
    out.crop.stage          = GrowthStage::SEEDLING;
    out.crop.autoStage      = 1;  // seçim yapıldığında gün saymaya hazır
    out.crop.intensity      = Intensity::NORMAL;
    out.crop.derivedFrom    = CropId::NONE;

    // --- kurallar: TAMAMEN BOŞ ---
    // İlk açılışta sistem kendiliğinden sulamaya başlamaz (TASK-054 Karar 6).
    out.rules.count = 0;
    for (uint8_t i = 0; i < MAX_RULES; ++i)
    {
        out.rules.rules[i].kind    = RuleKind::INACTIVE;
        out.rules.rules[i].enabled = 0u;
        out.rules.rules[i].target  = ActuatorId::NONE;
    }

    // --- system ---
    out.system.timezone.assign("EET-2EEST,M3.5.0/3,M10.5.0/4");  // TR, DST dahil
    out.system.telemetryIntervalMs = 1000;
    out.system.logLevel            = static_cast<uint8_t>(LogLevel::INFO);
}

namespace cfgvalid {
namespace {

constexpr ConfigError fail(ErrCode c, const char* field)
{
    return ConfigError{c, field};
}

constexpr ErrCode RANGE_ERR = ErrCode::CFG_VALIDATION_FAILED;

} // namespace

ConfigError validateNetwork(const NetworkConfig& c)
{
    if (c.ipMode != IpMode::DHCP && c.ipMode != IpMode::STATIC)
    {
        return fail(RANGE_ERR, "network.ipMode");
    }

    if (c.ipMode == IpMode::STATIC)
    {
        // Static IP seçiliyse adres, ağ geçidi ve maske ZORUNLU. Eksik
        // bırakılırsa cihaz ağa hiç çıkamaz ve nedeni belirsiz kalır.
        if (c.staticIp == 0u)
        {
            return fail(RANGE_ERR, "network.staticIp");
        }
        if (c.gateway == 0u)
        {
            return fail(RANGE_ERR, "network.gateway");
        }
        if (c.subnet == 0u)
        {
            return fail(RANGE_ERR, "network.subnet");
        }
        // DNS boşsa NTP çalışmaz → zaman geçersiz → çizelgeler durur.
        if (c.dns == 0u)
        {
            return fail(RANGE_ERR, "network.dns");
        }
    }
    return configOk();
}

ConfigError validateSensor(const SensorConfig& c, uint8_t index)
{
    if (index >= MAX_SENSORS)
    {
        return fail(RANGE_ERR, "sensors.index");
    }
    if (!c.validRange.valid())
    {
        return fail(RANGE_ERR, "sensors.validRange");
    }
    if (c.scale == 0.0f)
    {
        // Sıfır ölçek her ölçümü sabitler — sensör sessizce ölür.
        return fail(RANGE_ERR, "sensors.scale");
    }
    if (!limits::FILTER_STRENGTH.contains(c.filterStrength))
    {
        return fail(RANGE_ERR, "sensors.filterStrength");
    }
    if (c.maxChangePerSec < 0.0f)
    {
        return fail(RANGE_ERR, "sensors.maxChangePerSec");
    }
    return configOk();
}

ConfigError validateActuator(const ActuatorConfig& c, uint8_t index)
{
    if (index >= MAX_ACTUATORS)
    {
        return fail(RANGE_ERR, "actuators.index");
    }
    if (c.relayIndex >= MAX_ACTUATORS)
    {
        return fail(RANGE_ERR, "actuators.relayIndex");
    }

    // maxRunMs'in ÜST SINIRI olması kritik: sınırsız bırakılırsa
    // "maksimum çalışma süresi" koruması anlamsızlaşır.
    //
    // Sınır AKTÜATÖRÜN ROLÜNE göre seçilir (TASK-066): ışık 20 saate kadar
    // yanabilir, dozaj pompası 5 dakikayı geçemez. Tek bir global sınır
    // ikisinden birini mutlaka yanlış korurdu.
    if (!limits::maxRunLimitFor(index).contains(c.maxRunMs))
    {
        return fail(RANGE_ERR, "actuators.maxRunMs");
    }
    if (!limits::ACTUATOR_MIN_RUN.contains(c.minRunMs))
    {
        return fail(RANGE_ERR, "actuators.minRunMs");
    }
    if (!limits::ACTUATOR_COOLDOWN.contains(c.cooldownMs))
    {
        return fail(RANGE_ERR, "actuators.cooldownMs");
    }

    // ALANLAR ARASI: minRun >= maxRun ise aktüatör ne açılabilir ne kapanabilir.
    if (c.minRunMs >= c.maxRunMs)
    {
        return fail(RANGE_ERR, "actuators.minRunMs");
    }
    return configOk();
}

ConfigError validateSafety(const SafetyConfig& c)
{
    if (!limits::FLOW_VERIFY_DELAY.contains(c.flowVerifyDelayMs))
    {
        return fail(RANGE_ERR, "safety.flowVerifyDelayMs");
    }
    if (!limits::FLOW_MIN_RATE.contains(c.flowMinRate))
    {
        return fail(RANGE_ERR, "safety.flowMinRate");
    }
    if (!limits::MAX_RUNTIME_GRACE.contains(c.maxRuntimeGraceMs))
    {
        return fail(RANGE_ERR, "safety.maxRuntimeGraceMs");
    }
    if (c.maxRuntimeViolations == 0u || c.maxRuntimeViolations > 20u)
    {
        return fail(RANGE_ERR, "safety.maxRuntimeViolations");
    }
    return configOk();
}

ConfigError validateAutomation(const AutomationConfig& c)
{
    if (c.mode != AutomationMode::MANUAL && c.mode != AutomationMode::AUTO)
    {
        return fail(RANGE_ERR, "automation.mode");
    }
    if (!limits::MANUAL_OVERRIDE.contains(c.manualOverrideMs))
    {
        return fail(RANGE_ERR, "automation.manualOverrideMs");
    }
    return configOk();
}

ConfigError validateRule(const Rule& r, uint8_t index, const SensorConfig* sensors)
{
    (void)index;

    if (r.kind == RuleKind::INACTIVE) { return configOk(); }

    if (static_cast<uint8_t>(r.target) >= MAX_ACTUATORS)
    {
        return fail(RANGE_ERR, "rules.target");
    }

    if (r.kind == RuleKind::THRESHOLD)
    {
        const uint8_t si = static_cast<uint8_t>(r.sensor);
        if (si >= MAX_SENSORS) { return fail(RANGE_ERR, "rules.sensor"); }

        // EŞİKLER EŞİT OLAMAZ: eşitlik histerezis bandının sıfır olması,
        // yani rölenin gürültüyle çırpınması demektir.
        if (r.onThreshold == r.offThreshold)
        {
            return fail(RANGE_ERR, "rules.threshold");
        }

        // Eşikler sensörün geçerli aralığında olmalı. Aralık dışı bir eşik
        // ASLA tetiklenmez ve kullanıcı kuralın neden çalışmadığını anlayamaz.
        if (sensors != nullptr)
        {
            const Range<float>& vr = sensors[si].validRange;
            if (!vr.contains(r.onThreshold))  { return fail(RANGE_ERR, "rules.onThreshold"); }
            if (!vr.contains(r.offThreshold)) { return fail(RANGE_ERR, "rules.offThreshold"); }
        }
    }

    if (r.kind == RuleKind::SCHEDULE_WINDOW)
    {
        // Gün içi dakika: 0–1439. Gece yarısını aşan pencere (22:00–02:00)
        // GEÇERLİDİR — start > end bir hata değil, sarma penceredir.
        if (r.startMin > 1439u || r.endMin > 1439u)
        {
            return fail(RANGE_ERR, "rules.window");
        }
        if (r.startMin == r.endMin) { return fail(RANGE_ERR, "rules.window"); }
    }

    if (r.kind == RuleKind::SCHEDULE_CYCLE)
    {
        if (r.cyclePeriodS == 0u) { return fail(RANGE_ERR, "rules.cyclePeriodS"); }
        // Açık süre periyottan KISA olmalı; eşit veya uzun olması
        // "hiç kapanmayan çevrim" demektir ve `maxRunMs` ile çakışır.
        if (r.cycleOnS == 0u || r.cycleOnS >= r.cyclePeriodS)
        {
            return fail(RANGE_ERR, "rules.cycleOnS");
        }
    }

    return configOk();
}

ConfigError validateRules(const RuleSet& rs, const SensorConfig* sensors)
{
    if (rs.count > MAX_RULES) { return fail(RANGE_ERR, "rules.count"); }

    for (uint8_t i = 0; i < rs.count; ++i)
    {
        const ConfigError e = validateRule(rs.rules[i], i, sensors);
        if (!e.ok()) { return e; }
    }

    // Aynı aktüatörü hedefleyen EŞİT ÖNCELİKLİ etkin kurallar: hangisinin
    // kazanacağı belirsizdir. Belirsizlik bırakmıyoruz (TASK-054 kabul
    // kriteri: "çakışan kurallar tespit edilmeli").
    for (uint8_t i = 0; i < rs.count; ++i)
    {
        if (rs.rules[i].enabled == 0u || rs.rules[i].kind == RuleKind::INACTIVE) { continue; }
        for (uint8_t j = static_cast<uint8_t>(i + 1u); j < rs.count; ++j)
        {
            if (rs.rules[j].enabled == 0u || rs.rules[j].kind == RuleKind::INACTIVE)
            {
                continue;
            }
            if (rs.rules[i].target == rs.rules[j].target &&
                rs.rules[i].priority == rs.rules[j].priority)
            {
                return fail(RANGE_ERR, "rules.priority");
            }
        }
    }

    return configOk();
}

ConfigError validateCrop(const CropConfig& c)
{
    if (c.intensity != Intensity::SPARSE && c.intensity != Intensity::NORMAL &&
        c.intensity != Intensity::ABUNDANT)
    {
        return fail(RANGE_ERR, "crop.intensity");
    }

    if (c.plantedAtEpoch < 0)
    {
        // Negatif epoch = 1970 öncesi. Gün sayımı taşar ve dönem hesabı
        // anlamsızlaşır; sessizce kabul etmek yerine reddediyoruz.
        return fail(RANGE_ERR, "crop.plantedAtEpoch");
    }

    // Ürün seçilmemiş veya kullanıcı elle düzenlemiş: dönem ve dikim tarihi
    // hiçbir profile karşılık gelmez, denetlenecek bir şey yoktur.
    if (c.crop == CropId::NONE || c.crop == CropId::CUSTOM)
    {
        return configOk();
    }

    const CropProfile* p = cropById(c.crop);
    if (p == nullptr)
    {
        // Katalogda olmayan bir kimlik: firmware geri alınmış ve o ürün
        // kaldırılmış olabilir. Sessizce başka bir ürüne düşmek yanlış
        // parametrelerle sulama demektir.
        return fail(RANGE_ERR, "crop.crop");
    }

    // Yapraklı bir üründe "meyve dönemi" seçili olamaz: o dönemin tablosu
    // yoktur ve `buildCropRules` son geçerli döneme düşerdi — kullanıcı
    // ekranda "Meyve" görürken kurallar "Gelişme" olurdu.
    if (!stageValidFor(*p, c.stage))
    {
        return fail(RANGE_ERR, "crop.stage");
    }

    return configOk();
}

ConfigError validateSystem(const SystemConfig& c)
{
    if (!limits::TELEMETRY_INTERVAL.contains(c.telemetryIntervalMs))
    {
        return fail(RANGE_ERR, "system.telemetryIntervalMs");
    }
    if (c.logLevel > static_cast<uint8_t>(LogLevel::CRITICAL))
    {
        return fail(RANGE_ERR, "system.logLevel");
    }
    if (c.timezone.empty())
    {
        // Boş TZ → yerel saat UTC olur, çizelgeler yanlış saatte tetiklenir.
        return fail(RANGE_ERR, "system.timezone");
    }
    return configOk();
}

ConfigError validateAll(const Config& c)
{
    if (c.magic != CONFIG_MAGIC)
    {
        return fail(ErrCode::CFG_CORRUPT, "magic");
    }
    if (c.schemaVersion == 0u || c.schemaVersion > CONFIG_SCHEMA_VERSION)
    {
        return fail(ErrCode::CFG_VERSION_NEWER, "schemaVersion");
    }

    ConfigError e = validateNetwork(c.network);
    if (!e.ok())
    {
        return e;
    }

    for (uint8_t i = 0; i < MAX_SENSORS; ++i)
    {
        e = validateSensor(c.sensors[i], i);
        if (!e.ok())
        {
            return e;
        }
    }

    for (uint8_t i = 0; i < MAX_ACTUATORS; ++i)
    {
        e = validateActuator(c.actuators[i], i);
        if (!e.ok())
        {
            return e;
        }
    }

    e = validateSafety(c.safety);
    if (!e.ok())
    {
        return e;
    }
    e = validateRules(c.rules, c.sensors);
    if (!e.ok()) { return e; }

    e = validateAutomation(c.automation);
    if (!e.ok())
    {
        return e;
    }
    e = validateCrop(c.crop);
    if (!e.ok())
    {
        return e;
    }
    e = validateSystem(c.system);
    if (!e.ok())
    {
        return e;
    }

    // ALANLAR ARASI: seviye sensörü zorunlu tutulmuş ama sensör devre dışı
    // bırakılmışsa, koruma sessizce kapalı kalırdı.
    if (c.safety.requireLevelSensor != 0u &&
        c.sensors[static_cast<uint8_t>(SensorId::WATER_LEVEL)].enabled == 0u)
    {
        return fail(RANGE_ERR, "sensors.waterLevel.enabled");
    }

    return configOk();
}

} // namespace cfgvalid
} // namespace core
