#pragma once

// Merkezî sistem durumu — TASK-006
//
// Sistemin TÜM gözlemlenebilir durumu tek bir POD yapıda toplanır. Bu yapı
// `StateStore` (TASK-007) tarafından mutex altında yayınlanır ve okuyuculara
// atomik bir kopya (snapshot) olarak dağıtılır.
//
// POD ZORUNLULUĞU: snapshot deseni `memcpy` ile kopyalama gerektirir. İşaretçi,
// `String`, sanal fonksiyon veya dinamik dizi buraya GİREMEZ.
//
// TEK YAZAR KURALI (ARCHITECTURE P1): her alt-state'in tam olarak bir sahip
// task'ı vardır; aşağıda her yapının başında belirtilmiştir. Başka bir task
// o alt-state'e yazarsa bu bir katman ihlalidir.
//
// TİP SAHİPLİĞİ (ISSUE-010): burada YAYINLANAN state'in tipleri tanımlanır.
// Sonraki task'lar çalışma/konfigürasyon tiplerini tanımlar ve bu dosyayı
// include eder — yeniden tanımlamaz.

#include <stdint.h>
#include <type_traits>

#include "ErrorCodes.h"
#include "Time.h"
#include "Types.h"

namespace core {

// ---------------------------------------------------------------------------
// Kapasiteler — derleme zamanı sabitleri
// ---------------------------------------------------------------------------
constexpr uint8_t MAX_SENSORS   = 8;  ///< REQUIREMENTS §3'te 6 sensör + büyüme payı

/// Fiziksel röle sayısı. **Her slotun bir pini vardır** (TASK-066): eşlenmemiş
/// mantıksal aktüatör bırakmıyoruz. Eskiden `AUX_1`/`AUX_2` slotları vardı ama
/// `RelayOutput` onları `PIN_UNMAPPED` olarak reddediyordu — arayüzde açılıp
/// hiçbir şey yapmayan iki anahtar demekti (ARCHITECTURE P7'nin ruhu).
///
/// Beş, kartta kalan güvenli çıkış sayısıyla SINIRLIDIR (`BoardPins.h`).
/// Altıncı bir röle GPIO genişletici gerektirir; slot sayısını burada
/// artırmak tek başına bir işe yaramaz.
constexpr uint8_t MAX_ACTUATORS = 5;

/// Heap bu değerin altına düşerse sistem KENDİNİ KISAR (ARCHITECTURE §16.3):
/// telemetri hızı düşürülür, geçmiş yazımı duraklatılır.
///
/// Wi-Fi + AsyncTCP çalışırken tipik boş heap 120–180 KB. 32 KB'ın altında
/// TCP tamponu ve TLS olmayan bağlantı tahsisleri başarısız olmaya başlar;
/// çökmeyi beklemek yerine önce ÖNEMSİZ işleri bırakıyoruz.
///
/// Eşik `core/` içinde: hem `domain/` (izleme) hem `services/` (geçmiş) hem
/// `interfaces/` (telemetri) okuyor ve hiçbiri diğerine bağımlı olmuyor.
constexpr uint32_t LOW_HEAP_BYTES = 32768u;  ///< su pompası + hava pompası + 2 yedek

// ---------------------------------------------------------------------------
// Sistem modu — ARCHITECTURE §7.2
// ---------------------------------------------------------------------------
enum class SystemMode : uint8_t
{
    BOOTING   = 0,
    RUNNING   = 1,  ///< tüm aşamalar başarılı
    DEGRADED  = 2,  ///< kritik olmayan aşama başarısız; kalan işlevler çalışır
    SAFE      = 3,  ///< aktüatörler kapalı ve kilitli; yalnızca teşhis/kurulum
    EMERGENCY = 4,  ///< mandallı güvenlik ihlali; operatör onayı gerekir
};

// ---------------------------------------------------------------------------
// Sensörler
// ---------------------------------------------------------------------------

/// Sensör kimlikleri. Kararlıdır: API ve config'te kullanılır, sıraya bağlı
/// indeks yerine açık değer taşır.
/// Sayısal değer `Config::sensors[]` dizisindeki İNDEKSTİR (`SensorService`
/// `configIndexOf()` ile doğrudan dönüştürür). Bu yüzden değerler 0'dan
/// başlar, boşluksuz ilerler ve `MAX_SENSORS`'ın altında kalmak ZORUNDADIR.
enum class SensorId : uint8_t
{
    WATER_TEMP  = 0,
    WATER_FLOW  = 1,
    PH          = 2,
    EC          = 3,
    WATER_LEVEL = 4,
    HUMIDITY    = 5,  ///< ortam nemi — AHT20 (I2C)
    AMBIENT_TEMP = 6, ///< ortam (hava) sıcaklığı — AHT20 (I2C), TASK-066
    LIGHT        = 7, ///< aydınlık düzeyi (lüks) — BH1750 (I2C), TASK-066
    NONE        = 0xFF,
};

// Sensör kimlikleri config indeksidir: en büyüğü diziye SIĞMALI.
static_assert(static_cast<uint8_t>(SensorId::LIGHT) < MAX_SENSORS,
              "SensorId degeri Config::sensors[] disina tasiyor");

/// Ölçümün güvenilirliği. Değer ASLA kalitesiz taşınmaz (CODING_STANDARDS Z6):
/// mevcut sistemde `WaterTemprature = 0` değerinin gerçek ölçüm mü kopuk sensör
/// mü olduğu ayırt edilemiyordu — güvenlik kararı veren bir sensörde bu hayati.
enum class SensorQuality : uint8_t
{
    OK            = 0,  ///< geçerli ölçüm — otomasyonda kullanılabilir
    STALE         = 1,  ///< N örnek boyunca değişmiyor — uyarıyla kullanılır
    OUT_OF_RANGE  = 2,  ///< yapılandırılmış aralık dışı — kullanılmaz
    FAULT         = 3,  ///< kopuk/kısa/okunamıyor — kullanılmaz
    NOT_PRESENT   = 4,  ///< bu donanımda takılı değil — "yok" olarak gösterilir
};

/// Otomasyon ve güvenlik kararlarında kullanılabilir mi?
constexpr bool isUsable(SensorQuality q)
{
    return q == SensorQuality::OK;
}

/// Tek bir sensörün yayınlanan durumu.
struct SensorSample
{
    Millis        timestamp;  ///< monotonik — bayatlama hesabı için
    float         value;      ///< işlenmiş değer (int DEĞİL — hassasiyet korunur)
    SensorId      id;
    SensorQuality quality;
    uint16_t      faultCount;  ///< bu sensörde biriken hata sayısı (teşhis)
};

// ---------------------------------------------------------------------------
// Aktüatörler
// ---------------------------------------------------------------------------

/// Mantıksal aktüatör kimliği. Fiziksel röle eşlemesi konfigürasyondadır
/// (ARCHITECTURE §10.1) — donanım değişince kod değil config değişir.
/// Sayısal değer `Config::actuators[]` indeksidir ve `RelayOutput`'un pin
/// tablosunda aynı sırayı taşır — kimlik ile fiziksel eşleme arasında tek bir
/// doğruluk kaynağı olsun diye.
enum class ActuatorId : uint8_t
{
    WATER_PUMP    = 0,
    AIR_PUMP      = 1,
    GROW_LIGHT    = 2,  ///< büyütme ışığı — ışık penceresi kuralı (TASK-066)
    HEATER        = 3,  ///< besin çözeltisi ısıtıcısı — sıcaklık eşiği (TASK-066)
    NUTRIENT_PUMP = 4,  ///< besin dozaj pompası — EC eşiği (TASK-066)
    NONE          = 0xFF,
};

static_assert(static_cast<uint8_t>(ActuatorId::NUTRIENT_PUMP) < MAX_ACTUATORS,
              "ActuatorId degeri Config::actuators[] disina tasiyor");

/// Aktüatörün mevcut durumunu kim belirledi? Tahkim sırası (ARCHITECTURE §10.3):
/// SAFETY > MANUAL > AUTOMATION.
enum class ControlSource : uint8_t
{
    NONE       = 0,
    AUTOMATION = 1,
    MANUAL     = 2,
    SAFETY     = 3,
};

/// Tek bir aktüatörün yayınlanan durumu.
struct ActuatorStatus
{
    Millis        lastChange;    ///< son durum değişimi (monotonik)
    uint32_t      totalRunMs;    ///< toplam çalışma süresi (bakım göstergesi)
    uint16_t      cycleCount;    ///< açılma sayısı
    ErrCode       blockReason;   ///< açılamıyorsa nedeni; OK = engel yok
    ActuatorId    id;
    ControlSource source;
    uint8_t       isOn;          ///< GERÇEK pin durumu (talep edilen değil)
    uint8_t       reserved;
};

// ---------------------------------------------------------------------------
// Ağ
// ---------------------------------------------------------------------------

/// Wi-Fi durum makinesi — ARCHITECTURE §8.1
enum class NetState : uint8_t
{
    BOOT        = 0,
    AP_ONLY     = 1,  ///< credential yok → kurulum modu
    CONNECTING  = 2,
    CONNECTED   = 3,
    BACKOFF     = 4,  ///< üstel bekleme, yeniden denenecek
    AP_FALLBACK = 5,  ///< AP açık, STA denemesi arka planda sürüyor
};

// ---------------------------------------------------------------------------
// Otomasyon
// ---------------------------------------------------------------------------

/// Çalışma modu. Varsayılan MANUAL: ilk açılışta sistem kendiliğinden sulamaz
/// (güvenli varsayılan — TASK-014).
enum class AutomationMode : uint8_t
{
    MANUAL = 0,
    AUTO   = 1,
};

// ---------------------------------------------------------------------------
// Alt-state yapıları
// ---------------------------------------------------------------------------

/// Sahip: `app_core` task'ı (SystemSupervisor — TASK-012)
struct SystemStatus
{
    uint32_t   uptimeMs;
    uint32_t   freeHeapBytes;
    uint32_t   minFreeHeapBytes;  ///< açılıştan beri görülen en düşük değer
    uint16_t   faultSubsystemMask;  ///< hangi alt sistemlerde aktif hata var
    SystemMode mode;
    uint8_t    activeFaultCount;
    uint8_t    bootFailedStages;
    uint8_t    resetReason;  ///< core::ResetReason değeri
};

/// Sahip: `net` task'ı (NetworkService — TASK-035)
///
/// Wi-Fi ŞİFRESİ BURADA YOK ve asla eklenmeyecek (ARCHITECTURE §8.2).
struct NetworkStatus
{
    Millis   connectedSince;
    Millis   nextRetryAt;
    uint32_t ipv4;     ///< ham adres; metne çevirme sunum katmanının işi
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
    FixedString<32> ssid;
    uint8_t  mac[6];
    ErrCode  lastError;
    NetState state;
    int8_t   rssi;      ///< dBm; bağlı değilken 0
    uint8_t  apActive;
    uint8_t  apClients;
    uint8_t  retryCount;
    uint8_t  usingStaticIp;

