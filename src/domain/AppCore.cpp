#include "domain/AppCore.h"

#include <esp_timer.h>

#include "core/Command.h"
#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "core/SystemState.h"
#include "domain/ActuatorManager.h"
#include "domain/AutomationEngine.h"
#include "domain/EmergencyStop.h"
#include "domain/FlowVerification.h"
#include "domain/SafetyMonitor.h"
#include "domain/SystemSupervisor.h"
#include "services/ConfigService.h"

namespace domain {
namespace appcore {
namespace {

using core::ActuatorId;
using core::Command;
using core::CommandResult;
using core::CommandSource;
using core::CommandType;
using core::ControlSource;
using core::ErrCode;
using core::Millis;

uint32_t g_lastCycleUs   = 0;
uint32_t g_overruns      = 0;
bool     g_budgetLogged  = false;
bool     g_ready         = false;

/// `SystemSupervisor`'ın güvenli hâl işleyicisi (TASK-012).
///
/// Supervisor mod değişiminden ÖNCE bunu çağırır — sıralama orada yapısal
/// olarak garanti edilir. Burada tek iş röleleri kapatmaktır.
void onSafeState(ErrCode reason)
{
    actuators::forceAllOff(reason, Millis{0});
}

/// Bir komutun kaynağını kontrol yetkisine çevirir.
///
/// Web ve OLED aynı yetkiye sahiptir: ikisi de operatördür. `SYSTEM` kaynağı
/// iç kararlar içindir ve otomasyon seviyesinde kalır — bir iç kararın
/// operatörün manuel seçimini ezmesi P6'ya aykırı olurdu.
constexpr ControlSource authorityOf(CommandSource s)
{
    return (s == CommandSource::SYSTEM) ? ControlSource::AUTOMATION : ControlSource::MANUAL;
}

void applyCommand(const Command& c, const core::SystemState& snap, Millis now)
{
    switch (c.type)
    {
        case CommandType::SET_ACTUATOR:
        {
            const ActuatorId id = static_cast<ActuatorId>(c.target);
            const CommandResult r =
                actuators::request(id, c.param != 0, authorityOf(c.source), now);

            if (r != CommandResult::ACCEPTED && r != CommandResult::NO_CHANGE)
            {
                // Sessiz engelleme yok (§12.2): reddedilen her komut kaydedilir.
                core::diag::log(core::LogLevel::INFO, ErrCode::SAFETY_BLOCKED,
                                static_cast<int32_t>(r), "aktuator komutu uygulanmadi");
            }

            // OPERATÖR müdahalesi otomasyonu O AKTÜATÖR için süreli susturur
            // (§10.3). Yalnızca gerçekten operatörden gelen komutlar sayılır:
            // otomasyonun kendi isteği override üretseydi motor kendini
            // susturur ve bir daha hiç çalışmazdı.
            if (authorityOf(c.source) == ControlSource::MANUAL &&
                r != CommandResult::REJECTED_INVALID)
            {
                automation::noteManualCommand(id, now);
            }
            break;
        }

        case CommandType::SET_AUTOMATION_MODE:
        {
            const core::AutomationMode m = (c.param != 0) ? core::AutomationMode::AUTO
                                                          : core::AutomationMode::MANUAL;
            const ErrCode rc = automation::setMode(m);
            if (rc != ErrCode::OK)
            {
                core::diag::log(core::LogLevel::WARNING, rc, c.param,
                                "otomasyon modu degistirilemedi");
            }
            break;
        }

        case CommandType::EMERGENCY_CLEAR:
            // Tek onay noktası: akış mandalı + acil durum mandalı + sayaçlar
            // birlikte temizlenir, canlı koşullar önce kontrol edilir.
            (void)safety::acknowledge(snap, now);
            break;

        case CommandType::SYSTEM_RESTART:
            supervisor::requestRestart(ErrCode::OK);
            break;

        case CommandType::FACTORY_RESET:
            // Uç nokta `confirm=FACTORY_RESET` ister ve yetki arar (TASK-043);
            // buraya ulaşan bir komut ONAYLANMIŞ demektir.
            //
            // `config::factoryReset()` config NVS'ini VE sırları siler —
            // parola hash'i dahil. Yeniden başlatmada `auth::begin()` hash
            // bulamaz ve sistem KURULUM MODUNA döner. `auth` `interfaces/`
            // katmanında olduğu için buradan çağrılamaz (D1); gerek de yok.
            {
                const ErrCode fr = services::config::factoryReset();
                if (fr != ErrCode::OK) { core::diag::raise(fr); }

                // Kontrollü yeniden başlatma: silinmiş bir config ile
                // çalışmaya devam etmek, yarı sıfırlanmış bir sistem demektir.
                supervisor::requestRestart(ErrCode::OK, core::millisecs(1000));
            }
            break;

        case CommandType::CONFIG_RELOAD:
        case CommandType::NETWORK_SCAN:
        case CommandType::NETWORK_RETRY_NOW:
        case CommandType::NETWORK_FORGET:
            // Bu komutların sahibi başka modüllerdir (TASK-015 fabrika
            // ayarları, TASK-036 ağ). P7 gereği burada boş gövde YAZILMAZ —
            // sahibi uygulandığında bu satırlar oraya taşınır.
            break;

        case CommandType::NONE:
        default:
            core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_INVALID_REQUEST,
                            static_cast<int32_t>(c.type), "bilinmeyen komut turu");
            break;
    }
}

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    // Güvenlik zincirini kur. SIRA ÖNEMLİ: aktüatör yöneticisi güvenlik
    // izni olmadan başlatılamaz (`permit == nullptr` reddedilir).
    ErrCode rc = safety::begin(cfg);
    if (rc != ErrCode::OK) { return rc; }

