#include "Aht20.h"

#include <Wire.h>

#include "core/Diagnostics.h"
#include "hal/I2cBus.h"

namespace hal {
namespace aht20 {
namespace {

using core::ErrCode;
using core::Millis;

// --- Komutlar (veri sayfası) ------------------------------------------------
constexpr uint8_t CMD_STATUS      = 0x71;
constexpr uint8_t CMD_INIT        = 0xBE;
constexpr uint8_t CMD_INIT_ARG0   = 0x08;
constexpr uint8_t CMD_INIT_ARG1   = 0x00;
constexpr uint8_t CMD_MEASURE     = 0xAC;
constexpr uint8_t CMD_MEASURE_A0  = 0x33;
constexpr uint8_t CMD_MEASURE_A1  = 0x00;

constexpr uint8_t STATUS_BUSY       = 0x80;  ///< bit 7: ölçüm sürüyor
constexpr uint8_t STATUS_CALIBRATED = 0x08;  ///< bit 3: fabrika kalibrasyonu yüklü

/// Okunan çerçeve: durum + 5 veri baytı + CRC.
constexpr uint8_t FRAME_LEN = 7;

/// 20 bitlik ham değerin tam ölçeği (2^20).
constexpr float FULL_SCALE = 1048576.0f;

enum class Phase : uint8_t
{
    UNINITIALIZED = 0,
    IDLE          = 1,  ///< ölçüm tetiklenmeyi bekliyor
    MEASURING     = 2,  ///< tetiklendi, dönüşüm sürüyor
};

Phase    g_phase      = Phase::UNINITIALIZED;
Millis   g_triggerAt  = Millis{0};
Millis   g_lastService = Millis{0};
bool     g_serviced   = false;  ///< `g_lastService` anlamlı mı (Millis{0} geçerli bir andır)
bool     g_hasReading = false;
bool     g_available  = false;
uint16_t g_errors     = 0;

float g_tempC = 0.0f;
float g_rhPct = 0.0f;

/// Sensirion/ASAIR CRC8 — polinom 0x31, başlangıç 0xFF.
uint8_t crc8(const uint8_t* data, uint8_t len)
{
    uint8_t crc = 0xFFu;
    for (uint8_t i = 0; i < len; ++i)
    {
        crc = static_cast<uint8_t>(crc ^ data[i]);
        for (uint8_t bit = 0; bit < 8u; ++bit)
        {
            crc = (crc & 0x80u) != 0u ? static_cast<uint8_t>((crc << 1) ^ 0x31u)
                                      : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

/// Hata sayacını ilerletir ve sınır aşılınca çipi devre dışı bırakır.
void noteError()
{
    if (g_errors < 0xFFFFu)
    {
        ++g_errors;
    }
    if (g_errors == ERROR_LIMIT && g_available)
    {
        // Sistemi durdurmuyoruz: ortam sensörü güvenlik zincirinin parçası
        // DEĞİLDİR. Sensör kullanılamaz işaretlenir, sistem çalışmaya devam
        // eder (ARCHITECTURE P4).
        g_available = false;
        core::diag::log(core::LogLevel::ERROR, ErrCode::SENSOR_OPEN_CIRCUIT,
                        static_cast<int32_t>(g_errors),
                        "AHT20 yanit vermiyor — ortam sensoru devre disi");
    }
}

void noteSuccess()
{
    g_errors = 0;
}

/// Tek baytlık komut yazar.
bool writeCommand(uint8_t b0)
{
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(b0);
    return Wire.endTransmission() == 0;
}

/// Üç baytlık komut yazar.
bool writeCommand3(uint8_t b0, uint8_t b1, uint8_t b2)
{
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(b0);
    Wire.write(b1);
    Wire.write(b2);
    return Wire.endTransmission() == 0;
}

/// Durum baytını okur.
bool readStatus(uint8_t& out)
{
    if (!writeCommand(CMD_STATUS))
    {
        return false;
    }
    if (Wire.requestFrom(static_cast<int>(I2C_ADDRESS), 1) != 1)
    {
        return false;
    }
    out = static_cast<uint8_t>(Wire.read());
    return true;
}

/// Ölçüm tetikler ve fazı ilerletir.
void triggerMeasurement(Millis now)
{
    if (!writeCommand3(CMD_MEASURE, CMD_MEASURE_A0, CMD_MEASURE_A1))
    {
        noteError();
        // Faz IDLE kalır: bir sonraki turda yeniden denenir.
        g_phase = Phase::IDLE;
        return;
    }
    g_triggerAt = now;
    g_phase     = Phase::MEASURING;
}

/// Tamamlanmış ölçümü okur. Başarılıysa değerleri günceller.
///
/// @return true = çerçeve okundu ve geçerli
bool readMeasurement()
{
    uint8_t buf[FRAME_LEN];

    if (Wire.requestFrom(static_cast<int>(I2C_ADDRESS), static_cast<int>(FRAME_LEN)) !=
        static_cast<int>(FRAME_LEN))
    {
        return false;
    }
    for (uint8_t i = 0; i < FRAME_LEN; ++i)
    {
        buf[i] = static_cast<uint8_t>(Wire.read());
    }

    // Çip hâlâ meşgulse çerçeve önceki ölçümdür — kullanma, tekrar bekle.
    if ((buf[0] & STATUS_BUSY) != 0u)
    {
        return false;
    }

    // CRC BOZUKSA DEĞER KULLANILMAZ. Bozuk bir aktarım 22 °C yerine 120 °C
    // okutabilir ve bu değer doğrudan ısıtıcı eşiğine girer.
    if (crc8(buf, 6) != buf[6])
    {
        return false;
    }

    const uint32_t rawHum = (static_cast<uint32_t>(buf[1]) << 12) |
                            (static_cast<uint32_t>(buf[2]) << 4) |
                            (static_cast<uint32_t>(buf[3]) >> 4);
    const uint32_t rawTemp = ((static_cast<uint32_t>(buf[3]) & 0x0Fu) << 16) |
                             (static_cast<uint32_t>(buf[4]) << 8) |
                             static_cast<uint32_t>(buf[5]);

    g_rhPct = (static_cast<float>(rawHum) * 100.0f) / FULL_SCALE;
    g_tempC = ((static_cast<float>(rawTemp) * 200.0f) / FULL_SCALE) - 50.0f;

    g_hasReading = true;
    return true;
}

} // namespace

core::ErrCode begin()
{
    // ── İKİ KEZ ÇAĞRILIR, İKİNCİSİ İŞLEMSİZ (TASK-072) ─────────────────────
    // Sıcaklık ve nem sarmalayıcılarının ikisi de kendi `begin()`'inde bunu
    // çağırır. Eskiden ikinci çağrı durum makinesini sıfırlıyordu; çipin o
    // anda meşgul olması hâlinde `readStatus()` başarısız olur ve BİRİNCİ
    // çağrının başarısı silinip iki sensör birden arızalı görünürdü.
    if (g_available)
    {
        return core::ErrCode::OK;
    }

    g_phase      = Phase::UNINITIALIZED;
    g_hasReading = false;
    g_available  = false;
    g_serviced   = false;
    g_errors     = 0;

    const ErrCode busRc = i2cbus::begin();
    if (busRc != ErrCode::OK)
    {
        return busRc;
    }

    uint8_t status = 0;
    if (!readStatus(status))
    {
        // Çip yok veya kablo kopuk. Log YOK: `SensorService` bu dönüş
        // değerini zaten alan adıyla loglar; iki kez loglamak olay
        // günlüğünü aynı arızayla doldururdu.
        return ErrCode::SENSOR_NOT_PRESENT;
    }

    if ((status & STATUS_CALIBRATED) == 0u)
    {
        // Fabrika kalibrasyonu yüklenmemiş — yükleme komutu gönderilir.
        // Yükleme ~10 ms sürer; BEKLEMİYORUZ, ilk ölçüm zaten en az bir
        // örnekleme periyodu sonra okunacak.
        if (!writeCommand3(CMD_INIT, CMD_INIT_ARG0, CMD_INIT_ARG1))
        {
            return ErrCode::SENSOR_NOT_PRESENT;
        }
    }

    g_available = true;
    g_phase     = Phase::IDLE;

    // İlk ölçümü şimdi tetikle: boot ile ilk örnekleme turu arasında geçen
    // süre dönüşüm süresinden çok uzundur, dolayısıyla ilk `sample()` hazır
    // bir değer bulur.
    triggerMeasurement(Millis{0});
    return ErrCode::OK;
}

void service(core::Millis now)
{
    if (!g_available || g_phase == Phase::UNINITIALIZED)
    {
        return;
    }

    // AYNI TURDA İKİNCİ ÇAĞRI İŞLEMSİZ. Sıcaklık ve nem sarmalayıcıları tek
    // çipi paylaşır; ikisi de aynı `now` ile buraya gelir. Korumasız kalsaydı
    // ikinci çağrı ölçümü yeniden tetikler ve dönüşüm hiç tamamlanmazdı.
    if (g_serviced && now == g_lastService)
    {
        return;
    }
    g_lastService = now;
    g_serviced    = true;

    if (g_phase == Phase::IDLE)
    {
        triggerMeasurement(now);
        return;
    }

    // --- MEASURING ---
    if (!core::hasElapsed(now, g_triggerAt, CONVERSION_TIME))
    {
        return;  // dönüşüm sürüyor
    }

    if (readMeasurement())
    {
        noteSuccess();
        // Sonucu aldık; bir sonraki turun hazır bir değer bulması için
        // ölçümü HEMEN yeniden tetikliyoruz.
        triggerMeasurement(now);
        return;
    }

    // Okuma başarısız (meşgul, CRC hatası veya yanıt yok).
    if (core::hasElapsed(now, g_triggerAt, MEASURE_TIMEOUT))
    {
        // Tetikleme kaybolmuş olabilir; kilitlenmeyi kır.
        noteError();
        g_phase = Phase::IDLE;
    }
}

bool hasReading()
{
    return g_hasReading;
}

float temperatureC()
{
    return g_tempC;
}

float humidityPct()
{
    return g_rhPct;
}

bool isAvailable()
{
    return g_available;
}

uint16_t errorCount()
{
    return g_errors;
}

} // namespace aht20
} // namespace hal
