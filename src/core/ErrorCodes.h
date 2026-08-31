#pragma once

// Hata kodu taksonomisi — TASK-004
//
// ARCHITECTURE §16.2: her hata `{subsystem, code}` çiftidir.
// Burada tek bir uint16 içinde kodlanır: üst bayt = Subsystem, alt bayt = kod.
//
//   0x0703  →  Subsystem::NET (0x07), kod 0x03
//
// KURAL: Karar mantığı YALNIZCA koda bakar. Serbest metin insan içindir.
// Hiçbir yerde hata metni karşılaştırılmaz (CODING_STANDARDS §4).
//
// KAPSAM DİSİPLİNİ (ARCHITECTURE P7): burada yalnızca ARCHITECTURE §16.3 arıza
// matrisinde ve REQUIREMENTS §9'da belgelenmiş arıza modları tanımlıdır.
// Spekülatif kod üretilmez — yeni kodu, ona ihtiyaç duyan task ekler.

#include <stdint.h>

#include "Types.h"

namespace core {

/// Alt sistem kimliğini hata kodunun üst baytına yerleştirir.
constexpr uint16_t errBase(Subsystem s)
{
    return static_cast<uint16_t>(static_cast<uint16_t>(s) << 8);
}

enum class ErrCode : uint16_t
{
    OK = 0x0000,

    // --- SYS: boot, mod, watchdog, supervisor -----------------------------
    SYS_BOOT_STAGE_FAILED   = 0x0101,  ///< bir boot aşaması başarısız (§7.1)
    SYS_WATCHDOG_RESET      = 0x0102,  ///< önceki oturum WDT ile sonlandı (§16.3)
    SYS_TASK_CREATE_FAILED  = 0x0103,  ///< task oluşturulamadı (§6)
    SYS_TASK_HEARTBEAT_LOST = 0x0104,  ///< task heartbeat bayatladı (§16.3)
    SYS_LOW_HEAP            = 0x0105,  ///< heap kritik seviyede (§16.3)

    // --- CFG: konfigürasyon -----------------------------------------------
    CFG_NOT_FOUND        = 0x0201,  ///< kayıt yok → varsayılan (§15.3)
    CFG_CORRUPT          = 0x0202,  ///< kayıt bozuk → varsayılan (§15.3)
    CFG_VERSION_NEWER    = 0x0203,  ///< firmware geri alınmış (§15.3)
    CFG_VALIDATION_FAILED = 0x0204, ///< alan aralık dışı veya tutarsız (§14.5)

    // --- STORAGE: NVS, LittleFS, geçmiş -----------------------------------
    STORAGE_NVS_INIT_FAILED = 0x0301,  ///< NVS bölümü açılamadı
    STORAGE_FS_MOUNT_FAILED = 0x0302,  ///< LittleFS mount edilemedi (§16.3)
    STORAGE_WRITE_FAILED    = 0x0303,  ///< yazma başarısız (§16.3)
    STORAGE_FULL            = 0x0304,  ///< alan dolu (§16.3)
    STORAGE_RECORD_CORRUPT  = 0x0305,  ///< bozuk kayıt tespit edildi (TASK-058)

    // --- SENSOR: okuma ve işleme hattı ------------------------------------
    SENSOR_NOT_PRESENT   = 0x0401,  ///< donanımda takılı değil (§9.3)
    SENSOR_OPEN_CIRCUIT  = 0x0402,  ///< kopuk — ADC uçta sabit (§9.5)
    SENSOR_SHORT_CIRCUIT = 0x0403,  ///< kısa devre — ADC uçta sabit (§9.5)
    SENSOR_OUT_OF_RANGE  = 0x0404,  ///< yapılandırılmış aralık dışı (§9.5)
    SENSOR_STALE         = 0x0405,  ///< N örnek boyunca değişmiyor (§9.5)
    SENSOR_IMPLAUSIBLE   = 0x0406,  ///< fiziksel olmayan sıçrama (§9.5)
    SENSOR_FLOW_NO_PULSE = 0x0407,  ///< akış sensöründen darbe gelmiyor (§16.2)

