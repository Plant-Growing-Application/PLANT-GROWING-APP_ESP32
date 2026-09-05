#include "interfaces/ui/Navigation.h"

#include "core/SystemState.h"

namespace interfaces {
namespace ui {
namespace nav {
namespace {

using core::Millis;
using hal::ButtonId;
using hal::InputEventType;

/// Gezinilebilir ekranlar. `EMERGENCY` bu listede YOKTUR — oraya yalnızca
/// olay veya BACK ile girilir, encoder ile "denk gelinmez".
/// `SETUP` de listede YOKTUR: `EMERGENCY` gibi olayla girilir (ilk açılış),
/// encoder ile çıkılır. Aynı bilgiler `NETWORK` ekranında kalıcı durur, o
/// yüzden gezinme sırasında ikinci kez yer kaplamasına gerek yok.
///
/// `CROP` doğrudan `HOME`'un yanındadır: telefonu olmayan kullanıcının ilk
/// işi ürün seçmek ve programı başlatmaktır, altıncı ekranda aramak değil.
constexpr ScreenId ORDER[] = {
    ScreenId::HOME, ScreenId::CROP, ScreenId::SENSORS, ScreenId::CONTROL,
    ScreenId::NETWORK, ScreenId::SYSTEM, ScreenId::ALERTS,
};
constexpr uint8_t ORDER_LEN = sizeof(ORDER) / sizeof(ORDER[0]);

/// `CROP` ekranındaki satır sayısı: katalog + "BASLAT".
///
/// Katalog boyutu çalışma anında `core::cropCount()` ile bilinir ama
/// navigasyon ürün tablosunu TANIMAZ (bkz. `UiAction::APPLY_CROP`).
/// `UiService` gerçek sayıyı buraya bildirir; bildirilmediyse 0 ürün
/// varsayılır ve yalnızca "BASLAT" satırı kalır.
uint8_t g_cropCount = 0;

/// `SENSORS` ekranındaki satır sayısı — kayıtlı sensör adedi.
/// `UiService` her turda bildirir; bildirilmediyse ekran boş kabul edilir.
uint8_t g_sensorCount = 0;

/// Her ekranda kaç seçilebilir öğe var? 0 = seçim yok.
///
/// CONTROL: beş aktüatör + ACİL DURDUR (ISSUE-036). Sabit 3 iken büyütme
/// ışığı, ısıtıcı ve dozaj pompası OLED'den kontrol EDİLEMİYORDU.
///
/// SENSORS: seçilebilir eylem yok ama liste 8 satır ve ekran 5 satır alıyor;
/// imleç burada KAYDIRMA konumu olarak kullanılıyor. `itemCount` sıfır
/// bırakılsaydı encoder listeyi kaydıramazdı.
uint8_t itemCount(ScreenId s)
{
    return (s == ScreenId::CONTROL)   ? static_cast<uint8_t>(core::MAX_ACTUATORS + 1u)
         : (s == ScreenId::SENSORS)   ? g_sensorCount
         : (s == ScreenId::SYSTEM)    ? 1u   // yeniden başlat
         : (s == ScreenId::EMERGENCY) ? 1u   // onayla
         : (s == ScreenId::CROP)      ? static_cast<uint8_t>(g_cropCount + 1u)  // + BASLAT
                                      : 0u;
}

/// Bu ekranda onaylanacak bir eylem var mı?
///
/// `itemCount` ile AYNI ŞEY DEĞİLDİR: `SENSORS` ekranında imleç vardır
/// (8 ölçüm, 5 satır — kaydırma için) ama basılacak bir eylem YOKTUR.
/// Ayrılmasaydı, sensör ekranında basmak boş bir onay durumuna girer,
/// "Onaylamak icin bas" hiçbir yerde görünmez ve ikinci basış hiçbir şey
/// yapmazdı — kullanıcı cihazı takılmış sanardı (ISSUE-036).
bool isActionable(ScreenId s)
{
    return s == ScreenId::CONTROL || s == ScreenId::SYSTEM ||
           s == ScreenId::EMERGENCY || s == ScreenId::CROP;
}

uint8_t   g_index      = 0;      ///< ORDER içindeki konum
ScreenId  g_screen     = ScreenId::HOME;
uint8_t   g_cursor     = 0;
bool      g_confirm    = false;  ///< onay bekleniyor
Millis    g_lastInput{0};

/// Sayfanın İÇİNDE miyiz?
///
/// `false` (SAYFA MODU): encoder sayfaları gezer, imleç kullanılmaz.
/// `true`  (ÖĞE MODU):   encoder sayfa içindeki satırları gezer.
///
/// Tek seviyeli gezinmede kullanıcı, `SENSORS`'ın sekiz satırını çevirmeden
/// `CONTROL`'e geçemiyordu (TASK-075).
bool      g_focus      = false;

void resetConfirm() { g_confirm = false; }

/// Ekran değiştirir ve SAYFA MODUNA döner.
///
/// Yeni bir sayfaya öğe modunda düşmek, kullanıcıyı hiç istemediği bir
/// satırın üzerinde bırakırdı; ekran değişimi her zaman en dış seviyeye
/// döner.
void goTo(ScreenId s, Millis now)
{
    g_screen    = s;
    g_cursor    = 0;
    g_lastInput = now;
    g_focus     = false;
    resetConfirm();
}

/// Sayfa sırasındaki konumu ekrandan bulur.
///
/// `g_index` yeterli DEĞİLDİR: `EMERGENCY`/`SETUP` sıraya girmeden ekranı
/// devralır ve `g_index` o sırada eski sayfada kalır — gösterge yanlış
/// sayfayı işaret ederdi.
uint8_t indexOf(ScreenId s)
{
    for (uint8_t i = 0; i < ORDER_LEN; ++i)
    {
        if (ORDER[i] == s) { return i; }
    }
    return NO_PAGE;
}

/// Sayfa sırasında ilerler — DÖNGÜSELDİR.
///
/// Uçlarda durmak, tek seviyeli gezinmede imlecin ekran değiştirmesiyle
/// birleşince anlamlıydı. Sayfa modunda uçta durmak yalnızca yolu uzatır:
/// yedi sayfada döngüsellik ile en uzak sayfa üç detenttir.
void movePage(bool forward, Millis now)
{
    // Sıra dışı bir ekrandan (`EMERGENCY`, `SETUP`) çıkarken de YÖN KORUNUR.
    //
    // Bir ara "ayrılınan sayfaya dön" kestirmesi vardı: ilk çevirme yönü ne
    // olursa olsun aynı sayfayı açıyordu. Kullanıcı geriye çevirip ileri
    // gidince gezinmenin tutarsız olduğunu düşünüyordu. Bir sayfa atlamak,
    // encoder'ın yönüne güvenilmemesinden iyidir.
    g_index = forward ? static_cast<uint8_t>((g_index + 1u) % ORDER_LEN)
                      : static_cast<uint8_t>((g_index + ORDER_LEN - 1u) % ORDER_LEN);
    goTo(ORDER[g_index], now);
}

/// İmleç konumundan eylemi türetir. **Yalnızca onay verildikten sonra çağrılır.**
ActionRequest actionFor(ScreenId s, uint8_t cursor)
{
    if (s == ScreenId::EMERGENCY) { return {UiAction::EMERGENCY_CLEAR, 0}; }

    if (s == ScreenId::CONTROL)
    {
        // İmleç doğrudan aktüatör indeksidir; son öğe ACİL DURDUR'dur.
        // Eskiden iki aktüatör elle yazılıydı ve üçüncüsü acil durdurmaydı;
        // yeni röleler eklendiğinde bu zincirin güncellenmesi unutulsaydı
        // hata sahada "düğme hiçbir şey yapmıyor" olarak görünürdü.
        if (cursor < core::MAX_ACTUATORS)
        {
            return {UiAction::TOGGLE_ACTUATOR, cursor};
        }
        return {UiAction::EMERGENCY_STOP, 0};
    }

    if (s == ScreenId::CROP)
    {
        // Son satır programı başlatır/durdurur; öncekiler ürün seçer.
        // İmleç doğrudan KATALOG İNDEKSİDİR; kimliğe çeviren `UiService`.
        if (cursor < g_cropCount) { return {UiAction::APPLY_CROP, cursor}; }
        return {UiAction::TOGGLE_AUTOMATION, 0};
    }

    // SENSORS'ta imleç yalnızca kaydırma konumudur — onaylanacak bir eylem yok.
    if (s == ScreenId::SENSORS) { return {UiAction::NONE, 0}; }

    if (s == ScreenId::SYSTEM) { return {UiAction::RESTART, 0}; }

    return {UiAction::NONE, 0};
}

} // namespace

void begin()
{
    g_index   = 0;
    g_screen  = ScreenId::HOME;
    g_cursor  = 0;
    g_confirm = false;
    g_focus   = false;
}

ActionRequest handle(const hal::InputEvent& ev, Millis now, bool emergencyActive)
{
    g_lastInput = now;

    const uint8_t items = itemCount(g_screen);

    switch (ev.type)
    {
        case InputEventType::ENCODER_CW:
        case InputEventType::ENCODER_CCW:
        {
            const bool fwd = (ev.type == InputEventType::ENCODER_CW);

            // Onay beklerken encoder ONAYI İPTAL EDER. Kullanıcı fikrini
            // değiştirdiğinde en doğal hareket budur ve "yanlışlıkla
            // onayladım" durumunu engeller.
            if (g_confirm) { resetConfirm(); break; }

            // Kurulum ekranı gezinme sırasında değildir; çevirmek oradan
            // ÇIKAR. `g_index` zaten HOME'u gösteriyor.
            if (g_screen == ScreenId::SETUP) { g_index = 0; goTo(ORDER[0], now); break; }

            // ── ÖĞE MODU: imleç sayfanın İÇİNDE kalır ve DÖNER ─────────────
            //
            // Sayfa DEĞİŞTİRMEZ: eskiden imleç listenin sonuna gelince bir
            // sonraki sayfaya atlıyordu ve kullanıcı bir aktüatörü ararken
            // kendini başka bir ekranda buluyordu. Sayfadan çıkış tek bir
            // yerdendir: GERİ tuşu.
            //
            // Uçta DURMAK da olmaz: listenin sonunda encoder ölü görünüyordu —
            // kullanıcı çeviriyor, hiçbir piksel değişmiyor, ekran donmuş
            // sanılıyordu. Döngüsel imleç ikisini de çözer.
            //
            // `items` küçülmüş olsa bile (sensör sayısı düşerse) modülo
            // imleci kendiliğinden geçerli aralığa çeker.
            if (g_focus && items > 0u)
            {
                if (fwd) { g_cursor = static_cast<uint8_t>((g_cursor + 1u) % items); }
                else     { g_cursor = static_cast<uint8_t>((g_cursor + items - 1u) % items); }
                break;
            }

            // ── SAYFA MODU ─────────────────────────────────────────────────
            movePage(fwd, now);
            break;
        }

        case InputEventType::BUTTON_SHORT:
            if (ev.button == ButtonId::BACK)
            {
                // GERİ **tek adım** geri alır: önce onay, sonra sayfa modu,
                // en sonda HOME. Bir basışta birden çok seviye atlamak,
                // kullanıcının nerede olduğunu takip etmesini imkânsız kılar.
                if (g_confirm) { resetConfirm(); break; }
                if (g_focus)
                {
                    // İmleç de başa döner: sayfa modunda seçili satır
                    // ÇİZİLMEZ (imleç kullanıcıya ait değildir) ve kaydırılmış
                    // bir listenin ortasında bırakmak, sayfaya yeniden
                    // girildiğinde imlecin görünmeyen bir yerden başlaması
                    // demek olurdu.
                    g_focus  = false;
                    g_cursor = 0;
                    break;
                }

                // Acil durum aktifken BACK ACİL ekrana döner — uyarıya dönüş
                // her zaman bir tuş uzakta olmalı.
                //
                // ACİL EKRANIN KENDİSİ HARİÇ: orada da ACİL'e dönmek, ekranı
                // KİLİTLERDİ. Öğe modundan çıkıldıktan sonra tek çıkış yolu
                // bu tuş; teşhis için başka ekranlara bakmak gerekebilir
                // (ACİL rozeti zaten her ekranda kalıcı).
                if (emergencyActive && g_screen != ScreenId::EMERGENCY)
                {
                    goTo(ScreenId::EMERGENCY, now);   // SAYFA MODUNDA
                    break;
                }

                g_index = 0;
                goTo(ScreenId::HOME, now);
                break;
            }

            // Kurulum ekranında basmak KURULUMU SÜRDÜRÜR: Wi-Fi bilgilerini
            // okuyan kullanıcının bir sonraki işi ürün seçmektir. Burada
            // hiçbir şey yapmayan bir düğme bırakmak, ekranı takılmış
            // gösterirdi.
            if (g_screen == ScreenId::SETUP)
            {
                g_index = 1;                       // ORDER[1] == CROP
                goTo(ScreenId::CROP, now);          // SAYFA MODUNDA (bkz. `onEmergency`)
                break;
            }

            // ── SAYFA MODU: BAS = sayfanın İÇİNE gir ───────────────────
            //
            // İçeriği olmayan sayfada (HOME, NETWORK, ALERTS) hiçbir şey
            // yapmaz; ekran da bu sayfalarda "gir" ipucunu göstermez, yani
            // kullanıcı çalışmayan bir düğmeye basmaya davet edilmez.
            if (!g_focus)
            {
                if (items > 0u)
                {
                    g_focus  = true;
                    g_cursor = 0;   // her giriş listenin BAŞINDAN başlar
                }
                break;
            }

            // ENCODER_PUSH: onay akışı. Yalnızca eylemi OLAN ekranlarda.
            if (items == 0u || !isActionable(g_screen)) { break; }

            if (!g_confirm)
            {
                // İLK BASIŞ: yalnızca onay ister, eylem üretmez.
                g_confirm = true;
                break;
            }

            // İKİNCİ BASIŞ: eylem üretilir.
            resetConfirm();
            return actionFor(g_screen, g_cursor);

        case InputEventType::BUTTON_LONG:
            // Uzun basış her yerde HOME'a döner ve onayı iptal eder.
            g_index = 0;
            goTo(ScreenId::HOME, now);
            break;

        case InputEventType::NONE:
        default:
            break;
    }

    return {UiAction::NONE, 0};
}

void tick(Millis now, bool emergencyActive)
{
    // ── ÖNCE: ONAY VE ÖĞE MODU KENDİLİĞİNDEN DÜŞER ─────────────────────────
    //
    // Bu adım HER EKRANDA işler — acil ekranında da, acil durum aktifken de.
    // Aşağıdaki erken çıkışların arkasına konsaydı, acil durumdayken sayfanın
    // içinde unutulan bir kullanıcı orada SONSUZA KADAR kalırdı: encoder
    // yalnızca satırları gezer ve çıkışın tek yolu, varlığı bilinmeyen bir
    // tuş olurdu.
    //
    // Onayın da düşmesi bir GÜVENLİK kazancıdır: on dakika önce açılmış bir
    // "onaylamak icin bas" durumu, knob'a değen birinin pompayı çalıştırması
    // demektir. Zaman aşımı hiçbir zaman eylem ÜRETMEZ, yalnızca iptal eder.
    if (core::hasElapsed(now, g_lastInput, core::millisecs(FOCUS_IDLE_MS)))
    {
        resetConfirm();
        if (g_focus) { g_focus = false; g_cursor = 0; }
    }

    // Acil durumdan otomatik EKRAN dönüşü YOK; onay beklerken sayaç işlemez.
    if (g_screen == ScreenId::EMERGENCY || g_confirm || emergencyActive) { return; }
    if (g_screen == ScreenId::HOME) { return; }

    // Kurulum ekranından da otomatik dönüş YOK: kullanıcı o sırada Wi-Fi
    // şifresini telefonuna yazıyor olabilir ve ekranın altından kayması,
    // şifreyi baştan okumak için cihaza geri gitmek demektir.
    if (g_screen == ScreenId::SETUP) { return; }

    if (core::hasElapsed(now, g_lastInput, core::millisecs(IDLE_RETURN_MS)))
    {
        g_index = 0;
        goTo(ScreenId::HOME, now);
    }
}

void onEmergency(Millis now)
{
    // SAYFA MODUNDA açılır — otomatik olarak içine GİRİLMEZ.
    //
    // Bir süre öğe moduyla açıldı: "tek öğesi var, bir basış kazandıralım".
    // Sahada bunun bedeli ağırdı. Cihaz acil durumla açıldığında kullanıcı
    // farkında olmadan iki seviye derinde başlıyor, encoder'ı çeviriyor ve
    // HİÇBİR ŞEY olmuyordu (tek öğelik listede imleç kımıldamaz). Ekran
    // donmuş görünüyordu ve çıkış için önce onayı, sonra öğe modunu iptal
    // eden İKİ geri basışı gerekiyordu.
    //
    // Kural artık istisnasız: **sayfaya yalnızca kullanıcı basarak girer.**
    // Kazanılan bir basış, kaybedilen bir zihinsel modele değmez.
    if (g_screen != ScreenId::EMERGENCY) { goTo(ScreenId::EMERGENCY, now); }
}

void setCropCount(uint8_t n)
{
    g_cropCount = (n > UI_CROPS) ? UI_CROPS : n;
}

void setSensorCount(uint8_t n)
{
    g_sensorCount = (n > UI_SENSORS) ? UI_SENSORS : n;
}

void onSetupNeeded(Millis now)
{
    if (g_screen != ScreenId::SETUP) { goTo(ScreenId::SETUP, now); }
}

ScreenId screen()     { return g_screen; }
uint8_t  cursor()     { return g_cursor; }
bool     confirming() { return g_confirm; }
bool     focused()    { return g_focus; }

uint8_t  pageIndex()  { return indexOf(g_screen); }
uint8_t  pageCount()  { return ORDER_LEN; }

bool     pageEnterable() { return itemCount(g_screen) > 0u; }

} // namespace nav
} // namespace ui
} // namespace interfaces
