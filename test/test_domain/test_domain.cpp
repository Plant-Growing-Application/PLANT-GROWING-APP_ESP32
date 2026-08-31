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