    rc = flow::begin(cfg);
    if (rc != ErrCode::OK) { return rc; }

    rc = emergency::begin();
    if (rc != ErrCode::OK) { return rc; }

    rc = actuators::begin(cfg, &safety::permits);
    if (rc != ErrCode::OK) { return rc; }

    // Otomasyon EN SON: güvenlik zinciri kurulmadan motorun başlatılması,
    // kuralların vetosuz bir aktüatör yöneticisine istek göndermesi
    // anlamına gelirdi.
    rc = automation::begin(cfg);
    if (rc != ErrCode::OK) { return rc; }

    // Supervisor'ın SAFE/EMERGENCY geçişlerinde röleleri kapatması için.
    supervisor::setSafeStateHandler(&onSafeState);

    g_ready = true;
    return ErrCode::OK;
}

void tick(Millis now)
{
    if (!g_ready) { return; }

    const int64_t t0 = esp_timer_get_time();

    // ── 0) ACİL DURDURMA — snapshot'tan bile önce ───────────────────────────
    // Bu kararın girdisi yoktur; "her şeyi kapat" bir snapshot beklemez.
    // Kuyruğu tamamen atlayan atomik yol (TASK-008).
    {
        CommandSource src;
        ErrCode       reason;
        if (core::cmdq::takeEmergencyStop(src, reason))
        {
            emergency::trigger(reason, static_cast<uint8_t>(src), now);
        }
    }

    // ── 1) SNAPSHOT — bir kez, döngü boyunca aynı görüntü ───────────────────
    // Ortada yeniden okumak, güvenliğin bir görüntüye aktüatörün başka bir
    // görüntüye göre karar vermesi demektir.
    core::SystemState snap{};
    (void)core::state::snapshot(snap);

    // ── 2) KOMUTLARI AL (sınırlı) ──────────────────────────────────────────
    Command batch[MAX_COMMANDS_PER_CYCLE];
    uint8_t n = 0;
    while (n < MAX_COMMANDS_PER_CYCLE && core::cmdq::receive(batch[n]))
    {
        if (core::isStale(batch[n], now, core::millisecs(COMMAND_MAX_AGE_MS)))
        {
            // Kuyrukta bekleyip bayatlamış bir "pompayı aç" komutu, koşullar
            // değiştikten sonra uygulanmamalıdır.
            core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_INVALID_REQUEST,
                            static_cast<int32_t>(batch[n].type), "bayat komut atildi");
            continue;
        }
        ++n;
    }

    // ── 3) GÜVENLİK — HER ZAMAN otomasyondan önce ───────────────────────────
    // Akış doğrulaması ve acil durum mandalı bu çağrının içinde zincire girer.
    safety::evaluate(snap, now);

    // ── 4) OTOMASYON — TASK-057 ────────────────────────────────────────────
    // Güvenlik değerlendirmesinden SONRA, aktüatörleri sürmeden ÖNCE.
    // Bu sıra §11.1 gereği değiştirilemez: otomasyon, güvenlik
    // değerlendirmesi yapılmamış bir state üzerinde karar veremez.
    //
    // M4 KAPISI: motor `MANUAL` modda kuralları HİÇ değerlendirmez ve
    // varsayılan mod `MANUAL`. Kapı donanımda açılana kadar bu çağrı
    // etkisizdir (TASK-054 Karar 0).
    automation::evaluate(snap, snap.time.valid != 0u, now);

    // ── 5) KOMUTLARI UYGULA ────────────────────────────────────────────────
    for (uint8_t i = 0; i < n; ++i) { applyCommand(batch[i], snap, now); }

    // ── 6) AKTÜATÖRLERİ SÜR — röleye giden tek kapı ─────────────────────────
    actuators::apply(now);

    // ── 7) YAYINLA ─────────────────────────────────────────────────────────
    (void)actuators::publish(now);
    (void)safety::publish(now);
    (void)automation::publish(snap, snap.time.valid != 0u, now);

    supervisor::tick(now);

    // ── Bütçe izleme ───────────────────────────────────────────────────────
    g_lastCycleUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
    if (g_lastCycleUs > CYCLE_BUDGET_US)
    {
        ++g_overruns;
        if (!g_budgetLogged)
        {
            // Yalnızca ilk aşımda: her döngüde loglamak, aşımın kendisini
            // besleyen bir log seli üretirdi.
            g_budgetLogged = true;
            core::diag::log(core::LogLevel::WARNING, ErrCode::SYS_TASK_HEARTBEAT_LOST,
                            static_cast<int32_t>(g_lastCycleUs),
                            "app_core dongu butcesi asildi");
        }
    }
}

uint32_t lastCycleUs()    { return g_lastCycleUs; }
uint32_t budgetOverruns() { return g_overruns; }

} // namespace appcore
} // namespace domain
