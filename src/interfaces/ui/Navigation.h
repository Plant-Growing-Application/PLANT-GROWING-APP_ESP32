#pragma once

// Ekran gezinme ve onay akışı — TASK-051
//
// ── İKİ SEVİYELİ GEZİNME (TASK-075) ─────────────────────────────────────────
// Encoder'ın TEK bir listede hem sayfaları hem satırları gezmesi, kullanıcıyı
// istediği sayfaya varmak için yoldaki HER SATIRI çevirmeye zorluyordu:
// `SENSORS` (8) + `CONTROL` (6) + `CROP` (7) = tek başına 21 detent. Sayfa
// değiştirmek, ilgilenilmeyen her seçeneğin üzerinden geçmek demekti.
//
// Artık iki seviye var:
//
//     SAYFA MODU   çevir → sayfa değiştir (7 sayfa, döngüsel)
//                  BAS   → sayfanın içine gir
//     ÖĞE MODU     çevir → sayfa içindeki satırlar (döngüsel)
//                  BAS   → onay akışı (iki basış)
//                  GERİ  → sayfa moduna dön
//
// Sayfa modunda EN FAZLA 3 detent ile her sayfaya ulaşılır (döngüsel).
//
// ── ÖĞE MODUNA YALNIZCA KULLANICI GİRER ─────────────────────────────────────
// Hiçbir olay, hiçbir ekran geçişi gezinmeyi kendiliğinden sayfanın İÇİNE
// almaz. İki yerde "bir basış kazandıralım" diye istisna yapıldı (acil durum
// ekranı ve kurulumdan ürün listesine geçiş) ve ikisi de aynı sonucu verdi:
// kullanıcı iki seviye derinde başlıyor, encoder'ı çeviriyor, sayfa
// değişmiyor ve ekran DONMUŞ görünüyordu. Çıkmak için varlığını bilmediği
// iki geri basışı gerekiyordu.
//
// Kazanılan bir basış, kaybedilen bir zihinsel modele değmez.
//
// ── ONAY ADIMI ──────────────────────────────────────────────────────────────
// OLED'den yapılabilecek her eylem İKİ BASIŞ ister:
//     encoder'a bas → "ONAYLA?" → tekrar bas → eylem üretilir
//
// Tek basışla pompa çalıştırmak, cebe giren ve sera duvarına asılı bir
// cihazda kabul edilemez.
//
// ── OLED'DEN NELER YAPILIR ──────────────────────────────────────────────────
//   · ACİL DURDURMA · acil durumu temizle · aktüatör aç/kapa · yeniden başlat
// Sayısal ayarlar (eşikler, kalibrasyon, zaman dilimi) YALNIZCA web'den:
// tek encoder ile eşik girmek hem yavaş hem hataya açıktır.
//
// ── ÖNCELİKLİ EKRAN ─────────────────────────────────────────────────────────
// Acil durum olunca ekran OTOMATİK `EMERGENCY`'ye geçer — ama SAYFA MODUNDA
// (yukarı bakınız). Kullanıcı diğer ekranlara geçebilir (teşhis için
// gerekli), durum çubuğunda kalıcı "ACİL" rozeti kalır ve BACK her yerden
// `EMERGENCY`'ye döner.
//
// Temizleme üç basış eder: gir → onay → uygula. Bir güvenlik eyleminde bu bir
// bedel değil; buna karşılık cihaz acil durumla açıldığında encoder ANINDA
// sayfa değiştirir.
//
// TEK İSTİSNA `EMERGENCY`'nin KENDİSİDİR: orada BACK'in yine aynı ekranı
// açması, ekranı kilitler ve teşhis için başka bir sayfaya bakmayı imkânsız
// kılardı.

#include <stdint.h>

#include "core/Time.h"
#include "hal/InputDevices.h"
#include "interfaces/ui/ScreenFramework.h"
#include "interfaces/ui/ViewModels.h"

