#pragma once

// Aktüatörlerin TEK SAHİBİ — TASK-029
//
// ── TEK KAPI KURALI (ARCHITECTURE P2) ───────────────────────────────────────
// `hal::relay::set()` ve `hal::relay::allSafe()` çağrıları YALNIZCA
// `ActuatorManager.cpp` içinde bulunur ve yalnızca `app_core` task'ından
// çalışır. Web, UI, otomasyon veya başka bir servis röleye erişemez.
//
// Eski sistemde WebSocket handler'ı doğrudan `digitalWrite(pin, ...)` yapıyordu:
// AsyncTCP task bağlamından, güvenlik kontrolü olmadan pompa sürülüyordu
// (REQUIREMENTS Kritik Problem 2). Bu yol artık yapısal olarak kapalıdır.
//
// ── NİYET / GERÇEK AYRIMI ───────────────────────────────────────────────────
// `request()` yalnızca NİYET kaydeder. Fiziksel yazma `apply()` içinde olur.
// Kısıt nedeniyle ertelenen bir talep DÜŞMEZ, mandallanır ve her döngüde
// yeniden denenir — aksi hâlde operatörün KAPAT komutu sessizce kaybolur ve
// pompa `maxRunMs`'e kadar çalışmaya devam ederdi.
//
// **İSTİSNA — güvenlik reddi mandallanmaz.** Hazne boşken verilen AÇ komutu
// mandallansaydı, hazne 20 dakika sonra dolduğunda pompa kimse başında
// değilken kendiliğinden çalışırdı. Güvenlik engeli kalkınca YENİ bir talep
// gerekir (ARCHITECTURE P6).

#include <stdint.h>

#include "core/Command.h"
#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/Time.h"
#include "domain/models/Actuator.h"

namespace domain {
namespace actuators {

/// Güvenlik izni sorgusu: "bu aktüatör ŞU AN enerjili OLABİLİR Mİ?"
///
/// `OK` = izin var; başka bir kod = engelin nedeni.
///
/// Soru bilinçli olarak "açılabilir mi" değil "enerjili kalabilir mi"dir.
/// Aynı yanıt hem AÇMA hem DEVAM ETME için geçerlidir — su seviyesi düştüğünde
/// çalışan pompanın durması ile yeni bir pompanın başlayamaması aynı kuraldır.
/// Bu sayede §12.1 Katman 2 (çalışma sırasında izleme) TEK KAPININ İÇİNDE
/// uygulanır; SafetyMonitor röleye hiçbir şekilde uzanmaz.
///
/// Fonksiyon işaretçisi olması bilinçlidir (TASK-012 `SafeStateHandler` deseni):
///   1. Manager'ın saklayabileceği bir güvenlik nesnesi yoktur → **önbelleğe
///      alınmış eski izin kullanmak yapısal olarak imkânsızdır.**
///   2. TASK-064 sahte bir izin fonksiyonuyla tüm tahkim ve kısıt
///      senaryolarını donanımsız koşturabilir.
using SafetyPermitFn = core::ErrCode (*)(core::ActuatorId id);

/// Yönetici başlatılır. Röleler zaten Boot Aşama 1'de güvenli konumdadır.
///
/// Boot'ta aktüatör durumu **geri yüklenmez** (ARCHITECTURE §19): beklenmedik
/// bir reset sonrası pompanın kendiliğinden yeniden başlaması, tam da reset'e
/// yol açan arıza sürerken olur.
///
/// @param cfg     canlı yapılandırma referansı (ConfigService'in örneği)
/// @param permit  güvenlik izni sorgusu; `nullptr` verilemez
core::ErrCode begin(const core::Config& cfg, SafetyPermitFn permit);

/// Bir aktüatör için durum TALEBİ kaydeder. Fiziksel yazma yapmaz.
///
/// Tahkim: `SAFETY` > `MANUAL` > `AUTOMATION` (ARCHITECTURE §10.3). Düşük
/// öncelikli bir kaynak, yüksek öncelikli bir kaynağın belirlediği durumu
/// geçersiz kılamaz → `REJECTED_MODE`.
core::CommandResult request(core::ActuatorId id, bool on, core::ControlSource source,
                            core::Millis now);

/// Niyetleri fiziksel duruma uygular. **Yalnızca `app_core` döngüsünden.**
///
/// Sırayla: gerçek pin durumunu oku → uyuşmazlığı raporla → `maxRunMs` aşımını
/// zorla kapat → ÇALIŞANLAR için izni taze sor (kalkmışsa derhal durdur) →
/// bekleyen niyetler için izni taze sor → kısıtları uygula → röleyi sür.
void apply(core::Millis now);

/// TÜM aktüatörleri kapatır. Kısıt TANIMAZ, bloklamaz, tahsis yapmaz.
///
/// `minRunMs` acil durumda uygulanmaz: kısa çevrim aşınması, taşan bir hazneden
/// veya kuru çalışan bir pompadan ucuzdur.
void forceAllOff(core::ErrCode reason, core::Millis now);

/// Aktüatörün kontrol kaynağını serbest bırakır — **TASK-057 kullanır.**
///
/// Manuel override süresi dolduğunda çağrılır. Bu olmadan tahkim kalıcı
/// bir KİLİTLENME üretir: bir kez `MANUAL` kaynaklı komut geldiğinde
/// `sourceOutranks(AUTOMATION, MANUAL)` sonsuza kadar `false` döner ve
/// otomasyon o aktüatörü BİR DAHA HİÇ kontrol edemez.
///
/// Yalnızca kaynağı sıfırlar; aktüatörün durumunu DEĞİŞTİRMEZ.
void releaseSource(core::ActuatorId id);

/// `maxRunMs` aşım sayısı. SafetyMonitor (TASK-030) eşiği bunun üzerinden izler.
uint16_t maxRunViolations(core::ActuatorId id);

/// Aşım sayaçlarını sıfırlar — acil durumdan çıkışta (TASK-032).
void clearMaxRunViolations();

/// Yayınlanan aktüatör alt-state'ini doldurur ve `StateStore`'a yazar.
///
/// Yayınlanan `isOn` **gerçek pin durumudur**, talep edilen değil: arayüzde
/// "pompa çalışıyor" yazarken pompanın durmuş olması operatörü yanlış bir
/// güven duygusuna sokar (ARCHITECTURE §2.6).
core::ErrCode publish(core::Millis now);

/// İç çalışma durumu — tanılama ve host testi için.
const ActuatorRuntime& runtime(core::ActuatorId id);

} // namespace actuators
} // namespace domain
