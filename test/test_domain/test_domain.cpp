// Domain katmanı host testleri — TASK-064
//
// ── NEDEN HOST TESTİ MÜMKÜN ─────────────────────────────────────────────────
// ARCHITECTURE §17 gereği `domain/` yalnızca `core/` tiplerine bağımlıdır ve
// donanım çağrısı içermez. Bu, mimarinin bilinçli bir çıktısıdır — sonradan
// eklenen bir test kolaylığı değil.
//
// Doğrulandı: `core/` ve `domain/` başlıklarının HİÇBİRİ `<Arduino.h>`,
// `<freertos/*>` veya `<esp_*>` include etmiyor.
//
// ── ÖNCELİK: GERÇEKTE TEST EDİLEMEYENLER ────────────────────────────────────
// Bu testler donanımda test edilmesi ya İMKÂNSIZ ya da PRATİK OLMAYAN
// şeyleri hedefler:
//
//   `millis()` taşması       → gerçekte 49 GÜN sürer
//   Çizelge sarma penceresi  → gerçekte gece yarısını beklemek gerekir
//   Histerezis gürültüsü     → sentetik veri olmadan tekrarlanamaz
//   Backoff eğrisi           → gerçekte dakikalar sürer
//   Config kombinasyonları   → yüzlerce geçersiz kombinasyon
//   Güvenlik kilidi matrisi  → tüm kombinasyonlar donanımda üretilemez

#include <unity.h>

#include "core/Config.h"
#include "core/ConfigValidation.h"
#include "core/CropProfile.h"
#include "core/EncoderDecode.h"
#include "services/HistoryStore.h"
#include "core/Rule.h"
#include "core/SystemState.h"
#include "core/Time.h"
#include "domain/RuleEvaluator.h"
#include "domain/models/Actuator.h"
#include "domain/models/SafetyState.h"
#include "services/network/IpConfig.h"
#include "services/network/RetryPolicy.h"

using namespace core;
using namespace core::cfgvalid;   // validateAll, validateRule, loadDefaults …

// ═══════════════════════════════════════════════════════════════════════════
// 1. ZAMAN TAŞMASI — gerçekte 49 gün sürer
// ═══════════════════════════════════════════════════════════════════════════

void test_elapsed_wraps_correctly()
{
    // `millis()` 0xFFFFFFFF'ten 0'a sarar. Naif çıkarma dev bir sayı verir.
    TEST_ASSERT_EQUAL_UINT32(10u, elapsed(Millis{5}, Millis{0xFFFFFFFBu}).ms);
    TEST_ASSERT_EQUAL_UINT32(1u, elapsed(Millis{0}, Millis{0xFFFFFFFFu}).ms);
    TEST_ASSERT_EQUAL_UINT32(0u, elapsed(Millis{100}, Millis{100}).ms);
}

void test_hasElapsed_across_wrap()
{
    // Sarma anında bir bekleme SESSİZCE bozulmamalı.
    TEST_ASSERT_TRUE(hasElapsed(Millis{5}, Millis{0xFFFFFFFBu}, millisecs(10)));
    TEST_ASSERT_FALSE(hasElapsed(Millis{5}, Millis{0xFFFFFFFBu}, millisecs(11)));
}

void test_hasElapsed_zero_duration_is_always_true()
{
    // ISSUE-012'nin kökü: `hasElapsed(..., 0)` HER ZAMAN true'dur.
    // Bu davranış DOĞRU; hata onu "son tarih geldi mi" sanmaktı.
    // Test bu tuzağı belgelemek için var.
    TEST_ASSERT_TRUE(hasElapsed(Millis{0}, Millis{0}, Duration{0}));
    TEST_ASSERT_TRUE(hasElapsed(Millis{0}, Millis{5000}, Duration{0}));
}

// ═══════════════════════════════════════════════════════════════════════════
// 2. AKTÜATÖR KISITLARI — sınır değerlerde
// ═══════════════════════════════════════════════════════════════════════════

static ActuatorConfig makeActCfg()
{
    ActuatorConfig c{};
    c.minRunMs   = 5000;
    c.maxRunMs   = 300000;
    c.cooldownMs = 30000;
    c.enabled    = 1;
    return c;
}

void test_cooldown_blocks_then_allows()
{
    const ActuatorConfig cfg = makeActCfg();
    domain::ActuatorRuntime rt{};
    rt.reset();
    rt.everRan  = 1;
    rt.lastOffAt = Millis{1000};

    // Cooldown içinde → engelli
    TEST_ASSERT_FALSE(domain::actuator::canTurnOn(cfg, rt, Millis{20000}).allowed);
    // Tam sınırda → izinli (>= semantiği)
    TEST_ASSERT_TRUE(domain::actuator::canTurnOn(cfg, rt, Millis{31000}).allowed);
}

void test_cooldown_not_applied_on_first_run()
{
    // `everRan == 0` iken `lastOffAt` anlamsızdır; sıfır kabul edilirse
    // cihaz boot'tan sonra cooldown kadar gereksiz beklerdi.
    const ActuatorConfig cfg = makeActCfg();
    domain::ActuatorRuntime rt{};
    rt.reset();   // everRan = 0
    TEST_ASSERT_TRUE(domain::actuator::canTurnOn(cfg, rt, Millis{0}).allowed);
}

void test_min_runtime_defers_off()
{
    const ActuatorConfig cfg = makeActCfg();
    domain::ActuatorRuntime rt{};
    rt.reset();
    rt.isOn     = 1;
    rt.lastOnAt = Millis{1000};

    const domain::ConstraintVerdict early =
        domain::actuator::canTurnOff(cfg, rt, Millis{3000});
    TEST_ASSERT_FALSE(early.allowed);
    TEST_ASSERT_EQUAL(CommandResult::DEFERRED_MIN_RUNTIME, early.result);

    TEST_ASSERT_TRUE(domain::actuator::canTurnOff(cfg, rt, Millis{6000}).allowed);
}

