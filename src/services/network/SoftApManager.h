#pragma once

// SoftAP yönetimi ve AP fallback — TASK-038
//
// ── ESKİ DAVRANIŞIN EKSİĞİ ──────────────────────────────────────────────────
//   Eski: yalnızca boot'ta bir kez, bağlantı başarısızsa AP açılıyordu.
//         Çalışma sırasında ağ kalıcı koparsa AP AÇILMIYORDU → cihaza erişim yok.
//   Yeni: kalıcı kopmada AP otomatik açılır, STA denemesi arka planda sürer,
//         ağ geri gelince otomatik STA'ya dönülür.
//
// ── AP ŞİFRESİ: MAC'TEN TÜRETİLMEZ ──────────────────────────────────────────
// SoftAP kendi BSSID'sini YAYINLAR ve bu cihazın MAC adresidir. Şifre
// MAC'ten türetilseydi menzildeki HERKES onu hesaplayabilirdi — "cihaza özgü"
// olmak onu GİZLİ yapmaz.
//
// İlk boot'ta `esp_random()` ile üretilir, `SecretStore`'a yazılır, OLED'de
// gösterilir. Kullanıcı ekrandan okur: aynı sıfırdan-yapılandırma deneyimi,
// gerçek gizlilikle.
//
// SSID **MAC'ten türetilir** ve bu doğrudur: SSID'nin gizli olması gerekmez,
// aynı ortamdaki cihazlardan ayırt edilebilir olması gerekir.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace services {
namespace net {
namespace softap {

/// Bu süre boyunca bağlantı kurulamazsa AP açılır.
///
/// Süre tabanlı ölçüt seçildi: "N deneme" ölçütü backoff yüzünden geçen
/// süreyi belirsiz kılar (3 deneme 7 sn de olabilir 90 sn de). Kullanıcı
/// deneyimi süreyle ölçülür: "1.5 dakikadır bağlanamıyorum".
constexpr uint32_t FALLBACK_AFTER_MS = 90000u;

/// STA bağlandıktan sonra AP'nin açık kalmaya devam edeceği asgari süre.
///
/// STA bağlanır bağlanmaz AP'yi kapatmak, AP'ye bağlı kullanıcıyı **tam da
/// ayarları kaydettiği anda** ortada bırakır.
constexpr uint32_t LINGER_MS = 30000u;

/// STA ayaga kalktiktan sonra AP'nin KESIN kapanma suresi.
///
/// `LINGER_MS` yalnizca "bagli istemci yokken" gecerlidir. Istemci bagli
/// kaldigi surece AP'yi acik tutmak SURESIZ olamaz:
///
///   kullanici telefonuyla kurulum AP'sine baglanir → Wi-Fi'yi kaydeder
///   → cihaz ev agina gecer → AMA telefon hala kurulum AP'sinde
///   → AP hic kapanmaz → telefon 192.168.4.x agında kalir
///   → kullanici cihazin EV AGINDAKI IP'sine ERISEMEZ
///
/// Sahada tam olarak bu yasandi: "IP'ye giriyorum, ariyor ariyor hata
/// veriyor". Kesin kapanma, kullaniciyi kendi agina geri iter.
constexpr uint32_t HARD_LINGER_MS = 90000u;

/// AP ağ aralığı: `192.168.4.1/24`.
///
/// Ev ağlarında yaygın `192.168.0.x` ve `192.168.1.x` ile ÇAKIŞMAZ —
/// çakışma, AP'ye bağlanan telefonun yönlendirme tablosunu bozar.
constexpr uint32_t AP_IP     = 0x0104A8C0u;   // 192.168.4.1 (little-endian ham)
constexpr uint32_t AP_SUBNET = 0x00FFFFFFu;   // 255.255.255.0

constexpr uint8_t SSID_MAX = 33;
constexpr uint8_t PASS_LEN = 10;   ///< üretilen şifre uzunluğu

core::ErrCode begin();

/// AP'yi açar. Şifre yoksa üretir ve saklar.
core::ErrCode start(core::Millis now);

/// AP'yi kapatır.
core::ErrCode stop();

bool active();

/// Cihaza özgü SSID (`Sera-AB12CD`). Gizli olması gerekmez.
const char* ssid();

/// AP şifresi — **yalnızca OLED kurulum ekranı** okumalıdır.
/// Web API'sine ve log'a girmez.
const char* password();

/// AP şu an kapatılabilir mi?
///
/// STA bağlı **ve** bağlı istemci yok **ve** `LINGER_MS` geçmiş olmalıdır.
bool canCloseNow(core::Millis now, bool staConnected);

/// AP'nin açılması gerekip gerekmediği — süre tabanlı fallback ölçütü.
bool shouldFallback(core::Millis now, core::Millis disconnectedSince);

uint8_t clientCount();

} // namespace softap
} // namespace net
} // namespace services
