#include "BootWiring.h"

#include <Arduino.h>

#include "core/BoardPins.h"
#include "core/BootReport.h"
#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "core/TaskRegistry.h"
#include "core/WatchdogGuard.h"
#include "hal/AdcInput.h"
#include "hal/FileStore.h"
#include "hal/InputDevices.h"
#include "hal/NvsStore.h"
#include "hal/OledPanel.h"
#include "hal/PulseCounter.h"
#include "hal/RelayOutput.h"
#include "hal/WifiRadio.h"
#include "services/ConfigService.h"
#include "tasks/TaskConfig.h"
#include "tasks/TaskRunner.h"

// ---------------------------------------------------------------------------
// Task giriş noktaları — her biri kendi dosyasında tanımlı.
//
// Bildirimler burada: `TaskConfig.h`'a koymak, tabloyu kuran her dosyanın
// tüm task uygulamalarını görmesi demek olurdu.
// ---------------------------------------------------------------------------
namespace tasks {
void appCoreTaskEntry(void*);
void sensorTaskEntry(void*);
void networkTaskEntry(void*);
void uiTaskEntry(void*);
void storeTaskEntry(void*);
} // namespace tasks

namespace app {
namespace {

using core::BootStage;
using core::BootStageDef;
using core::ErrCode;

// ── Aşama 0: reset nedeni + watchdog ───────────────────────────────────────
//
// LOG ALTYAPISINDAN ÖNCE çalışır. Reset nedeni burada okunmazsa, sonraki
// aşamalarda üretilen loglar önceki oturumun nasıl bittiğini gizler.
ErrCode stageResetAndWdt()
{
    (void)core::diag::captureResetReason();
    return core::wdt::begin();
}

// ── Aşama 1: RÖLELER GÜVENLİ ───────────────────────────────────────────────
//
// PAZARLIKSIZ SIRA. Bu aşama her şeyden önce gelir; log altyapısı bile
// henüz hazır değil. Pompanın korunması loglamadan önceliklidir.
//
// ISSUE-003: GPIO'lar reset'ten sonra yüzlerce ms yüksek empedansta kalır;
// bu pencere ancak DONANIMLA (harici pull-up/pull-down) kapatılır.
ErrCode stageGpioSafe()
{
    pinMode(board::STATUS_LED, OUTPUT);
    digitalWrite(board::STATUS_LED, LOW);
    return hal::relay::begin();
}

// ── Aşama 2: çekirdek servisler ────────────────────────────────────────────
ErrCode stageCoreServices()
{
    ErrCode rc = core::diag::begin();
    if (rc != ErrCode::OK) { return rc; }

    core::taskreg::begin();

    rc = core::state::begin();
    if (rc != ErrCode::OK) { return rc; }

    return core::cmdq::begin();
}

// ── Aşama 3: config ────────────────────────────────────────────────────────
//
// Başarısızsa VARSAYILAN config kullanılır — `ConfigService::load()` bunu
// kendi içinde yapar ve `CFG_NOT_FOUND`/`CFG_CORRUPT` döner. Bu bir
// başarısızlık değil, belgelenmiş bir geri düşüştür; aşama yine de OK
// raporlar ki mod gereksiz yere DEGRADED'a düşmesin.
ErrCode stageConfigLoad()
{
    const ErrCode nvs = hal::nvsstore::begin();
    if (nvs != ErrCode::OK) { return nvs; }

    const ErrCode rc = services::config::load();
    return (rc == ErrCode::CFG_NOT_FOUND || rc == ErrCode::CFG_CORRUPT) ? ErrCode::OK : rc;
}

// ── Aşama 4: dosya sistemi ─────────────────────────────────────────────────
ErrCode stageFilesystem() { return hal::fs::begin(); }

// ── Aşama 5: OLED ──────────────────────────────────────────────────────────
//
// Başarısızsa sistem TAM çalışır, yalnızca ekran yoktur. `UiService` bunu
// kendi içinde ele alıyor: girdi işlenir, komut üretilir, çizim atlanır.
ErrCode stageDisplay() { return hal::oled::begin(); }

// ── Aşama 6: sensör donanımı ───────────────────────────────────────────────
ErrCode stageSensorHw()
{
    const ErrCode adc = hal::adc::begin();
    if (adc != ErrCode::OK) { return adc; }

    const ErrCode pcnt = hal::pulse::begin(board::FLOW_PULSE);
    if (pcnt != ErrCode::OK) { return pcnt; }

    // Encoder ve butonlar: `ui` task'ının girdi kaynağı.
    //
    // Varsayılan detent oranı 4'tür (EC11 ve benzerleri). SAHADA DOĞRULANMALI:
    // oran yanlışsa bir tık iki satır atlar ya da iki tık bir satır ilerletir.
    // Sayfa gezinmesinde (TASK-075) bu doğrudan "sayfa atlıyor" olarak görünür.
    //
    // Test: bir sayfanın içine girip listeyi çevirin — BİR tık BİR satır
    // ilerletmeli. İki satır atlıyorsa buraya `begin(2)` yazılır.
    return hal::input::begin();
}

// ── Aşama 7: Wi-Fi radyosu ─────────────────────────────────────────────────
//
// Başarısızsa sistem OFFLINE çalışır; GÜVENLİK ETKİLENMEZ. Ağ ve güvenlik
// arasında hiçbir bağımlılık yoktur (ARCHITECTURE §16.3).
ErrCode stageNetworkRadio() { return hal::wifi::begin(); }

// ── Aşama 8: task oluşturma ────────────────────────────────────────────────
//
// Bu noktada config (3), dosya sistemi (4), OLED (5), sensör donanımı (6)
// ve radyo (7) ZATEN hazır. Bir task oluşturulduğu anda bağımlılıkları
// karşılanmış durumda — ayrı bir EventGroup senkronizasyonu gerekmiyor
// (TASK-060 Karar 4).
ErrCode stageTaskCreation()
{
    using namespace ::tasks;

    static const TaskDef TABLE[] = {
        {core::TaskId::APP_CORE, "app_core", appCoreTaskEntry, core::millisecs(100),
         core::TaskClass::CONTROL, STACK_APP_CORE, PRIO_APP_CORE, CORE_CONTROL},

        {core::TaskId::IO_SENSE, "io_sense", sensorTaskEntry, core::millisecs(250),
         core::TaskClass::SENSING, STACK_IO_SENSE, PRIO_IO_SENSE, CORE_CONTROL},

        {core::TaskId::NET, "net", networkTaskEntry, core::millisecs(100),
         core::TaskClass::NETWORK, STACK_NET, PRIO_NET, CORE_NETWORK},

        {core::TaskId::UI, "ui", uiTaskEntry, core::millisecs(50),
         core::TaskClass::UI, STACK_UI, PRIO_UI, CORE_CONTROL},

        {core::TaskId::STORE, "store", storeTaskEntry, core::millisecs(0),
         core::TaskClass::STORAGE, STACK_STORE, PRIO_STORE, CORE_NETWORK},
    };

    return createAll(TABLE, static_cast<uint8_t>(sizeof(TABLE) / sizeof(TABLE[0])));
}

// ── Aşama tablosu — ARCHITECTURE §7.1 ile BİREBİR ──────────────────────────
//
// `required` sütunu mod türetmesini belirler:
//   zorunlu başarısız         → SAFE
//   zorunlu olmayan başarısız → DEGRADED
//   hepsi başarılı            → RUNNING
//
// Zorunlu olmayan hiçbir aşama boot'u DURDURMAZ (P4 — fail-degraded):
// Wi-Fi yoksa sistem sulamaya devam eder, OLED yoksa web çalışır.
constexpr BootStageDef STAGES[] = {
    {BootStage::RESET_AND_WDT,   true,  stageResetAndWdt},
    {BootStage::GPIO_SAFE_STATE, true,  stageGpioSafe},
    {BootStage::CORE_SERVICES,   true,  stageCoreServices},
    {BootStage::CONFIG_LOAD,     false, stageConfigLoad},
    {BootStage::FILESYSTEM,      false, stageFilesystem},
    {BootStage::DISPLAY_HW,      false, stageDisplay},
    {BootStage::SENSOR_HW,       false, stageSensorHw},
    {BootStage::NETWORK_RADIO,   false, stageNetworkRadio},
    {BootStage::TASK_CREATION,   true,  stageTaskCreation},
};

constexpr uint8_t STAGE_COUNT = static_cast<uint8_t>(sizeof(STAGES) / sizeof(STAGES[0]));

// Tablo eksiksiz mi? Bir aşama eklenip tabloya konmazsa derleme uyarır.
static_assert(STAGE_COUNT == static_cast<uint8_t>(BootStage::TASK_CREATION) + 1u,
              "boot asama tablosu eksik - BootStage ile birebir olmali");

} // namespace

core::SystemMode runBoot(core::BootReport& outReport)
{
    const core::SystemMode mode = core::boot::run(STAGES, STAGE_COUNT, outReport);
    core::emitBootReport(outReport, &core::boot::stageName);
    return mode;
}

} // namespace app