void test_max_runtime_exceeded()
{
    const ActuatorConfig cfg = makeActCfg();
    domain::ActuatorRuntime rt{};
    rt.reset();
    rt.isOn     = 1;
    rt.lastOnAt = Millis{0};

    TEST_ASSERT_FALSE(domain::actuator::maxRunExceeded(cfg, rt, Millis{299000}, 0));
    TEST_ASSERT_TRUE(domain::actuator::maxRunExceeded(cfg, rt, Millis{301000}, 0));
    // Pay (grace) uygulanınca eşik kayar
    TEST_ASSERT_FALSE(domain::actuator::maxRunExceeded(cfg, rt, Millis{301000}, 5000));
}

void test_source_arbitration()
{
    using domain::actuator::sourceOutranks;
    TEST_ASSERT_TRUE(sourceOutranks(ControlSource::SAFETY, ControlSource::MANUAL));
    TEST_ASSERT_TRUE(sourceOutranks(ControlSource::MANUAL, ControlSource::AUTOMATION));
    // Otomasyon operatörü EZEMEZ.
    TEST_ASSERT_FALSE(sourceOutranks(ControlSource::AUTOMATION, ControlSource::MANUAL));
}

// ═══════════════════════════════════════════════════════════════════════════
// 3. GÜVENLİK KİLİDİ MATRİSİ — tüm kombinasyonlar donanımda üretilemez
// ═══════════════════════════════════════════════════════════════════════════

void test_water_pump_blocked_by_every_interlock()
{
    using namespace domain::safety;
    const uint32_t m = masksFor(ActuatorId::WATER_PUMP);
    TEST_ASSERT_TRUE((m & ILK_EMERGENCY_LATCHED) != 0u);
    TEST_ASSERT_TRUE((m & ILK_LEVEL_INSUFFICIENT) != 0u);
    TEST_ASSERT_TRUE((m & ILK_LEVEL_SENSOR_FAULT) != 0u);
    TEST_ASSERT_TRUE((m & ILK_DRY_RUN) != 0u);
}

void test_air_pump_not_blocked_by_level()
{
    // Hava taşı susuz kalınca hasar görmez; gereksiz kilit operatörü
    // güvenlik uyarılarına duyarsızlaştırır.
    using namespace domain::safety;
    const uint32_t m = masksFor(ActuatorId::AIR_PUMP);
    TEST_ASSERT_TRUE((m & ILK_EMERGENCY_LATCHED) != 0u);
    TEST_ASSERT_FALSE((m & ILK_LEVEL_INSUFFICIENT) != 0u);
}

void test_emergency_has_highest_reporting_priority()
{
    using namespace domain::safety;
    TEST_ASSERT_EQUAL(ErrCode::SAFETY_EMERGENCY_LATCHED,
                      firstReason(ILK_EMERGENCY_LATCHED | ILK_LEVEL_INSUFFICIENT));
    // Sensör arızası, "seviye yetersiz"ten önce raporlanır: okunamayan bir
    // sensör daha ciddi bir durumdur.
    TEST_ASSERT_EQUAL(ErrCode::SAFETY_LEVEL_SENSOR_FAULT,
                      firstReason(ILK_LEVEL_SENSOR_FAULT | ILK_LEVEL_INSUFFICIENT));
    TEST_ASSERT_EQUAL(ErrCode::OK, firstReason(ILK_NONE));
}

// ═══════════════════════════════════════════════════════════════════════════
// 4. BACKOFF — gerçekte dakikalar sürer
// ═══════════════════════════════════════════════════════════════════════════

void test_backoff_curve_and_cap()
{
    using namespace services::net::retry;
    TEST_ASSERT_EQUAL_UINT32(1000u, baseDelayMs(0));
    TEST_ASSERT_EQUAL_UINT32(2000u, baseDelayMs(1));
    TEST_ASSERT_EQUAL_UINT32(32000u, baseDelayMs(5));
    TEST_ASSERT_EQUAL_UINT32(MAX_DELAY_MS, baseDelayMs(6));
    // Büyük deneme sayısında TAŞMA YOK
    TEST_ASSERT_EQUAL_UINT32(MAX_DELAY_MS, baseDelayMs(200));
}

void test_jitter_stays_within_bounds()
{
    using namespace services::net::retry;
    // ±%20, tavanı ASLA aşmaz
    TEST_ASSERT_EQUAL_UINT32(8000u, applyJitter(10000u, 0u));
    TEST_ASSERT_EQUAL_UINT32(12000u, applyJitter(10000u, 255u));
    TEST_ASSERT_EQUAL_UINT32(MAX_DELAY_MS, applyJitter(MAX_DELAY_MS, 255u));

    for (uint16_t r = 0; r <= 255u; ++r)
    {
        const uint32_t d = applyJitter(10000u, static_cast<uint8_t>(r));
        TEST_ASSERT_TRUE(d >= 8000u && d <= 12000u);
    }
}

void test_auth_failure_stops_but_ap_missing_never_does()
{
    using namespace services::net;
    TEST_ASSERT_FALSE(retry::shouldStop(DisconnectClass::AUTH_FAILED, 2));
    TEST_ASSERT_TRUE(retry::shouldStop(DisconnectClass::AUTH_FAILED, 3));
    // AP geri gelebilir — ASLA durmaz.
    TEST_ASSERT_FALSE(retry::shouldStop(DisconnectClass::AP_NOT_FOUND, 200));
}

