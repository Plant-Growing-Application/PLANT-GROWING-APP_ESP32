#include "SensorService.h"

#include "ConfigService.h"
#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "hal/AdcInput.h"
#include "sensors/AnalogSensors.h"
#include "sensors/EnvSensors.h"
#include "sensors/FlowSensor.h"
#include "sensors/SensorRegistry.h"
#include "sensors/WaterLevelSensor.h"

namespace services {
namespace sensorsvc {
namespace {

using core::ErrCode;
using core::SensorId;
using core::SensorQuality;
using namespace services::sensors;

// --- Sensör örnekleri: STATİK, heap yok (ARCHITECTURE §9.1) -----------------
WaterLevelSensor  g_level;
FlowSensor        g_flow;
WaterTempSensor   g_temp;
PhSensor          g_ph;
EcSensor          g_ec;
AmbientTempSensor g_airTemp;
HumiditySensor    g_humidity;
LightSensor       g_light;

/// Kayıt tablosuyla AYNI SIRADA. Sıra önemli: su sıcaklığı EC'den ÖNCE
/// gelir, çünkü EC sıcaklık telafisi için o turun sıcaklık değerini ister.
///
/// Ortam sensörleri en sonda: hiçbiri güvenlik zincirinin girdisi değildir ve
/// aralarında bağımlılık yoktur.
ISensor* const g_sensors[REGISTERED_SENSOR_COUNT] = {
    &g_level,     // WATER_LEVEL  — güvenlik
    &g_flow,      // WATER_FLOW   — güvenlik
    &g_temp,      // WATER_TEMP   — EC telafisi için ÖNCE
    &g_ph,        // PH
    &g_ec,        // EC           — sıcaklığı bağlamdan alır
    &g_airTemp,   // AMBIENT_TEMP — AHT20 (paylaşılan çip)
    &g_humidity,  // HUMIDITY     — AHT20 (paylaşılan çip)
    &g_light,     // LIGHT        — BH1750
};

PipelineState      g_pipeline[REGISTERED_SENSOR_COUNT];
core::SensorSample g_lastSample[REGISTERED_SENSOR_COUNT];
core::Millis       g_lastSampleAt[REGISTERED_SENSOR_COUNT];

/// EC telafisi için son geçerli sıcaklık. O turda sıcaklık okunmadıysa
/// önceki geçerli değer kullanılır; yaşı kontrol edilir.
float        g_lastValidTempC  = 0.0f;
core::Millis g_lastValidTempAt = core::Millis{0};
bool         g_hasValidTemp    = false;

/// Bağlam sıcaklığının en fazla ne kadar eski olabileceği. Daha eskiyse
/// telafi yapılmaz ve EC `lowConfidence` işaretlenir.
constexpr core::Duration TEMP_CONTEXT_MAX_AGE = core::seconds(10);

bool    g_ready            = false;
uint8_t g_lastCycleSamples = 0;

// ── ÇALIŞMA ANINDA SENSÖR AÇMA (ISSUE-035) ─────────────────────────────────
//
// `begin()` yalnızca boot'ta koşuyordu. Bir sensör web'den etkinleştirildiğinde
// sürücüsü HİÇ başlatılmıyor, `sample()` her turda `FAULT` dönüyordu — yani
// kullanıcı sensörü açıyor, arayüzde "okunamıyor" görüyor ve nedenini
// anlayamıyordu. Yeniden başlatma gerektiği hiçbir yerde yazmıyordu.
//
// Çözüm: config sürüm sayacını izle. Sensör açıldığında sürücüsünü BURADA,
// `io_sense` bağlamında başlat — donanıma dokunan tek task orası.

/// Her sensörün sürücüsü başlatıldı mı?
bool g_driverReady[REGISTERED_SENSOR_COUNT] = {};

/// Görülen son sensör config sürümü.
uint32_t g_seenSensorsRevision = 0;

/// Başlatma başarısızsa bu süre dolmadan yeniden denenmez.
///
/// Takılı olmayan bir çipi her 250 ms'de yoklamak I2C hattını OLED ile
/// gereksiz yere paylaştırır ve her denemede bir hata satırı üretirdi.
constexpr core::Duration DRIVER_RETRY_PERIOD = core::seconds(10);

core::Millis g_lastInitTry[REGISTERED_SENSOR_COUNT] = {};
bool         g_everTriedInit[REGISTERED_SENSOR_COUNT] = {};

/// Kayıt tablosundaki indeksi `SensorConfig` dizisindeki indekse çevirir.
inline uint8_t configIndexOf(SensorId id)
{
    return static_cast<uint8_t>(id);
}

} // namespace

core::ErrCode begin()
{
    const core::Config& cfg = config::get();

    core::ErrCode overall = ErrCode::OK;

    for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
    {
        g_pipeline[i].reset();
        g_lastSampleAt[i] = core::Millis{0};

        const SensorId id     = kSensorTable[i].id;
        const uint8_t  cfgIdx = configIndexOf(id);

        g_lastSample[i]    = core::SensorSample{};
        g_lastSample[i].id = id;

        // Devre dışı sensörün DONANIMINA HİÇ DOKUNULMAZ.
        // pH/EC sahada takılı olmayabilir; arayüz bunu "arıza" değil
        // "yok" olarak göstermeli (ARCHITECTURE §9.3).
        if (cfg.sensors[cfgIdx].enabled == 0u)
        {
            g_lastSample[i].quality = SensorQuality::NOT_PRESENT;
            continue;
        }

        const ErrCode rc = g_sensors[i]->begin(cfg.sensors[cfgIdx]);
        if (rc == ErrCode::OK)
        {
            g_driverReady[i] = true;
        }
        else
        {
            // BİR SENSÖRÜN HATASI DİĞERLERİNİ ETKİLEMEZ: döngü devam eder.
            core::diag::log(core::LogLevel::ERROR, rc, static_cast<int32_t>(id),
                            "sensor baslatilamadi");
            g_lastSample[i].quality = SensorQuality::FAULT;
            overall                 = rc;
        }
        g_everTriedInit[i] = true;
    }

    g_seenSensorsRevision = config::sensorsRevision();
    g_ready = true;
    return overall;
}

void tick(core::Millis now)
{
    if (!g_ready)
    {
        return;
    }

    const core::Config& cfg = config::get();

    // ── SENSÖR AÇILDI MI? (ISSUE-035) ──────────────────────────────────────
    // Sürüm sayacı değiştiyse, etkinleştirilmiş ama sürücüsü hazır olmayan
    // sensörler burada başlatılır. Kapatılanların sürücüsü serbest bırakılır
    // ki tekrar açıldığında yeniden başlatılsın.
    const uint32_t sensRev = config::sensorsRevision();
    if (sensRev != g_seenSensorsRevision)
    {
        g_seenSensorsRevision = sensRev;
        for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
        {
            if (cfg.sensors[configIndexOf(kSensorTable[i].id)].enabled == 0u)
            {
                g_driverReady[i]   = false;
                g_everTriedInit[i] = false;
                g_pipeline[i].reset();
            }
        }
    }

    for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
    {
        const uint8_t cfgIdx = configIndexOf(kSensorTable[i].id);
        if (cfg.sensors[cfgIdx].enabled == 0u || g_driverReady[i]) { continue; }

        // Başarısız denemeler hız sınırlı: takılı olmayan bir çipi her
        // 250 ms'de yoklamak I2C hattını meşgul eder ve olay günlüğünü aynı
        // hatayla doldururdu.
        if (g_everTriedInit[i] &&
            !core::hasElapsed(now, g_lastInitTry[i], DRIVER_RETRY_PERIOD))
        {
            continue;
        }

        g_lastInitTry[i]   = now;
        g_everTriedInit[i] = true;

        const ErrCode rc = g_sensors[i]->begin(cfg.sensors[cfgIdx]);
        if (rc == ErrCode::OK)
        {
            g_driverReady[i] = true;
            g_pipeline[i].reset();
            g_lastSampleAt[i] = core::Millis{0};   // ilk örneği hemen al
            core::diag::log(core::LogLevel::INFO, ErrCode::OK,
                            static_cast<int32_t>(kSensorTable[i].id),
                            "sensor calisma aninda baslatildi");
        }
        else
        {
            core::diag::log(core::LogLevel::WARNING, rc,
                            static_cast<int32_t>(kSensorTable[i].id),
                            "sensor baslatilamadi - yeniden denenecek");
        }
    }

    // Bağlam: EC sıcaklık telafisi için. Bağımlılık AÇIK ve TEK YÖNLÜ —
    // EC sensörü sıcaklık sensörünü ÇAĞIRMAZ, verilen bağlamı kullanır.
    SampleContext ctx;
    ctx.now            = now;
    ctx.waterTempC     = g_lastValidTempC;
    ctx.waterTempValid = g_hasValidTemp &&
                         !core::hasElapsed(now, g_lastValidTempAt, TEMP_CONTEXT_MAX_AGE);

    uint8_t sampled = 0;

    for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
    {
        const SensorDescriptor& d      = kSensorTable[i];
        const uint8_t           cfgIdx = configIndexOf(d.id);

        if (cfg.sensors[cfgIdx].enabled == 0u)
        {
            g_lastSample[i] = pipeline::notPresent(d.id, now);
            continue;
        }

        // Etkin ama sürücüsü henüz hazır değil (çip yanıt vermiyor veya
        // yeniden deneme bekliyor). Sürücüyü çağırmak yerine FAULT
        // bildiriyoruz — hazır olmayan bir sürücüden okumak tanımsızdır.
        if (!g_driverReady[i])
        {
            g_lastSample[i]           = core::SensorSample{};
            g_lastSample[i].id        = d.id;
            g_lastSample[i].timestamp = now;
            g_lastSample[i].quality   = SensorQuality::FAULT;
            continue;
        }

        // Periyodu dolmadıysa geç.
        //
        // TAŞMA GÜVENLİ DESEN: SON okuma anı saklanır ve GEÇEN SÜRE periyotla
        // karşılaştırılır. "Bir sonraki okuma zamanı" saklayıp `elapsed()` ile
        // karşılaştırmak ISSUE-012'nin tuzağıdır: unsigned çıkarma sarar ve
        // koşul her zaman doğru olur — periyotlar sessizce yok sayılırdı.
        //
        // GÜVENLİK SENSÖRÜ GARANTİSİ: burada yük dağıtma (döngü başına en
        // fazla N sensör) YOKTUR. Periyodu dolan her sensör okunur; güvenlik
        // sensörü hiçbir zaman sıraya girip gecikmez.
        if (g_lastSampleAt[i].v != 0u &&
            !core::hasElapsed(now, g_lastSampleAt[i], d.samplePeriod))
        {
            continue;
        }

        const RawSample raw = g_sensors[i]->sample(ctx);

        g_lastSample[i] =
            pipeline::process(d.id, raw, cfg.sensors[cfgIdx], g_pipeline[i], now);

        g_lastSampleAt[i] = now;
        ++sampled;

        // Sıcaklık okunduysa bağlamı AYNI TURDA güncelle: sırada EC var.
        if (d.id == SensorId::WATER_TEMP &&
            g_lastSample[i].quality == SensorQuality::OK)
        {
            g_lastValidTempC   = g_lastSample[i].value;
            g_lastValidTempAt  = now;
            g_hasValidTemp     = true;
            ctx.waterTempC     = g_lastValidTempC;
            ctx.waterTempValid = true;
        }
    }

    g_lastCycleSamples = sampled;

    // --- TEK YAYINLAMA ---
    //
    // Sensör başına ayrı `publishSensors()` çağrısı, okuyucuya tutarsız bir
    // ara görüntü verirdi (seviye yeni, akış eski) ve güvenlik kararı karışık
    // veriyle alınırdı. Ayrıca versiyon sayacını gereksiz artırıp web'de
    // boşuna WS trafiği üretirdi.
    core::SensorsStatus out{};
    out.count = REGISTERED_SENSOR_COUNT;
    for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
    {
        out.samples[i] = g_lastSample[i];
    }
    core::state::publishSensors(out);
}

void lastSample(SensorId id, core::SensorSample& out)
{
    for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
    {
        if (kSensorTable[i].id == id)
        {
            out = g_lastSample[i];
            return;
        }
    }
    out         = core::SensorSample{};
    out.id      = id;
    out.quality = SensorQuality::NOT_PRESENT;
}

float totalLiters()
{
    return g_flow.totalLiters();
}

bool levelSensorsInconsistent()
{
    return g_level.inconsistent();
}

uint8_t lastCycleSampleCount()
{
    return g_lastCycleSamples;
}

} // namespace sensorsvc
} // namespace services