    // --- ACTUATOR: röleler ve kısıtlar ------------------------------------
    ACTUATOR_MIN_RUNTIME    = 0x0501,  ///< min çalışma süresi dolmadı (§10.4)
    ACTUATOR_COOLDOWN       = 0x0502,  ///< bekleme süresi dolmadı (§10.4)
    ACTUATOR_MAX_RUNTIME    = 0x0503,  ///< maks süre aşıldı, zorla kapatıldı (§10.4)
    ACTUATOR_STATE_MISMATCH = 0x0504,  ///< talep ile gerçek pin durumu farklı (§2.6)

    // --- SAFETY: kilitler ve acil durum -----------------------------------
    SAFETY_LEVEL_INSUFFICIENT = 0x0601,  ///< su seviyesi yetersiz (§12.1)
    SAFETY_LEVEL_SENSOR_FAULT = 0x0602,  ///< seviye sensörü okunamıyor → fail-safe (§12.2)
    SAFETY_DRY_RUN            = 0x0603,  ///< kuru çalışma tespit edildi (§16.2)
    SAFETY_FLOW_VERIFY_FAILED = 0x0604,  ///< akış doğrulama başarısız (§12.1)
    SAFETY_EMERGENCY_LATCHED  = 0x0605,  ///< acil durum mandalı aktif (§12.1)
    SAFETY_BLOCKED            = 0x0606,  ///< komut güvenlik vetosuyla reddedildi (§10.4)

    // --- NET: Wi-Fi --------------------------------------------------------
    NET_NO_CREDENTIALS = 0x0701,  ///< kayıtlı SSID yok → AP_ONLY (§8.1)
    NET_AUTH_FAILED    = 0x0702,  ///< kimlik doğrulama hatası (§16.2, §8.2)
    NET_AP_NOT_FOUND   = 0x0703,  ///< AP kapsama dışı (§8.2)
    NET_DISCONNECTED   = 0x0704,  ///< bağlantı koptu (§8.2)
    NET_CONNECT_TIMEOUT = 0x0705, ///< bağlantı zaman aşımı (§8.2)
    NET_SCAN_FAILED    = 0x0706,  ///< tarama başarısız (§8.2)
    NET_IP_CONFIG_INVALID = 0x0707, ///< static IP alanları eksik/geçersiz (§8.2)

    // --- TIME: SNTP --------------------------------------------------------
    TIME_NOT_SYNCED  = 0x0801,  ///< zaman geçersiz → çizelgeler duraklatıldı (§11.2)
    TIME_SYNC_FAILED = 0x0802,  ///< SNTP senkronizasyonu başarısız (§2.13)

    // --- WEB: HTTP, WebSocket, API ----------------------------------------
    WEB_UNAUTHORIZED    = 0x0901,  ///< yetkisiz istek (§14.4)
    WEB_INVALID_REQUEST = 0x0902,  ///< şema doğrulaması başarısız (§14.5)
    WEB_BUSY            = 0x0903,  ///< komut kuyruğu dolu (§2.2)
    WEB_PAYLOAD_TOO_LARGE = 0x0904, ///< istek gövdesi sınır aşımı (§14.5)

    // --- UI: OLED ve girdi --------------------------------------------------
    UI_DISPLAY_UNAVAILABLE = 0x0A01,  ///< OLED başlatılamadı/koptu (§16.3)
    UI_INPUT_QUEUE_FULL    = 0x0A02,  ///< girdi olay kuyruğu taştı (TASK-021)
};

/// Hata kodundan alt sistemi çıkarır.
constexpr Subsystem subsystemOf(ErrCode e)
{
    return static_cast<Subsystem>(static_cast<uint16_t>(e) >> 8);
}

/// Hata var mı? (`OK` dışındaki her şey hatadır.)
constexpr bool isError(ErrCode e)
{
    return e != ErrCode::OK;
}

// Kodlamanın tutarlılığı: alt sistem baytı doğru çıkarılıyor mu?
static_assert(subsystemOf(ErrCode::NET_AUTH_FAILED) == Subsystem::NET,
              "ErrCode ust bayti Subsystem ile eslesmeli");
static_assert(subsystemOf(ErrCode::SAFETY_DRY_RUN) == Subsystem::SAFETY,
              "ErrCode ust bayti Subsystem ile eslesmeli");
static_assert(errBase(Subsystem::SENSOR) == 0x0400, "errBase kodlama hatasi");

} // namespace core