void test_link_lost_first_retry_is_fast_but_auth_never_is()
{
    using namespace services::net;
    TEST_ASSERT_EQUAL_UINT32(retry::FAST_RETRY_MS,
                             retry::delayFor(DisconnectClass::LINK_LOST, 0, 128));
    // Kimlik hatasında hızlı deneme SONSUZ DÖNGÜ üretirdi.
    TEST_ASSERT_NOT_EQUAL(retry::FAST_RETRY_MS,
                          retry::delayFor(DisconnectClass::AUTH_FAILED, 0, 128));
}

void test_disconnect_classification()
{
    using namespace services::net;
    TEST_ASSERT_EQUAL(DisconnectClass::AUTH_FAILED, classify(202));   // AUTH_FAIL
    TEST_ASSERT_EQUAL(DisconnectClass::AUTH_FAILED, classify(15));    // 4WAY timeout
    TEST_ASSERT_EQUAL(DisconnectClass::AP_NOT_FOUND, classify(201));
    TEST_ASSERT_EQUAL(DisconnectClass::LINK_LOST, classify(200));     // BEACON timeout
    TEST_ASSERT_EQUAL(DisconnectClass::UNKNOWN, classify(9999));
}

// ═══════════════════════════════════════════════════════════════════════════
// 5. ÇİZELGE — gece yarısı sarması gerçekte beklemek gerektirir
// ═══════════════════════════════════════════════════════════════════════════

void test_normal_window()
{
    using domain::rules::inWindow;
    TEST_ASSERT_TRUE(inWindow(600, 480, 720));    // 10:00 ∈ [08:00,12:00)
    TEST_ASSERT_FALSE(inWindow(780, 480, 720));   // 13:00 ∉
    TEST_ASSERT_TRUE(inWindow(480, 480, 720));    // başlangıç DAHİL
    TEST_ASSERT_FALSE(inWindow(720, 480, 720));   // bitiş HARİÇ
}

void test_wrapping_window_over_midnight()
{
    // Çizelge mantığındaki EN YAYGIN hata kaynağı.
    using domain::rules::inWindow;
    TEST_ASSERT_TRUE(inWindow(1380, 1320, 120));  // 23:00 ∈ [22:00,02:00)
    TEST_ASSERT_TRUE(inWindow(0, 1320, 120));     // 00:00 ∈
    TEST_ASSERT_TRUE(inWindow(60, 1320, 120));    // 01:00 ∈
    TEST_ASSERT_FALSE(inWindow(720, 1320, 120));  // 12:00 ∉
    TEST_ASSERT_FALSE(inWindow(120, 1320, 120));  // 02:00 bitiş HARİÇ
}

void test_periodic_cycle()
{
    // "Her 2 saatte 15 dk sula"
    using domain::rules::inCycle;
    TEST_ASSERT_TRUE(inCycle(0, 900, 7200));
    TEST_ASSERT_TRUE(inCycle(899, 900, 7200));
    TEST_ASSERT_FALSE(inCycle(900, 900, 7200));
    TEST_ASSERT_FALSE(inCycle(7199, 900, 7200));
    TEST_ASSERT_TRUE(inCycle(7200, 900, 7200));   // sonraki çevrim
    // Periyot 0 → SIFIRA BÖLME YOK, asla açılmaz
    TEST_ASSERT_FALSE(inCycle(0, 900, 0));
}

// ═══════════════════════════════════════════════════════════════════════════
// 6. IP PLANI — eksik alan kombinasyonları
// ═══════════════════════════════════════════════════════════════════════════

void test_dhcp_plan()
{
    NetworkConfig n{};
    n.ipMode = IpMode::DHCP;
    const services::net::IpPlan p = services::net::planFor(n);
    TEST_ASSERT_FALSE(p.useStatic);
    TEST_ASSERT_EQUAL(ErrCode::OK, p.warning);
}

void test_static_plan_falls_back_when_incomplete()
{
    NetworkConfig n{};
    n.ipMode   = IpMode::STATIC;
    n.staticIp = 0x0101A8C0u;
    // gateway ve subnet EKSİK
    const services::net::IpPlan p = services::net::planFor(n);
    TEST_ASSERT_FALSE(p.useStatic);
    // Sessizce düşülmez — uyarı taşınır.
    TEST_ASSERT_EQUAL(ErrCode::NET_IP_CONFIG_INVALID, p.warning);
}

void test_static_plan_uses_gateway_when_dns_missing()
{
    // DNS'siz statik yapılandırmada SNTP hiç çalışmazdı.
    NetworkConfig n{};
    n.ipMode   = IpMode::STATIC;
    n.staticIp = 0x0101A8C0u;
    n.gateway  = 0xFE01A8C0u;
    n.subnet   = 0x00FFFFFFu;
    n.dns      = 0;
    const services::net::IpPlan p = services::net::planFor(n);
    TEST_ASSERT_TRUE(p.useStatic);
    TEST_ASSERT_EQUAL_UINT32(n.gateway, p.dns);
}

// ═══════════════════════════════════════════════════════════════════════════
// 7. CONFIG DOĞRULAMA — yüzlerce geçersiz kombinasyon
// ═══════════════════════════════════════════════════════════════════════════

void test_defaults_pass_their_own_validation()
{
    // Varsayılanların kendi doğrulamasından geçmesi ZORUNLUDUR; geçmezse
    // sistem her boot'ta geçersiz config'e düşer (TASK-014'te bulunan hata).
    Config c{};
    loadDefaults(c);
    const ConfigError e = validateAll(c);
    TEST_ASSERT_EQUAL_STRING(e.ok() ? "" : e.field, e.ok() ? "" : e.field);
    TEST_ASSERT_TRUE(e.ok());
}

void test_defaults_are_safe()
{
    Config c{};
    loadDefaults(c);
    // Kutudan çıktığında sistem KENDİLİĞİNDEN hiçbir şey yapmamalı.
    TEST_ASSERT_EQUAL(AutomationMode::MANUAL, c.automation.mode);
    TEST_ASSERT_EQUAL_UINT8(0, c.rules.count);
    TEST_ASSERT_EQUAL_UINT8(1, c.safety.requireLevelSensor);
}

