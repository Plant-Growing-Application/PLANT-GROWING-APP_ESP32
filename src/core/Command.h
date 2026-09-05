#pragma once

// Komut modeli — TASK-008
//
// Komut, arayüzlerden (web, OLED) domain katmanına giden bir NİYET BİLDİRİMİDİR.
// Arayüzler eylemi kendileri yapmaz; komut üretir (ARCHITECTURE §13.2, §14.2).
//
// Bu, mevcut sistemdeki en tehlikeli desenin karşıtıdır: orada WebSocket
// handler'ı AsyncTCP task bağlamından doğrudan `digitalWrite(pin, ...)`
// çağırıyordu — güvenlik kontrolü olmadan, röle sürerek.
//
// TİP SAHİPLİĞİ (ISSUE-010): `CommandResult` burada tanımlanır. TASK-028 ve
// TASK-045 include eder, yeniden tanımlamaz.

#include <stdint.h>
#include <type_traits>

#include "SystemState.h"
#include "Time.h"

namespace core {

/// Komutu kim üretti? Tahkim (ARCHITECTURE §10.3) bu bilgiye dayanır:
/// MANUAL komut, AUTOMATION kararını süreli olarak geçersiz kılar.
enum class CommandSource : uint8_t
{
    SYSTEM = 0,  ///< iç kaynak (supervisor, boot)
    WEB    = 1,
    UI     = 2,  ///< OLED + encoder
};

/// Komut tipleri — KAPALI küme. Serbest metin komut yoktur.
///
/// Yalnızca ARCHITECTURE'da belgelenmiş eylemler tanımlıdır (P7):
/// §14.3 API sözleşmesi, §10.3 tahkim, §8.2 ağ yönetimi.
enum class CommandType : uint8_t
{
    NONE = 0,

    /// Aktüatör aç/kapa. target = ActuatorId, param = 0 (kapat) / 1 (aç)
    SET_ACTUATOR = 1,

    /// Otomasyon modu. param = AutomationMode değeri
    SET_AUTOMATION_MODE = 2,

    /// Acil durumu temizle. Koşullar düzelmemişse reddedilir (§12.2).
    /// (Acil durdurmanın KENDİSİ kuyruktan geçmez — bkz. CommandQueue.)
    EMERGENCY_CLEAR = 3,

    /// Kontrollü yeniden başlatma (§14.3)
    SYSTEM_RESTART = 4,

    /// Fabrika ayarları — config + sırlar + geçmiş silinir (§14.3)
    FACTORY_RESET = 5,

    /// Konfigürasyon değişti, yeniden yükle ve uygula (§14.3)
    CONFIG_RELOAD = 6,

    /// Wi-Fi taraması başlat (§8.2)
    NETWORK_SCAN = 7,

    /// Backoff'u atlayıp hemen bağlanmayı dene (§8.2 — kullanıcı sorunu
    /// düzelttiğinde 60 sn beklemek zorunda kalmamalı)
    NETWORK_RETRY_NOW = 8,

    /// Kayıtlı Wi-Fi bilgilerini sil (§8.2 "ağı unut")
    NETWORK_FORGET = 9,

    /// Ürün profilini uygula — OLED'den ürün seçimi. target = `CropId`
    ///
    /// ── NEDEN KOMUT, NEDEN DOĞRUDAN ÇAĞRI DEĞİL ────────────────────────────
    /// Profil uygulamak kural kümesini YENİDEN YAZAR ve NVS'e dokunur. `ui`
    /// task'i bunu kendi bağlamında yapsaydı, flash yazımı boyunca ekran ve
    /// girdi donar; üstelik `app_core` aynı anda kuralları okuyor olabilirdi.
    /// Tek yazar kuralı (P1) gereği yol kuyruktan geçer.
    ///
    /// Dönem, yoğunluk ve dikim tarihi TAŞINMAZ: tek encoder ile tarih
    /// girilmez (bkz. `Navigation.h`). `app_core` makul varsayılanları
    /// uygular; ince ayar web arayüzünün işidir.
    CROP_APPLY = 10,
};

/// Komut sonucu — ARCHITECTURE §10.4 sözleşmesi.
///
/// Sessiz başarısızlık yoktur: her komut açık bir sonuç üretir ve bu sonuç
/// hem API yanıtına, hem WebSocket ack'ine, hem OLED geri bildirimine dönüşür.
enum class CommandResult : uint8_t
{
    ACCEPTED             = 0,  ///< uygulandı
    NO_CHANGE            = 1,  ///< zaten istenen durumda
    REJECTED_SAFETY      = 2,  ///< güvenlik kilidi engelledi
    REJECTED_MODE        = 3,  ///< mevcut modda geçersiz
    REJECTED_INVALID     = 4,  ///< şema/parametre doğrulaması başarısız (§14.5)
    DEFERRED_MIN_RUNTIME = 5,  ///< min çalışma süresi dolmadı, şimdi kapatılamaz
    DEFERRED_COOLDOWN    = 6,  ///< bekleme süresi dolmadı, şimdi açılamaz
    BUSY                 = 7,  ///< komut kuyruğu dolu (§2.2)
};

constexpr bool isAccepted(CommandResult r)
{
    return r == CommandResult::ACCEPTED || r == CommandResult::NO_CHANGE;
}

/// Tek bir komut — sabit boyutlu POD, kuyrukta değer olarak taşınır.
struct Command
{
    Millis        issuedAt;  ///< monotonik — eskime kontrolü için (Karar 3)
    uint32_t      reqId;     ///< ack eşleştirmesi (§14.2); 0 = ack beklenmiyor
    int32_t       param;     ///< tipe göre anlam kazanır
    CommandType   type;
    uint8_t       target;    ///< tipe göre: ActuatorId, SensorId, ...
    CommandSource source;
    uint8_t       reserved;
};

/// Komut kuyrukta çok beklediyse artık uygulanmamalıdır: bu arada güvenlik
/// durumu değişmiş olabilir. Eşik ve düşürme politikası TASK-033'e aittir.
constexpr bool isStale(const Command& c, Millis now, Duration maxAge)
{
    return hasElapsed(now, c.issuedAt, maxAge);
}

// ---------------------------------------------------------------------------
// Derleme zamanı doğrulama
// ---------------------------------------------------------------------------

static_assert(std::is_trivially_copyable<Command>::value,
              "Command trivially copyable olmali (kuyrukta deger olarak tasinir)");
static_assert(std::is_standard_layout<Command>::value, "Command standard layout olmali");
static_assert(sizeof(Command) <= 20, "Command 20 bayti asmamali");

} // namespace core
