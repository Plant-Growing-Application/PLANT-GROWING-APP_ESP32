#pragma once

// Ağ bağlantı durum makinesi — TASK-035
//
// ── FSM (ARCHITECTURE §8.1) ─────────────────────────────────────────────────
//
//                        ┌──────────────┐
//                        │     BOOT     │
//                        └──────┬───────┘
//                       credential var?
//              hayir ┌──────────┴────────┐ evet
//                    ▼                   ▼
//            ┌─────────────┐      ┌──────────────┐
//            │  AP_ONLY    │      │  CONNECTING  │◀────────┐
//            └──────┬──────┘      └──────┬───────┘         │
//                   │           basari ┌─┴─┐ basarisiz     │
//                   │                  ▼   ▼               │
//                   │        ┌───────────┐ ┌──────────────┐│
//                   │        │ CONNECTED │ │   BACKOFF    │┘
//                   │        └─────┬─────┘ └──────┬───────┘
//                   │       kopma  │              │ 5 sn gecti
//                   │              └──────────────┤
//                   │                             ▼
//                   │                   ┌────────────────────┐
//                   └──────────────────▶│    AP_FALLBACK     │
//                                       └────────────────────┘
//
// ── BLOKLAMA YASAĞI — MUTLAK ────────────────────────────────────────────────
// Hiçbir durumda `while (WiFi.status() != WL_CONNECTED) delay(...)` benzeri
// bir bekleme YOKTUR. `CONNECTING`'de task normal periyoduyla döner; sonuç
// event olarak gelir.
//
// Eski sistemde `connect(5000)` task'ı 5 saniye blokluyordu — ve bu, watchdog
// beslemesinden SONRA yapıldığı için watchdog tarafından da görülmüyordu.
//
// ── BAĞIMSIZLIK ─────────────────────────────────────────────────────────────
// `net` task'ı tamamen kilitlense bile aktüatör kontrolü ve güvenlik
// kilitleri çalışmaya devam eder: bu modül `StateStore`'a yalnızca YAZAR,
// `app_core` ondan bağımsız çalışır (ARCHITECTURE §16.3).

// ── İLK KURULUM: BAĞLANDIKTAN SONRA KONTROLLÜ YENİDEN BAŞLATMA ──────────────
//
// Kurulum AP'sinde Wi-Fi bilgisi girildiğinde cihaz `AP_STA` moduna geçer ve
// ev ağına bağlanır. ESKİ DAVRANIŞ BURADA BİTİYORDU ve kullanıcı ortada
// kalıyordu:
//
//   • telefon hâlâ `Sera-XXXX` kurulum ağındaydı; AP linger boyunca (30-90 sn)
//     açık kaldığı için hangi ağın "gerçek" olduğu belirsizdi,
//   • cihazın ev ağındaki YENİ adresini söyleyen kimse yoktu,
//   • radyo `AP_STA`'da kalıyordu: iki arayüz, tek anten, düşük verim,
//   • kurulum sırasında ayağa kalkmış her şey (SNTP, WebSocket, mDNS) yarı
//     yapılandırılmış hâlde çalışmaya devam ediyordu.
//
// Yeni davranış — kurulum oturumunda İLK kez IP alındığında:
//
//   1. durum yayınlanır: arayüz "bağlandı, adres X, cihaz yeniden başlıyor"
//      diyebilsin (telefon hâlâ AP'ye bağlıyken görür),
//   2. config'in flash'a yazılması İSTENİR ve BEKLENİR — yazılmadan reset
//      atmak SSID'yi siler ve cihazı kurulum AP'sine geri düşürür,
//   3. `SYSTEM_RESTART` komutu kuyruğa konur: reset'i `app_core` yapar,
//      aktüatörler önce güvenli duruma alınır (§14.3).
//
// Yeniden başlayan cihaz `BOOT → CONNECTING → CONNECTED` yolunu izler: AP
// hiç açılmaz, radyo saf `STA` olur. Kurulum, tanımlı bir noktada BİTER.
//
// NEDEN AP FALLBACK'TE DEĞİL: orada kayıtlı ve daha önce çalışmış bir ağ
// vardır; sorun geçici bir kopmadır. Her bağlantı dönüşünde reset atmak,
// sinyali zayıf bir kurulumu sonsuz yeniden başlatma döngüsüne sokardı.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/Time.h"
#include "services/network/NetworkState.h"

namespace services {
namespace net {
namespace fsm {

/// Kurulum başarıyla bittikten sonra reset'ten ÖNCE beklenen nefes payı.
///
/// Bu süre kullanıcı içindir, teknik bir gereklilik değildir: telefonu hâlâ
/// kurulum AP'sindeyken "bağlandı, adres bu, şimdi kendi ağına geç" mesajını
/// GÖRMESİ gerekir. Reset anında atılsaydı tarayıcı yalnızca kopmuş bir
/// bağlantı görürdü — kurulumun başarılı olduğunu anlamanın yolu kalmazdı.
constexpr uint32_t SETUP_REBOOT_GRACE_MS = 4000u;

/// Config'in flash'a yazılmasını bekleme üst sınırı.
///
/// `ConfigService` yazmayı 2 sn geciktirir (debounce) ve iş `store` task'ında
/// yapılır. Yazma bitmeden reset atmak SSID'yi kaybettirir; bu yüzden
/// beklenir. Süre dolarsa yeniden başlatma İPTAL EDİLİR — kimlik bilgisini
/// kaybetmektense AP'yi açık bırakmak yeğdir.
constexpr uint32_t SETUP_REBOOT_MAX_WAIT_MS = 20000u;

/// Alt modülleri (radyo, bağlantı, AP, tarama) başlatır.
core::ErrCode begin(const core::Config& cfg);

/// Bir FSM çevrimi: olayları tüket → zamanlayıcıları kontrol et → yayınla.
void tick(core::Millis now);

/// Kullanıcının "şimdi dene" komutu — backoff'u **ve** kimlik hatası
/// durdurmasını atlar. Kullanıcı sorunu düzelttiğinde 60 saniye beklemek
/// zorunda kalmamalı.
void requestRetryNow();

/// "Ağı unut" isteği — bayrak koyar, işi `net` task'ı kendi bağlamında yapar.
///
/// `app_core` radyoya DOKUNAMAZ (P2) ve AsyncTCP callback'i flash yazamaz
/// (§14.6); bu yüzden komut bir bayrağa çevrilir.
void requestForget();

/// Yeni credential girildi: kimlik hatası sayacı ve durdurma sıfırlanır.
///
/// Aksi hâlde kullanıcı şifreyi düzeltir ama sistem hâlâ durmuş olur — ve
/// bunu anlamanın hiçbir yolu olmaz.
void onCredentialsChanged();

core::NetState state();

/// Kurulum oturumu sürüyor mu? (AP kimlik bilgisi olmadığı için açıldı)
bool provisioning();

/// Kurulum tamamlandı ve kontrollü yeniden başlatma bekleniyor mu?
///
/// Sunum katmanı bunu kullanıcıya söylemek için okur: sessizce reset atan bir
/// cihaz, bozulmuş bir cihazdan ayırt edilemez.
bool setupRebootPending();

/// Bir sonraki bağlanma denemesine kalan süre (ms). Bekleyen deneme yoksa 0.
uint32_t retryInMs(core::Millis now);

/// Yeniden başlatmaya kalan süre (ms). Beklenen bir reset yoksa 0.
uint32_t rebootInMs(core::Millis now);

/// İç durum — tanılama ve host testi için.
const NetworkRuntime& runtime();

} // namespace fsm
} // namespace net
} // namespace services