void test_actuator_min_must_be_less_than_max()
{
    Config c{};
    loadDefaults(c);
    c.actuators[0].minRunMs = c.actuators[0].maxRunMs;
    TEST_ASSERT_FALSE(validateActuator(c.actuators[0], 0).ok());
}

void test_rule_thresholds_cannot_be_equal()
{
    // Eşit eşik = sıfır histerezis = röle çırpınması.
    Config c{};
    loadDefaults(c);
    Rule r{};
    r.kind         = RuleKind::THRESHOLD;
    r.target       = ActuatorId::WATER_PUMP;
    r.enabled      = 1;
    r.sensor       = SensorId::EC;
    r.onThreshold  = 1.0f;
    r.offThreshold = 1.0f;
    TEST_ASSERT_FALSE(validateRule(r, 0, c.sensors).ok());

    r.offThreshold = 1.2f;
    TEST_ASSERT_TRUE(validateRule(r, 0, c.sensors).ok());
}

void test_cycle_on_time_must_be_shorter_than_period()
{
    Config c{};
    loadDefaults(c);
    Rule r{};
    r.kind         = RuleKind::SCHEDULE_CYCLE;
    r.target       = ActuatorId::WATER_PUMP;
    r.enabled      = 1;
    r.cyclePeriodS = 600;
    r.cycleOnS     = 600;   // "hiç kapanmayan çevrim"
    TEST_ASSERT_FALSE(validateRule(r, 0, c.sensors).ok());

    r.cycleOnS = 60;
    TEST_ASSERT_TRUE(validateRule(r, 0, c.sensors).ok());
}

void test_conflicting_rules_with_equal_priority_rejected()
{
    Config c{};
    loadDefaults(c);
    for (uint8_t i = 0; i < 2; ++i)
    {
        c.rules.rules[i].kind         = RuleKind::THRESHOLD;
        c.rules.rules[i].target       = ActuatorId::WATER_PUMP;
        c.rules.rules[i].enabled      = 1;
        c.rules.rules[i].priority     = 5;
        c.rules.rules[i].sensor       = SensorId::EC;
        c.rules.rules[i].onThreshold  = 1.0f;
        c.rules.rules[i].offThreshold = 1.2f;
    }
    c.rules.count = 2;
    // Aynı hedef + aynı öncelik → belirsizlik → REDDEDİLİR
    TEST_ASSERT_FALSE(validateRules(c.rules, c.sensors).ok());

    c.rules.rules[1].priority = 6;
    TEST_ASSERT_TRUE(validateRules(c.rules, c.sensors).ok());
}

// ═══════════════════════════════════════════════════════════════════════════
// 8. ÜRÜN PROFİLLERİ — TASK-067
//
// Bu bölüm donanımda test EDİLEMEZ: her ürünün her döneminin geçerli bir kural
// kümesi üretip üretmediğini görmek için 6 ürün × 4 dönem × 3 yoğunluk = 72
// kombinasyonun her birini gerçek bir serada denemek gerekirdi.
// ═══════════════════════════════════════════════════════════════════════════

/// Tüm röleler ve sensörler bağlı — profilin üretebileceği azami kural kümesi.
static void allHardwarePresent(bool (&acts)[MAX_ACTUATORS], bool (&sens)[MAX_SENSORS])
{
    for (uint8_t i = 0; i < MAX_ACTUATORS; ++i) { acts[i] = true; }
    for (uint8_t i = 0; i < MAX_SENSORS; ++i)   { sens[i] = true; }
}

void test_every_crop_and_stage_produces_valid_rules()
{
    // ASIL İDDİA: katalogdaki hiçbir satır, doğrulamadan geçemeyecek bir kural
    // üretemez. Üretse, kullanıcı "çilek profili uygulanamadı" hatası alır ve
    // hatanın kaynağı bir tabloda gömülü kalırdı.
    Config c{};
    loadDefaults(c);

    bool acts[MAX_ACTUATORS];
    bool sens[MAX_SENSORS];
    allHardwarePresent(acts, sens);

    const Intensity levels[3] = {Intensity::SPARSE, Intensity::NORMAL, Intensity::ABUNDANT};

    for (uint8_t ci = 0; ci < cropCount(); ++ci)
    {
        const CropProfile* p = cropAt(ci);
        TEST_ASSERT_NOT_NULL(p);

        for (uint8_t si = 0; si < p->stageCount; ++si)
        {
            for (uint8_t li = 0; li < 3u; ++li)
            {
                RuleSet rs{};
                const uint8_t n = buildCropRules(*p, static_cast<GrowthStage>(si),
                                                 levels[li], acts, sens, rs);

                TEST_ASSERT_TRUE(n > 0u);
                TEST_ASSERT_TRUE(n <= MAX_RULES);
                TEST_ASSERT_EQUAL_UINT8(n, rs.count);

                // Varsayılan sensör aralıkları içinde kalmalı ve çakışmamalı.
                TEST_ASSERT_TRUE(validateRules(rs, c.sensors).ok());
            }
        }
    }
}