namespace interfaces {
namespace ui {
namespace nav {

/// Kullanıcı işlem yapmazsa HOME'a dönüş süresi.
///
/// Ekranın bir alt menüde takılı kalması, yanına gelen birinin sistemin
/// durumunu göremeyeceği anlamına gelir. `EMERGENCY`'den otomatik dönüş
/// YOKTUR ve onay bekleyen bir işlem varsa sayaç işlemez — ama onayın kendisi
/// `FOCUS_IDLE_MS` sonunda düştüğü için sayaç sonsuza kadar durmaz.
constexpr uint32_t IDLE_RETURN_MS = 60000u;

/// Sayfa içinde (öğe modunda) ve onay beklerken hareketsizlik süresi.
///
/// Bu süre sonunda onay iptal edilir ve gezinme SAYFA MODUNA döner — ekran
/// değişmez, yalnızca seviye düşer. `IDLE_RETURN_MS`'ten çok daha kısadır
/// çünkü amacı farklıdır: ekranı toparlamak değil, kullanıcının **nerede
/// olduğunu bilmediği** bir moddan onu kurtarmak.
///
/// Cihaz acil durumla açıldığında ya da kullanıcı yanlışlıkla bir sayfaya
/// girdiğinde, encoder yalnızca satırları geziyor ve ekran DONMUŞ görünüyordu;
/// çıkış yolu bilinmiyorsa cihaz kullanılamaz hâle geliyordu.
constexpr uint32_t FOCUS_IDLE_MS = 20000u;

void begin();

/// Bir girdi olayını işler.
///
/// @return üretilen eylem; yoksa `UiAction::NONE`
ActionRequest handle(const hal::InputEvent& ev, core::Millis now, bool emergencyActive);

/// Zamanlayıcıları ilerletir (boşta kalma dönüşü).
void tick(core::Millis now, bool emergencyActive);

ScreenId screen();
uint8_t  cursor();
bool     confirming();

/// Sayfanın İÇİNDE miyiz? `false` ise encoder sayfaları geziyordur.
///
/// Ekran katmanı bunu bilmek ZORUNDADIR: sayfa modunda imleç kullanıcıya ait
/// değildir ve seçili satır çizilmez (bkz. `Screens.cpp`, seçim çubuğu).
bool focused();

/// Gezinme sırasındaki sayfa konumu — sayfa göstergesi için.
///
/// `EMERGENCY` ve `SETUP` sırada YOKTUR; oradayken `NO_PAGE` döner ve
/// gösterge çizilmez.
uint8_t pageIndex();
uint8_t pageCount();

/// Sayfa göstergesinde yeri olmayan ekranlar için işaret.
/// Ekran katmanıyla aynı sabit (`ViewModels.h`) — iki yerde tanımlanıp
/// birbirinden ayrı düşmesin.
constexpr uint8_t NO_PAGE = UI_NO_PAGE;

/// Bu sayfanın girilebilecek bir içeriği var mı?
///
/// Yoksa BAS hiçbir şey yapmaz ve ekranda "gir" ipucu GÖSTERİLMEZ: çalışmayan
/// bir düğmeyi göstermek, cihazı takılmış gibi gösterir (§12.2).
bool pageEnterable();

/// Acil duruma girildiğinde ekranı öncelikli ekrana taşır.
void onEmergency(core::Millis now);

/// Katalogdaki ürün sayısını bildirir.
///
/// Navigasyon ürün tablosunu tanımaz; yalnızca `CROP` ekranında kaç satır
/// olduğunu bilmesi gerekir. Sayı `UiService` tarafından her turda verilir,
/// böylece katalog büyürse burada değişecek bir şey olmaz.
void setCropCount(uint8_t n);

/// Kayıtlı sensör sayısını bildirir.
///
/// `setCropCount()` ile aynı gerekçe: navigasyon sensör tablosunu tanımaz,
/// yalnızca `SENSORS` ekranında kaç satır olduğunu bilmesi gerekir.
///
/// Sabit `MAX_SENSORS` kullanılamaz: daha az sensör kayıtlıysa imleç
/// çizilmeyen satırlara kadar ilerler ve encoder'ın son birkaç tıkı
/// **hiçbir şey yapmıyormuş gibi** görünürdü. Sayfa modu bunu gizliyordu
/// (imleç uçta ekran değiştiriyordu); iki seviyeli gezinmede (TASK-075)
/// ölü tık doğrudan görünür.
void setSensorCount(uint8_t n);

/// İlk açılış kurulum ekranını açar. `UiService` bunu YALNIZCA BİR KEZ
/// çağırır: kullanıcı ekrandan çıktıktan sonra kendiliğinden geri gelmez.
void onSetupNeeded(core::Millis now);

/// Kurulum bitti (ev ağına bağlanıldı): kurulum ekranından ÇIKAR.
///
/// Kurulum ekranı bir kez açılıp orada KALIYORDU. Kullanıcı kurulumu
/// telefonundan yaptığı için encoder'a hiç dokunmuyor; cihaz ev ağına
/// bağlandıktan sonra bile ekran kurulum ağını ve şifresini göstermeye devam
/// ediyordu. Ekrana bakan kullanıcı bağlanamadığını sanıp cihazı elle
/// resetliyordu.
///
/// Yalnızca `SETUP` ekranındayken iş yapar; kullanıcı o sırada başka bir
/// sayfaya geçmişse ekranı ELİNDEN ALMAZ.
void onSetupFinished(core::Millis now);

} // namespace nav
} // namespace ui
} // namespace interfaces
