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
         : (s == ScreenId::SENSORS)   ? core::MAX_SENSORS
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

void resetConfirm() { g_confirm = false; }

void goTo(ScreenId s, Millis now)
{
    g_screen    = s;
    g_cursor    = 0;
    g_lastInput = now;
    resetConfirm();
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

            if (items > 0u)
            {
                // Ekran içi imleç. Uçlarda DURUR — dairesel değil.
                if (fwd && g_cursor + 1u < items)  { ++g_cursor; }
                else if (!fwd && g_cursor > 0u)    { --g_cursor; }
                else if (!fwd && g_cursor == 0u)
                {
                    // İmleç başındayken geriye çevirmek ekran değiştirir.
                    if (g_index > 0u) { --g_index; goTo(ORDER[g_index], now); }
                }
                else if (fwd && g_cursor + 1u >= items)
                {
                    if (g_index + 1u < ORDER_LEN) { ++g_index; goTo(ORDER[g_index], now); }
                }
            }
            else
            {
                // Seçilebilir öğesi olmayan ekranlarda encoder ekran değiştirir.
                if (fwd && g_index + 1u < ORDER_LEN)  { ++g_index; goTo(ORDER[g_index], now); }
                else if (!fwd && g_index > 0u)        { --g_index; goTo(ORDER[g_index], now); }
            }
            break;
        }

        case InputEventType::BUTTON_SHORT:
            if (ev.button == ButtonId::BACK)
            {
                // Acil durum aktifken BACK her yerden ACİL ekrana döner —
                // uyarıya dönüş her zaman bir tuş uzakta olmalı.
                if (emergencyActive) { goTo(ScreenId::EMERGENCY, now); }
                else if (g_confirm)  { resetConfirm(); }
                else                 { g_index = 0; goTo(ScreenId::HOME, now); }
                break;
            }

            // Kurulum ekranında basmak KURULUMU SÜRDÜRÜR: Wi-Fi bilgilerini
            // okuyan kullanıcının bir sonraki işi ürün seçmektir. Burada
            // hiçbir şey yapmayan bir düğme bırakmak, ekranı takılmış
            // gösterirdi.
            if (g_screen == ScreenId::SETUP)
            {
                g_index = 1;                       // ORDER[1] == CROP
                goTo(ScreenId::CROP, now);
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
    // Acil durumdan otomatik dönüş YOK; onay beklerken sayaç işlemez.
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
    if (g_screen != ScreenId::EMERGENCY) { goTo(ScreenId::EMERGENCY, now); }
}

void setCropCount(uint8_t n)
{
    g_cropCount = (n > UI_CROPS) ? UI_CROPS : n;
}

void onSetupNeeded(Millis now)
{
    if (g_screen != ScreenId::SETUP) { goTo(ScreenId::SETUP, now); }
}

ScreenId screen()     { return g_screen; }
uint8_t  cursor()     { return g_cursor; }
bool     confirming() { return g_confirm; }

} // namespace nav
} // namespace ui
} // namespace interfaces