void test_generated_cycle_never_exceeds_pump_max_runtime()
{
    // Üretilen çevrim su pompasının varsayılan `maxRunMs`'ini aşarsa,
    // `ActuatorManager` pompayı HER çevrimde süre aşımıyla zorla kapatır ve
    // kullanıcı doğru görünen bir kuralın neden kesildiğini bulamaz.
    Config c{};
    loadDefaults(c);
    const uint32_t pumpMaxRunS =
        c.actuators[static_cast<uint8_t>(ActuatorId::WATER_PUMP)].maxRunMs / 1000u;

    bool acts[MAX_ACTUATORS];
    bool sens[MAX_SENSORS];
    allHardwarePresent(acts, sens);

    for (uint8_t ci = 0; ci < cropCount(); ++ci)
    {
        const CropProfile* p = cropAt(ci);
        for (uint8_t si = 0; si < p->stageCount; ++si)
        {
            RuleSet rs{};
            // En uzun çevrimi üreten yoğunluk: BOL.
            buildCropRules(*p, static_cast<GrowthStage>(si), Intensity::ABUNDANT, acts,
                           sens, rs);

            for (uint8_t r = 0; r < rs.count; ++r)
            {
                if (rs.rules[r].kind != RuleKind::SCHEDULE_CYCLE) { continue; }
                TEST_ASSERT_TRUE(rs.rules[r].cycleOnS < pumpMaxRunS);
                TEST_ASSERT_TRUE(rs.rules[r].cycleOnS < rs.rules[r].cyclePeriodS);
            }
        }
    }
}

void test_missing_hardware_produces_no_rule_for_it()
{
    // Bağlı olmayan bir ısıtıcıya kural yazmak, kural listesinde hiçbir şey
    // yapmayan bir satır bırakmaktı.
    const CropProfile* p = cropById(CropId::STRAWBERRY);
    TEST_ASSERT_NOT_NULL(p);

    bool acts[MAX_ACTUATORS];
    bool sens[MAX_SENSORS];
    allHardwarePresent(acts, sens);

    // Isıtıcı rölesi yok → ısıtma kuralı da yok.
    acts[static_cast<uint8_t>(ActuatorId::HEATER)] = false;

    RuleSet rs{};
    buildCropRules(*p, GrowthStage::FRUITING, Intensity::NORMAL, acts, sens, rs);
    for (uint8_t i = 0; i < rs.count; ++i)
    {
        TEST_ASSERT_TRUE(rs.rules[i].target != ActuatorId::HEATER);
    }

    // Röle var ama SICAKLIK SENSÖRÜ yok → kural yine üretilmemeli; ölçüm
    // olmadan eşik kuralı asla tetiklenmez ve sessizce ölü durur.
    allHardwarePresent(acts, sens);
    sens[static_cast<uint8_t>(SensorId::WATER_TEMP)] = false;

    RuleSet rs2{};
    buildCropRules(*p, GrowthStage::FRUITING, Intensity::NORMAL, acts, sens, rs2);
    for (uint8_t i = 0; i < rs2.count; ++i)
    {
        TEST_ASSERT_TRUE(rs2.rules[i].target != ActuatorId::HEATER);
    }
}

void test_heater_and_dosing_thresholds_point_the_right_way()
{
    // Yön iki eşikten TÜRER (Rule.h): `on < off` = "altına düşünce AÇ".
    // Ters kurulmuş bir ısıtıcı, hazne soğudukça ısıtmayı KAPATIRDI.
    const CropProfile* p = cropById(CropId::STRAWBERRY);
    bool acts[MAX_ACTUATORS];
    bool sens[MAX_SENSORS];
    allHardwarePresent(acts, sens);

    RuleSet rs{};
    buildCropRules(*p, GrowthStage::FRUITING, Intensity::NORMAL, acts, sens, rs);

    bool sawHeater = false;
    bool sawDosing = false;
    for (uint8_t i = 0; i < rs.count; ++i)
    {
        const Rule& r = rs.rules[i];
        if (r.target == ActuatorId::HEATER)
        {
            sawHeater = true;
            TEST_ASSERT_EQUAL(RuleKind::THRESHOLD, r.kind);
            TEST_ASSERT_TRUE(r.sensor == SensorId::WATER_TEMP);
            TEST_ASSERT_TRUE(r.onThreshold < r.offThreshold);  // soğuyunca AÇ
        }
        if (r.target == ActuatorId::NUTRIENT_PUMP)
        {
            sawDosing = true;
            TEST_ASSERT_EQUAL(RuleKind::THRESHOLD, r.kind);
            TEST_ASSERT_TRUE(r.sensor == SensorId::EC);
            TEST_ASSERT_TRUE(r.onThreshold < r.offThreshold);  // EC düşünce AÇ
            // Dozaj arka arkaya tetiklenmemeli: karışma zaman alır.
            TEST_ASSERT_TRUE(r.minTriggerIntervalS >= 60u);
        }
    }
    TEST_ASSERT_TRUE(sawHeater);
    TEST_ASSERT_TRUE(sawDosing);
}

void test_ec_rises_across_fruiting_stages()
{
    // Bu tablonun VARLIK NEDENİ: çileği domates EC'sinde çalıştırmak meyve
    // tutumunu öldürür, aynı ürün içinde de fide meyveyle aynı çözeltiyi
    // kaldıramaz. Dönem ilerledikçe EC hedefi artmalı.
    const CropId fruiting[4] = {CropId::STRAWBERRY, CropId::TOMATO, CropId::PEPPER,
                                CropId::CUCUMBER};
    for (uint8_t i = 0; i < 4u; ++i)
    {
        const CropProfile* p = cropById(fruiting[i]);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_UINT8(4u, p->stageCount);

        for (uint8_t s = 1; s < p->stageCount; ++s)
        {
            TEST_ASSERT_TRUE(p->stages[s].ec.min >= p->stages[s - 1].ec.min);
            TEST_ASSERT_TRUE(p->stages[s].ec.valid());
        }
    }
}

void test_leafy_crops_reject_fruiting_stage()
{
    // Marulda "meyve dönemi" yoktur. Kabul edilseydi ekranda "Meyve" yazarken
    // kurallar "Gelişme" olurdu — arayüz yalan söylerdi.
    Config c{};
    loadDefaults(c);
    c.crop.crop  = CropId::LETTUCE;
    c.crop.stage = GrowthStage::FRUITING;
    TEST_ASSERT_FALSE(validateCrop(c.crop).ok());

    c.crop.stage = GrowthStage::VEGETATIVE;
    TEST_ASSERT_TRUE(validateCrop(c.crop).ok());

    // Meyveli üründe aynı dönem geçerli.
    c.crop.crop  = CropId::TOMATO;
    c.crop.stage = GrowthStage::FRUITING;
    TEST_ASSERT_TRUE(validateCrop(c.crop).ok());
}