    // ── İLK KURULUM (§8.4) ─────────────────────────────────────────────────
    // Kurulum, "bağlandı"dan farklı bir durumdur: cihaz kurulum AP'sini
    // kimlik bilgisi olmadığı için açmıştır ve bağlantı kurulunca kontrollü
    // biçimde yeniden başlayacaktır. Kullanıcı bunu BİLMELİDİR — habersiz
    // yeniden başlayan bir cihaz, bozulmuş bir cihazdan ayırt edilemez.
    uint8_t  provisioning;   ///< kurulum oturumu sürüyor
    uint8_t  setupReboot;    ///< kurulum bitti, kontrollü reset bekleniyor
    uint8_t  rebootIn;       ///< reset'e kalan saniye; 0 = her an

    /// Bir sonraki bağlanma denemesine kalan saniye; 0 = bekleyen deneme yok.
    ///
    /// `nextRetryAt` cihazın monotonik saatindedir ve arayüz onu tek başına
    /// yorumlayamaz. Sessiz bekleme kullanıcıya "bozuk" izlenimi verir;
    /// sayılan bir bekleme vermez.
    uint8_t  retryIn;
};

/// Sahip: `io_sense` task'ı (SensorService — TASK-027)
struct SensorsStatus
{
    SensorSample samples[MAX_SENSORS];
    uint8_t      count;
    uint8_t      reserved[3];
};

/// Sahip: `app_core` task'ı (ActuatorManager — TASK-029)
struct ActuatorsStatus
{
    ActuatorStatus items[MAX_ACTUATORS];
    uint8_t        count;
    uint8_t        reserved[3];
};

/// Sahip: `app_core` task'ı (SafetyMonitor — TASK-030)
struct SafetyStatus
{
    Millis   latchedAt;
    uint32_t interlockMask;    ///< aktif kilitler; anlamını TASK-030 tanımlar
    ErrCode  emergencyReason;
    uint8_t  emergencyLatched;
    uint8_t  reserved;
};

/// Sahip: `app_core` task'ı (AutomationEngine — TASK-057)
struct AutomationStatus
{
    EpochSeconds   nextScheduleAt;  ///< duvar saati — yalnızca gösterim/çizelge
    Duration       overrideRemaining;
    AutomationMode mode;
    uint8_t        activeRuleId;
    uint8_t        schedulesPaused;  ///< zaman geçersiz → çizelgeler duraklatıldı
    uint8_t        reserved;
};

/// Sahip: `net` task'ı (TimeService — TASK-040)
struct TimeStatus
{
    EpochSeconds epoch;      ///< duvar saati; geçersizken EPOCH_INVALID
    Millis       lastSyncAt;
    uint8_t      valid;      ///< 0 = senkronize değil → çizelgeler çalışmaz
    uint8_t      reserved[3];
};

// ---------------------------------------------------------------------------
// Kök yapı
// ---------------------------------------------------------------------------

/// Sistemin tam durumu. `StateStore` (TASK-007) tarafından yönetilir.
///
/// `version` her yayınlamada artar; okuyucular değişim tespiti için kullanır
/// (ARCHITECTURE §4.2): web gereksiz WS trafiği üretmez, UI ekranı gereksiz
/// yeniden çizmez.
struct SystemState
{
    uint32_t         version;
    SystemStatus     system;
    NetworkStatus    network;
    SensorsStatus    sensors;
    ActuatorsStatus  actuators;
    SafetyStatus     safety;
    AutomationStatus automation;
    TimeStatus       time;
};

// ---------------------------------------------------------------------------
// Derleme zamanı doğrulama
// ---------------------------------------------------------------------------

static_assert(std::is_trivially_copyable<SystemState>::value,
              "SystemState trivially copyable olmali (StateStore snapshot memcpy)");
static_assert(std::is_standard_layout<SystemState>::value,
              "SystemState standard layout olmali");

// Boyut bütçesi: snapshot her okumada kopyalanacak (UI 20 Hz, web 1 Hz,
// app_core 10 Hz). 512 baytlık memcpy ~1 us mertebesindedir ve TASK-007'nin
// 10 us kritik bolge hedefine sigar.
static_assert(sizeof(SystemState) <= 512, "SystemState 512 bayti asmamali");

// Alt-state boyutlarının makul kalması
static_assert(sizeof(SensorSample) <= 16, "SensorSample 16 bayti asmamali");
static_assert(sizeof(ActuatorStatus) <= 20, "ActuatorStatus 20 bayti asmamali");

} // namespace core