void test_stage_advances_by_day_and_never_goes_backwards()
{
    const CropProfile* p = cropById(CropId::STRAWBERRY);
    // fide 21 · gelişme 30 · çiçek 21 · meyve süresiz
    TEST_ASSERT_EQUAL(GrowthStage::SEEDLING,   stageForDay(*p, 0));
    TEST_ASSERT_EQUAL(GrowthStage::SEEDLING,   stageForDay(*p, 20));
    TEST_ASSERT_EQUAL(GrowthStage::VEGETATIVE, stageForDay(*p, 21));
    TEST_ASSERT_EQUAL(GrowthStage::VEGETATIVE, stageForDay(*p, 50));
    TEST_ASSERT_EQUAL(GrowthStage::FLOWERING,  stageForDay(*p, 51));
    TEST_ASSERT_EQUAL(GrowthStage::FRUITING,   stageForDay(*p, 72));

    // Tablodaki toplam süre aşılsa bile SON dönemde kalınır; dönemi geri
    // almak EC hedefini yarıya indirirdi.
    TEST_ASSERT_EQUAL(GrowthStage::FRUITING, stageForDay(*p, 100000));

    // İki dönemli üründe son dönem VEGETATIVE'dir, FRUITING değil.
    const CropProfile* lettuce = cropById(CropId::LETTUCE);
    TEST_ASSERT_EQUAL(GrowthStage::VEGETATIVE, stageForDay(*lettuce, 100000));
}

void test_crop_key_roundtrip_is_lossless()
{
    // Kablo üzerindeki sözlük tek yönlü bozulursa, kullanıcının seçtiği ürün
    // sessizce başka bir ürüne dönüşür.
    for (uint8_t i = 0; i < cropCount(); ++i)
    {
        const CropProfile* p = cropAt(i);
        CropId back = CropId::NONE;
        TEST_ASSERT_TRUE(cropIdFromKey(cropKeyOf(p->id), back));
        TEST_ASSERT_TRUE(back == p->id);
    }

    CropId id = CropId::STRAWBERRY;
    TEST_ASSERT_TRUE(cropIdFromKey("none", id));
    TEST_ASSERT_TRUE(id == CropId::NONE);
    TEST_ASSERT_TRUE(cropIdFromKey("custom", id));
    TEST_ASSERT_TRUE(id == CropId::CUSTOM);
    TEST_ASSERT_FALSE(cropIdFromKey("kavun", id));

    for (uint8_t s = 0; s < CROP_MAX_STAGES; ++s)
    {
        GrowthStage gs = GrowthStage::SEEDLING;
        TEST_ASSERT_TRUE(stageFromKey(stageKeyOf(static_cast<GrowthStage>(s)), gs));
        TEST_ASSERT_EQUAL_UINT8(s, static_cast<uint8_t>(gs));
    }
}

void test_config_still_fits_the_nvs_blob_budget()
{
    // Şema büyüdükçe bu sayı büyür. Sınır aşıldığında derleme zaten durur
    // (`Config.h` static_assert), ama buradaki test SAYIYI KAYDA GEÇİRİR:
    // bir sonraki geliştirici ne kadar pay kaldığını görebilsin.
    TEST_ASSERT_TRUE(sizeof(Config) <= 640);
    TEST_ASSERT_TRUE(sizeof(CropConfig) <= 24);
    TEST_ASSERT_EQUAL_UINT8(5, MAX_ACTUATORS);
    TEST_ASSERT_EQUAL_UINT8(8, MAX_SENSORS);
}

// ═══════════════════════════════════════════════════════════════════════════
// 9. ENCODER — yön değişiminde kaybolan tık (TASK-071)
//
// Bu hata donanımda GÖRÜLÜR ama TEKRARLANAMAZ: artığın oluşması için bir
// geçişin kaybolması gerekir ve bunu elle üretmek mümkün değildir. Burada
// sentetik olarak kuruluyor.
// ═══════════════════════════════════════════════════════════════════════════

/// Quadrature durum döngüsü: 00 → 01 → 11 → 10 → 00 …
/// Tabloda `quadDelta(0,1) == -1` olduğu için bu yön CCW'dir; ters sırada
/// dönmek CW üretir. Testler yönü tablodan okur, varsaymaz.
static const uint8_t PHASES[4] = {0, 1, 3, 2};

/// Encoder'ı `n` geçiş kadar çevirir ve üretilen tıkları sayar.
///
/// @param dir +1 = tablo sırasında ileri · -1 = geri
static void turn(EncoderDecoder& d, int dir, int transitions, int& cw, int& ccw,
                 int& phaseIdx)
{
    for (int i = 0; i < transitions; ++i)
    {
        phaseIdx = (phaseIdx + dir + 4) % 4;
        const EncoderTick t = encoderAdvance(d, PHASES[phaseIdx], 4);
        if (t == EncoderTick::CW)  { ++cw; }
        if (t == EncoderTick::CCW) { ++ccw; }
    }
}

void test_encoder_clean_rotation_counts_one_tick_per_detent()
{
    EncoderDecoder d{};
    d.reset(PHASES[0]);

    int cw = 0, ccw = 0, idx = 0;
    turn(d, +1, 4 * 5, cw, ccw, idx);          // 5 detent, tek yön

    // Yön tabloya bağlı; hangisi olursa olsun TAM 5 tık üretilmeli ve
    // ters yönde hiç tık olmamalı.
    TEST_ASSERT_EQUAL_INT(5, cw + ccw);
    TEST_ASSERT_TRUE(cw == 0 || ccw == 0);
    TEST_ASSERT_EQUAL_INT8(0, d.steps);        // artık kalmamalı
}

void test_encoder_reversal_after_lost_transition_still_ticks_on_first_detent()
{
    // ── BİLDİRİLEN HATA ────────────────────────────────────────────────────
    // "İleri sürekli gidersem sorun yok; ileri gidip bir geri geldiğimde
    //  o 1 tık çalışmıyor, 2. tıkta geri geliyor."
    //
    // Kurulum: bir geçiş KAYBEDİLİR (flash yazarken GPIO ISR'si çalışmaz veya
    // sıçrama filtresi eler). Bu, biriktiricide kalıcı bir artık bırakır.
    EncoderDecoder d{};
    d.reset(PHASES[0]);

    int cw = 0, ccw = 0, idx = 0;

    // Bir detent döndür ama SON geçişi kaybet: çözücüye 3 geçiş ulaşır.
    turn(d, +1, 3, cw, ccw, idx);
    TEST_ASSERT_EQUAL_INT(0, cw + ccw);        // henüz tık yok
    // Kaybolan 4. geçiş: faz ilerler ama çözücü görmez.
    idx = (idx + 1) % 4;

    // İkinci detent: 4 geçiş. Artık nedeniyle tık ERKEN gelir — kullanıcı
    // ileri yönde bir sorun HİSSETMEZ, her detent bir tık üretmeye devam eder.
    turn(d, +1, 4, cw, ccw, idx);
    TEST_ASSERT_EQUAL_INT(1, cw + ccw);
    const int forwardTicks = cw + ccw;

    // Artık var mı? Hatanın ön koşulu budur.
    TEST_ASSERT_NOT_EQUAL(0, d.steps);

    // ── ASIL İDDİA: ŞİMDİ GERİ DÖN ─────────────────────────────────────────
    // Bir detentlik geri dönüş TAM BİR tık üretmeli. Yön değişiminde artık
    // atılmasaydı bu detent sessiz kalır, tık ancak ikinci detentte gelirdi.
    const int cwBefore = cw, ccwBefore = ccw;
    turn(d, -1, 4, cw, ccw, idx);

    TEST_ASSERT_EQUAL_INT(forwardTicks + 1, cw + ccw);          // tık geldi
    TEST_ASSERT_TRUE((cw - cwBefore) + (ccw - ccwBefore) == 1); // tam bir tane
    // ...ve TERS yönde geldi.
    TEST_ASSERT_TRUE((cw > cwBefore) != (ccw > ccwBefore));
}

void test_encoder_direction_change_discards_partial_movement()
{
    // Kısmi bir dönüş (detenti tamamlamayan) ters yöndeki ilk tıkın hesabına
    // yazılmamalı: kullanıcı o hareketi "bir tık" saymamıştır.
    EncoderDecoder d{};
    d.reset(PHASES[0]);

    int cw = 0, ccw = 0, idx = 0;
    turn(d, +1, 2, cw, ccw, idx);              // yarım detent ileri
    TEST_ASSERT_EQUAL_INT(0, cw + ccw);

    turn(d, -1, 4, cw, ccw, idx);              // tam bir detent geri
    TEST_ASSERT_EQUAL_INT(1, cw + ccw);        // tık GELMELİ
}

void test_encoder_invalid_transitions_are_ignored()
{
    // Çift bit değişimi = kaçırılmış ara durum. Sayılmamalı, ama çözücü
    // yeni duruma senkron kalmalı ki sonraki geçişler doğru hesaplansın.
    EncoderDecoder d{};
    d.reset(0);

    int cw = 0, ccw = 0;
    TEST_ASSERT_EQUAL(EncoderTick::NONE, encoderAdvance(d, 3, 4));  // 00 → 11
    TEST_ASSERT_EQUAL_UINT8(3, d.phase);
    TEST_ASSERT_EQUAL_INT8(0, d.steps);

    // Aynı durumda kalmak da hareket değildir.
    TEST_ASSERT_EQUAL(EncoderTick::NONE, encoderAdvance(d, 3, 4));
    TEST_ASSERT_EQUAL_INT8(0, d.steps);
    (void)cw; (void)ccw;
}

void test_encoder_sync_phase_does_not_count()
{
    // Zaman kapısı bir kenarı reddettiğinde durum güncellenir ama SAYILMAZ.
    // Güncellenmeseydi çözücü gerçek pinlerden ayrışır ve sonraki geçiş
    // tabloda geçersiz görünüp sessizce kaybolurdu.
    EncoderDecoder d{};
    d.reset(PHASES[0]);

    encoderSyncPhase(d, PHASES[1]);
    TEST_ASSERT_EQUAL_UINT8(PHASES[1], d.phase);
    TEST_ASSERT_EQUAL_INT8(0, d.steps);

    // Senkronizasyondan sonraki gerçek geçiş DOĞRU hesaplanmalı.
    const EncoderTick t = encoderAdvance(d, PHASES[2], 4);
    TEST_ASSERT_EQUAL(EncoderTick::NONE, t);   // tek geçiş, detent değil
    TEST_ASSERT_NOT_EQUAL(0, d.steps);         // ama SAYILDI
}

void test_encoder_step_accumulator_cannot_overflow()
{
    // Aynı yönde çok uzun bir dönüş `int8_t` sınırında sarmamalı: sarma,
    // encoder'ın aniden ters yöne dönmesi gibi görünürdü.
    EncoderDecoder d{};
    d.reset(PHASES[0]);

    int cw = 0, ccw = 0, idx = 0;
    turn(d, +1, 4 * 300, cw, ccw, idx);        // 300 detent
    TEST_ASSERT_EQUAL_INT(300, cw + ccw);
    TEST_ASSERT_TRUE(d.steps >= -4 && d.steps <= 4);
}

// ═══════════════════════════════════════════════════════════════════════════
// 10. GEÇMİŞ KAYDI SLOT SIRASI — ISSUE-034
//
// Yazıcı ve okuyucu farklı sıralar kullanıyordu ve grafik aylardır yanlış
// sensörü yanlış ölçekle gösteriyordu. Hata donanımda görünmez: değerler
// "makul" aralıkta kaldığı için kimse fark etmez.
// ═══════════════════════════════════════════════════════════════════════════

void test_history_every_sensor_has_exactly_one_slot()
{
    // Bir sensörün iki slotu olsaydı ikincisi birincisini ezerdi; hiç slotu
    // olmasaydı sessizce kaydedilmezdi. İkisi de ISSUE-034'ün yaşama biçimi.
    for (uint8_t s = 0; s < MAX_SENSORS; ++s)
    {
        const SensorId id   = static_cast<SensorId>(s);
        const uint8_t  slot = services::history::slotOf(id);

        TEST_ASSERT_TRUE(slot < services::history::SENSOR_SLOTS);
        TEST_ASSERT_TRUE(services::history::SLOT_ORDER[slot] == id);
    }
}

void test_history_slot_lookup_rejects_unknown_sensor()
{
    // Tabloda olmayan bir kimlik `SENSOR_SLOTS` döner ve yazıcı onu ATLAR —
    // dizinin dışına yazmak yerine.
    TEST_ASSERT_EQUAL_UINT8(services::history::SENSOR_SLOTS,
                            services::history::slotOf(SensorId::NONE));
}

void test_history_slot_table_covers_all_sensors()
{
    // Slot sayısı sensör sayısıyla eşleşmeli: eşleşmezse bir ölçüm geçmişe
    // hiç girmez ve grafikte "veri yok" olarak görünürdü.
    TEST_ASSERT_EQUAL_UINT8(MAX_SENSORS, services::history::SENSOR_SLOTS);

    // Maskeler 8 bit; slot sayısı 8'i aşarsa üst slotların kalite biti
    // sessizce kaybolurdu.
    TEST_ASSERT_TRUE(services::history::SENSOR_SLOTS <= 8);
}

// ═══════════════════════════════════════════════════════════════════════════

void setUp() {}
void tearDown() {}

int runAllTests()
{
    UNITY_BEGIN();

    RUN_TEST(test_elapsed_wraps_correctly);
    RUN_TEST(test_hasElapsed_across_wrap);
    RUN_TEST(test_hasElapsed_zero_duration_is_always_true);

    RUN_TEST(test_cooldown_blocks_then_allows);
    RUN_TEST(test_cooldown_not_applied_on_first_run);
    RUN_TEST(test_min_runtime_defers_off);
    RUN_TEST(test_max_runtime_exceeded);
    RUN_TEST(test_source_arbitration);

    RUN_TEST(test_water_pump_blocked_by_every_interlock);
    RUN_TEST(test_air_pump_not_blocked_by_level);
    RUN_TEST(test_emergency_has_highest_reporting_priority);

    RUN_TEST(test_backoff_curve_and_cap);
    RUN_TEST(test_jitter_stays_within_bounds);
    RUN_TEST(test_auth_failure_stops_but_ap_missing_never_does);
    RUN_TEST(test_link_lost_first_retry_is_fast_but_auth_never_is);
    RUN_TEST(test_disconnect_classification);

    RUN_TEST(test_normal_window);
    RUN_TEST(test_wrapping_window_over_midnight);
    RUN_TEST(test_periodic_cycle);

    RUN_TEST(test_dhcp_plan);
    RUN_TEST(test_static_plan_falls_back_when_incomplete);
    RUN_TEST(test_static_plan_uses_gateway_when_dns_missing);

    RUN_TEST(test_defaults_pass_their_own_validation);
    RUN_TEST(test_defaults_are_safe);
    RUN_TEST(test_actuator_min_must_be_less_than_max);
    RUN_TEST(test_rule_thresholds_cannot_be_equal);
    RUN_TEST(test_cycle_on_time_must_be_shorter_than_period);
    RUN_TEST(test_conflicting_rules_with_equal_priority_rejected);

    RUN_TEST(test_every_crop_and_stage_produces_valid_rules);
    RUN_TEST(test_generated_cycle_never_exceeds_pump_max_runtime);
    RUN_TEST(test_missing_hardware_produces_no_rule_for_it);
    RUN_TEST(test_heater_and_dosing_thresholds_point_the_right_way);
    RUN_TEST(test_ec_rises_across_fruiting_stages);
    RUN_TEST(test_leafy_crops_reject_fruiting_stage);
    RUN_TEST(test_stage_advances_by_day_and_never_goes_backwards);
    RUN_TEST(test_crop_key_roundtrip_is_lossless);
    RUN_TEST(test_config_still_fits_the_nvs_blob_budget);

    RUN_TEST(test_encoder_clean_rotation_counts_one_tick_per_detent);
    RUN_TEST(test_encoder_reversal_after_lost_transition_still_ticks_on_first_detent);
    RUN_TEST(test_encoder_direction_change_discards_partial_movement);
    RUN_TEST(test_encoder_invalid_transitions_are_ignored);
    RUN_TEST(test_encoder_sync_phase_does_not_count);
    RUN_TEST(test_encoder_step_accumulator_cannot_overflow);

    RUN_TEST(test_history_every_sensor_has_exactly_one_slot);
    RUN_TEST(test_history_slot_lookup_rejects_unknown_sensor);
    RUN_TEST(test_history_slot_table_covers_all_sensors);

    return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup()
{
    delay(2000);   // seri portun hazır olması için
    runAllTests();
}
void loop() {}
#else
int main() { return runAllTests(); }
#endif
