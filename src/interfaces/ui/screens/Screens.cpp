// OLED ekranları — TASK-052
//
// ── MUTLAK YASAKLAR (§13.2) ─────────────────────────────────────────────────
// Bu dosyadaki hiçbir fonksiyon: sensör okumaz · Wi-Fi'ye dokunmaz · röle
// sürmez · NVS/EEPROM yazmaz · task suspend/resume çağırmaz.
//
// Eski `GrowPlant.cpp` bunların DÖRDÜNÜ BİRDEN yapıyordu: sayfa içindeyken
// encoder çevrilince `StateWifi()` çağrılıp Wi-Fi modu değiştiriliyor,
// EEPROM'a yazılıyor ve `pauseWiFiMonitor()` ile başka bir task askıya
// alınıyordu (REQUIREMENTS §6.3).
//
// İmza bunu yapısal olarak garanti eder: `void draw(const UiModel&)`.
//
// ── TEK DOSYA, PLANDAKİ SEKİZ DOSYA DEĞİL ───────────────────────────────────
// Plan her ekran için ayrı `.cpp` öngörüyordu. Ekranların tamamı 300 satır
// ve HEPSİ aynı `Layout` sabitlerini, aynı yardımcıları kullanıyor. Sekiz
// dosyaya bölmek, sekiz kez aynı başlık bloğunu ve aynı `namespace`
// merdivenini tekrarlamak demekti. Bölme, kod büyüdüğünde yapılır.
//
// ── DÜZEN SABİTLERİ ─────────────────────────────────────────────────────────
// Çıplak koordinat YOK. Eski projede aynı yazı bir yerde Y=35, başka yerde
// Y=28'e yazılıyordu ve ekran geçişlerinde satırlar kayıyordu.

#include <stdio.h>
#include <string.h>

#include "hal/OledPanel.h"
#include "interfaces/ui/Navigation.h"
#include "interfaces/ui/ScreenFramework.h"

namespace interfaces {
namespace ui {
namespace {

namespace L = layout;

/// Metin. `inverse` ise beyaz zemine SİYAH yazar (seçim çubuğunun içi).
void text(int16_t x, int16_t y, const char* s, bool inverse = false)
{
    if (inverse) { hal::oled::drawTextInverse(x, y, s); }
    else         { hal::oled::drawText(x, y, s); }
}

/// Yerleşik 5×7 fontta bir karakterin kapladığı genişlik (1 px boşlukla).
constexpr int16_t CHAR_W = 6;

/// `row()` kesme tamponu. 128 px / 6 px = 21 karakter; sonlandırıcıyla pay.
constexpr size_t VALUE_MAX = 26;

/// Değeri verilen genişliğe SIĞDIRIR; sığmıyorsa keser ve ".." ekler.
///
/// ── NEDEN GEREKLİ ───────────────────────────────────────────────────────────
/// Uzun bir ağ adı ("MisafirAgi_2.4GHz_UstKat" gibi) 128 px'lik panele
/// sığmaz: sağa yaslanan değer etiketin üzerine biner, kalan kısım ekranın
/// dışına taşar ve sürücü onu ortadan keser. Satır okunmaz hâle gelir —
/// sahada "ekran karışıyor" olarak görüldü.
///
/// KESİLDİĞİNİ SÖYLEMEK ŞART: sessizce kısaltılmış bir ağ adı, kullanıcıya
/// yanlış ağa bağlanıldığı izlenimi verir. ".." bunu iki karakterle anlatır.
///
/// Sığan değerler KOPYALANMAZ; işaretçi olduğu gibi döner.
const char* fitValue(const char* value, int16_t availPx, char* buf, size_t cap)
{
    if (availPx < 0) { availPx = 0; }

    size_t maxChars = static_cast<size_t>(availPx / CHAR_W);
    if (maxChars > cap - 1) { maxChars = cap - 1; }

    const size_t len = strlen(value);
    if (len <= maxChars) { return value; }

    // ".." için iki karakter ayrılır. O kadar bile yer yoksa düz kırpılır:
    // "..", bilginin tamamının yerini alacak kadar değerli değildir.
    if (maxChars <= 2u)
    {
        memcpy(buf, value, maxChars);
        buf[maxChars] = '\0';
        return buf;
    }

    memcpy(buf, value, maxChars - 2u);
    buf[maxChars - 2u] = '.';
    buf[maxChars - 1u] = '.';
    buf[maxChars]      = '\0';
    return buf;
}

/// Bir satırı "etiket ......... değer" olarak yazar.
///
/// DEGER SAGA YASLANIR ve ekrandan TASMAZ.
///
/// Onceki hali degeri sabit `COL_VALUE = 62`'den basliyordu. Bir IP adresi
/// ("192.168.1.100" = 13 karakter × 6 px = 78 px) 62 + 78 = 140 px eder ve
/// 128 px'lik ekranin DISINA tasar — sahada "IP ekrana sigmiyor" olarak
/// goruldu.
///
/// Saga yaslama ile ayni IP x=50'den baslar ve tam oturur. Deger etiketin
/// uzerine binecek kadar uzunsa `fitValue()` onu keser ve ".." ekler: etiket
/// okunur kalir, satir tasmaz.
void row(int16_t y, const char* label, const char* value, bool inverse = false)
{
    text(L::COL_LABEL, y, label, inverse);

    if (value == nullptr || *value == 0) { return; }

    const int16_t labelEnd = static_cast<int16_t>(
        L::COL_LABEL + hal::oled::textWidth(label, 1) + 4);

    // Sağ kenarda `PAD_RIGHT` kadar yer bırakılır: seçim çubuğu satırın
    // tamamını kaplıyor ve kenara yapışan bir değer çubuğun içinde sıkışmış
    // görünüyordu.
    const int16_t rightEdge = static_cast<int16_t>(L::W - L::PAD_RIGHT);

    // Etiketten sonra kalan alana SIĞDIRILIR; taşan değer kesilir.
    char        buf[VALUE_MAX];
    const char* shown  = fitValue(value, static_cast<int16_t>(rightEdge - labelEnd),
                                  buf, sizeof(buf));
    const int16_t valueW = static_cast<int16_t>(hal::oled::textWidth(shown, 1));

    int16_t x = static_cast<int16_t>(rightEdge - valueW);
    if (x < labelEnd) { x = labelEnd; }

    text(x, y, shown, inverse);
}

/// Seçili satırın ters renkli vurgusu.
///
/// ── NEDEN `>` DEĞİL ─────────────────────────────────────────────────────────
/// Seçim eskiden satır başına konan tek bir `>` karakteriyle anlatılıyordu:
/// 128×64 bir panelde 5×7 piksellik bir işaret, bir metre öteden — yani
/// cihazın sera duvarında asılı olduğu normal mesafeden — seçilmiyordu.
/// Kullanıcı hangi satırda olduğunu ancak ekrana eğilerek görebiliyordu.
///
/// Satırın tamamının ters renge dönmesi, aynı bilgiyi 128×10 piksellik bir
/// yüzeyle anlatır ve uzaktan bakışta tek görünen şey olur.
void selectionBar(int16_t y)
{
    hal::oled::fillHighlight(0, static_cast<int16_t>(y + L::BAR_DY), L::W, L::BAR_H);
}

/// Seçilebilir satır.
///
/// @param focused  sayfanın İÇİNDE miyiz? Sayfa modunda imleç KULLANICIYA AİT
///                 DEĞİLDİR (encoder sayfaları geziyordur) ve hiçbir satır
///                 vurgulanmaz: vurgulanan satır, çevirince oraya gidileceği
///                 sözünü verirdi ve bu söz tutulmazdı.
/// @param confirming  onay bekleniyor. Değer sütunu "ONAY?" olur; hangi
///                 satırın onay beklediği, ekranın altındaki ipucu satırından
///                 bağımsız olarak SATIRIN ÜZERİNDE yazar.
void selectableRow(int16_t y, uint8_t index, uint8_t cursor, bool focused,
                   bool confirming, const char* label, const char* value)
{
    if (index != cursor || !focused)
    {
        row(y, label, value);
        return;
    }

    selectionBar(y);
    row(y, label, confirming ? "ONAY?" : value, true);
}

/// Sayfa göstergesi ve mod işareti — ayırıcı çizginin YERİNE.
///
/// ── NEDEN ───────────────────────────────────────────────────────────────────
/// Yedi sayfa arasında dolaşırken "kaçıncı sayfadayım, kaç sayfa var"
/// sorusunun cevabı hiçbir yerde yazmıyordu: kullanıcı sayfayı yalnızca
/// içeriğinden tanıyor, sıranın nerede biteceğini bilmiyordu.
///
/// Gösterge FAZLADAN SATIR HARCAMAZ — zaten çizilen ayırıcı çizginin şeridini
/// kullanır. 128×64'te bir satır, gösterilebilecek ölçümlerin beşte biridir.
///
///     ▁▁ ▁▁ ██ ▁▁ ▁▁ ▁▁ ▁▁          bulunulan sayfa kalın ve dolu
///                            ▼      sayfa modu:  BAS = içeri gir
///                            ▲      öğe modu:    GERİ = dışarı çık
///
/// Mod işareti, iki seviyeli gezinmenin (TASK-075) tek görsel ipucudur:
/// kullanıcının "şu an neyi çeviriyorum" sorusuna bakışta cevap verir.
void drawNavStrip(const UiModel& m)
{
    constexpr int16_t GLYPH_W = 5;
    constexpr int16_t GLYPH_X = L::W - GLYPH_W - 1;
    constexpr int16_t STRIP_W = GLYPH_X - 2;          ///< işarete kadar olan alan

    // ── ÖĞE MODU: ŞERİDİN TAMAMI YANAR ─────────────────────────────────────
    //
    // Mod farkı yalnızca 5×3 piksellik bir ok işaretiyle anlatılıyordu ve
    // KİMSE FARK ETMEDİ: kullanıcı sayfanın içinde olduğunu bilmeden encoder'ı
    // çeviriyor, sayfa değişmiyor ve ekran donmuş sanılıyordu.
    //
    // Dolu şerit ile dilimlenmiş şerit arasındaki fark, ekrana bakar bakmaz
    // görülür. Sayfa konumu bu modda GÖSTERİLMEZ; kullanıcı zaten sayfa
    // değiştirmiyor, çıkmayı bilmesi gerekiyor.
    if (m.navFocus != 0u)
    {
        hal::oled::fillHighlight(0, L::SEP_Y, STRIP_W, 3);

        // ▲ — "GERİ ile çık"
        hal::oled::drawLine(GLYPH_X + 2, L::SEP_Y,     GLYPH_X + 2, L::SEP_Y);
        hal::oled::drawLine(GLYPH_X + 1, L::SEP_Y + 1, GLYPH_X + 3, L::SEP_Y + 1);
        hal::oled::drawLine(GLYPH_X,     L::SEP_Y + 2, GLYPH_X + 4, L::SEP_Y + 2);
        return;
    }

    // ── SAYFA MODU: KONUM GÖSTERGESİ ───────────────────────────────────────
    if (m.pageCount == 0u || m.pageIndex >= m.pageCount)
    {
        // Sırada YERİ OLMAYAN ekran (`EMERGENCY`, `SETUP`): gösterilecek bir
        // konum yok. Yanlış bir dilimi yakmaktansa düz ayırıcı çizilir.
        hal::oled::drawLine(0, L::SEP_Y, L::W - 1, L::SEP_Y);
    }
    else
    {
        const int16_t seg = static_cast<int16_t>(STRIP_W / m.pageCount);
        const int16_t w   = static_cast<int16_t>(seg - 2);   // dilimler arası boşluk

        for (uint8_t i = 0; i < m.pageCount; ++i)
        {
            const int16_t x = static_cast<int16_t>(i * seg);

            if (i == m.pageIndex) { hal::oled::fillHighlight(x, L::SEP_Y, w, 3); }
            else
            {
                hal::oled::drawLine(x, L::SEP_Y + 1, static_cast<int16_t>(x + w - 1),
                                    L::SEP_Y + 1);
            }
        }
    }

    if (m.pageEnterable != 0u)
    {
        // ▼ — "BAS ile gir". İçeriği olmayan sayfada ÇİZİLMEZ: çalışmayan bir
        // düğmeye davet etmek, cihazı takılmış gösterir (§12.2).
        hal::oled::drawLine(GLYPH_X,     L::SEP_Y,     GLYPH_X + 4, L::SEP_Y);
        hal::oled::drawLine(GLYPH_X + 1, L::SEP_Y + 1, GLYPH_X + 3, L::SEP_Y + 1);
        hal::oled::drawLine(GLYPH_X + 2, L::SEP_Y + 2, GLYPH_X + 2, L::SEP_Y + 2);
    }
}

/// Sayfa modunda gösterilen ipucu.
///
/// Şeritteki ▼ işareti gezinme yapısını anlatır ama İLK KEZ karşılaşan
/// kullanıcıya bir sembol yetmez. Boş kalan ipucu satırı olan ekranlarda
/// gesture bir kez açıkça yazılır; kullanıcı öğrendikten sonra da yeri
/// başka bir bilgiden çalınmış olmaz.
constexpr const char* HINT_ENTER = "Girmek icin bas";

/// Öğe modunda gösterilen ipucu — **çıkış yolu yazıyla durur**.
///
/// Sayfanın içinde olduğunu fark etmeyen kullanıcı için tek kurtuluş, geri
/// tuşunun orada yazıyor olmasıdır. Zaman aşımı (bkz. `FOCUS_IDLE_MS`) bunun
/// ağıdır; bu satır ise yirmi saniye beklemeyi gereksiz kılar.
constexpr const char* HINT_BACK  = "Geri: cikis";

/// Wi-Fi sinyal çubukları — 0 çubuk = bağlı değil.
///
/// ── TERSİNE ÇİZİLİYORDU ─────────────────────────────────────────────────────
/// Dolu çubuklar `drawRect(..., filled=true)` ile çiziliyordu ve o çağrı
/// SİYAH doldurur: gösterge tam tersini anlatıyordu — dört çubukluk sinyalde
/// hiçbir şey görünmüyor, sinyal yokken dört boş çerçeve duruyordu.
void drawBars(int16_t x, int16_t y, uint8_t bars)
{
    for (uint8_t i = 0; i < 4u; ++i)
    {
        const int16_t h  = static_cast<int16_t>(2 + i * 2);
        const int16_t bx = static_cast<int16_t>(x + i * 3);
        const int16_t by = static_cast<int16_t>(y + 8 - h);

        if (i < bars) { hal::oled::fillHighlight(bx, by, 2, h); }
        else          { hal::oled::drawRect(bx, by, 2, h, false); }
    }
}

} // namespace

// ── Durum çubuğu — TÜM ekranlarda ortak ────────────────────────────────────
//
// Her ekranda yeniden yazılmaz; `renderScreen()` bir kez çağırır.
void drawStatusBar(const UiModel& m)
{
    text(L::COL_LABEL, L::STATUS_Y, m.clock);

    drawBars(34, L::STATUS_Y, m.wifiBars);
    if (m.apActive != 0u) { text(48, L::STATUS_Y, "AP"); }

    // Hata rozeti SAĞA YASLANIR ve yerini önce ayırır; mod yazısı kalan
    // alana sığdırılır.
    //
    // Önceki hâlde mod yazısı sabit x=64'ten başlıyor, hata rozeti sabit
    // x=114'e yazılıyordu: "CALISIYOR" (54 px) 64+54 = 118'e kadar uzanıp
    // rozetin üzerine biniyordu.
    int16_t rightEdge = L::W;

    if (m.faultCount > 0u)
    {
        char b[8];
        snprintf(b, sizeof(b), "!%u", m.faultCount);
        const int16_t bw = static_cast<int16_t>(hal::oled::textWidth(b, 1));
        rightEdge = static_cast<int16_t>(L::W - bw);
        text(rightEdge, L::STATUS_Y, b);
        rightEdge = static_cast<int16_t>(rightEdge - 3);   // ayırıcı boşluk
    }

    // ACİL rozeti KALICIDIR: kullanıcı hangi ekranda olursa olsun görür.
    //
    // Rozet artık GERÇEKTEN ters renkli. Önceki hâlde zemin `drawRect(...,
    // true)` ile SİYAH dolduruluyordu (o çağrı bir alanı temizler) ve rozet
    // yanındaki mod yazısından hiçbir farkı kalmıyordu: sistemin en kritik
    // göstergesi, düz bir kelime olarak duruyordu.
    if (m.emergency != 0u)
    {
        const int16_t w = static_cast<int16_t>(hal::oled::textWidth("ACIL", 1) + 6);
        const int16_t x = static_cast<int16_t>(rightEdge - w);
        hal::oled::fillHighlight(x, L::STATUS_Y, w, 10);
        text(static_cast<int16_t>(x + 3), L::STATUS_Y + 1, "ACIL", true);
    }
    else
    {
        const int16_t mw = static_cast<int16_t>(hal::oled::textWidth(m.modeText, 1));
        int16_t       x  = static_cast<int16_t>(rightEdge - mw);
        if (x < 62) { x = 62; }   // "AP" göstergesinin üzerine binmesin
        text(x, L::STATUS_Y, m.modeText);
    }

    drawNavStrip(m);
}

// ── HOME: özet ──────────────────────────────────────────────────────────────
void drawHome(const UiModel& m)
{
    // Kurulum bitti ve cihaz yeniden başlıyor: kurulum bilgisini göstermenin
    // anlamı kalmadı — kullanıcının ihtiyacı artık YENİ ADRES.
    if (m.setupReboot != 0u)
    {
        text(L::COL_LABEL, L::ROW0, "KURULUM TAMAM");
        row(L::ROW1, "Ag", m.ssid);
        row(L::ROW2, "Adres", m.ip);
        text(L::COL_LABEL, L::ROW3, "Yeniden baslatiliyor");
        return;
    }

    // ── KURULUM MODU: bağlantı bilgisi HOME'DA ─────────────────────────────
    //
    // AP açıkken kullanıcının tek ihtiyacı bu bilgidir. Onu yalnızca NETWORK
    // ekranına koymak, kullanıcıyı önce gezinmeye zorlar — encoder çalışmıyorsa
    // cihaza girmenin HİÇBİR YOLU KALMAZ. İlk sahada tam olarak bu yaşandı.
    //
    // ── AMA YALNIZCA EV AĞINA BAĞLI DEĞİLKEN ───────────────────────────────
    // `apActive` tek başına YETMEZ: cihaz ev ağına bağlandıktan sonra kurulum
    // AP'si linger süresi boyunca (30-90 sn) açık kalır. Ekran o pencerede
    // hâlâ "KURULUM MODU" ve kurulum şifresi yazıyordu — bağlantı çoktan
    // kurulmuşken. Kullanıcı ekrana bakıp bağlanamadığını sanıyor, cihazı
    // elle resetliyordu.
    //
    // Bağlıyken doğru bilgi aşağıdaki özet ve **IP adresidir**.
    if (m.apActive != 0u && m.staConnected == 0u && m.apSsid[0] != '\0')
    {
        text(L::COL_LABEL, L::ROW0, "KURULUM MODU");
        row(L::ROW1, "Ag", m.apSsid);
        row(L::ROW2, "Sifre", m.apPassword);
        text(L::COL_LABEL, L::ROW3, "192.168.4.1");
        return;
    }

    // Kritik olanlar önce: seviye ve pompa. 128×64'te "ne gösterilmeyeceği"
    // de bir tasarım kararıdır.
    const SensorLine* level = nullptr;
    for (uint8_t i = 0; i < UI_SENSORS; ++i)
    {
        if (m.sensors[i].present && strcmp(m.sensors[i].label, "Seviye") == 0)
        {
            level = &m.sensors[i];
            break;
        }
    }

    row(L::ROW0, "Su seviye", level ? level->value : "--");
    row(L::ROW1, m.actuators[0].label, m.actuators[0].on ? "ACIK" : "kapali");
    row(L::ROW2, m.actuators[1].label, m.actuators[1].on ? "ACIK" : "kapali");

    if (m.alertText[0] != '\0') { text(L::COL_LABEL, L::ROW3, m.alertText); }
    else                        { row(L::ROW3, "Calisma", m.uptime); }

    // ── IP AÇILIŞ EKRANINDA ────────────────────────────────────────────────
    //
    // Adres yalnızca `NETWORK` sayfasındaydı: arayüzü açmak isteyen herkes
    // önce cihazın başına gidip sayfalar arasında dolaşmak zorundaydı. Cihazın
    // İLK gösterdiği ekranda durması, telefondan bağlanmak için gereken tek
    // bilgiyi tek bakışa indirir (TASK-075).
    //
    // Bağlantı yokken model "bagli degil" taşır ve BU DA BİLGİDİR: boş bir
    // satır, kullanıcıya adresin ne olduğunu değil, ekranın bozuk olduğunu
    // düşündürürdü.
    row(L::ROW4, "IP", m.ip);
}

// ── SENSORS: tüm sensörler + kalite ────────────────────────────────────────
/// Kayan pencerenin başlangıcını hesaplar.
///
/// İmleç her zaman GÖRÜNÜR kalmalı: seçili öğe pencerenin altına taşarsa
/// pencere onunla birlikte kayar. `cursor` doğrudan başlangıç olarak
/// kullanılsaydı, listenin sonuna gelindiğinde boş satırlar çizilirdi.
static uint8_t windowStart(uint8_t cursor, uint8_t total, uint8_t visible)
{
    if (total <= visible) { return 0u; }
    const uint8_t maxStart = static_cast<uint8_t>(total - visible);
    if (cursor < visible) { return 0u; }
    const uint8_t want = static_cast<uint8_t>(cursor - visible + 1u);
    return (want > maxStart) ? maxStart : want;
}

// KAYDIRMA GÖSTERGESİ OLARAK İMLEÇ KULLANILIYOR — sayısal bir "3-7/8"
// göstergesi denendi ve DURUM ÇUBUĞUNA sığmadı: çubuğun sağı mod yazısı
// tarafından kullanılıyor ("CALISIYOR" ~54 px, sağa yaslı) ve gösterge onun
// üzerine biniyordu. Ekranın gövdesinde de yer yok; değerler sağa yaslı.
//
// İmleç zaten her satırda `>` ile çiziliyor ve kullanıcı çevirdikçe hareket
// ediyor; listenin devam ettiğini bu yeterince anlatıyor. Fazladan bir metin,
// 128 px'lik bir ekranda çakışma riskine değmez.

// ── SENSORS: 8 ölçüm, 5 satır — KAYAN PENCERE (ISSUE-036) ──────────────────
//
// `UI_SENSORS` 6'dan 8'e çıktı (TASK-066 ile nem, hava sıcaklığı ve ışık
// eklendi). Ekran 5 satır alıyor; eskiden ilk 5 çizilip gerisi SESSİZCE
// düşüyordu. Artık encoder listeyi kaydırıyor.
void drawSensors(const UiModel& m)
{
    const int16_t rows[] = {L::ROW0, L::ROW1, L::ROW2, L::ROW3, L::ROW4};

    // Önce görünür (present) satırların sıkıştırılmış listesi çıkarılır:
    // takılı olmayan bir sensör pencerede yer kaplamamalı.
    uint8_t visibleIdx[UI_SENSORS];
    uint8_t total = 0;
    for (uint8_t i = 0; i < UI_SENSORS; ++i)
    {
        if (m.sensors[i].present != 0u) { visibleIdx[total++] = i; }
    }

    if (total == 0u)
    {
        text(L::COL_LABEL, L::ROW0, "Sensor verisi yok");
        return;
    }

    const uint8_t cursor = (m.cursor < total) ? m.cursor : static_cast<uint8_t>(total - 1u);
    const uint8_t start  = windowStart(cursor, total, UI_VISIBLE_ROWS);

    uint8_t drawn = 0;
    for (uint8_t k = start; k < total && drawn < UI_VISIBLE_ROWS; ++k, ++drawn)
    {
        const SensorLine& s = m.sensors[visibleIdx[k]];

        // Değer zaten `UiModel` tarafından kalite tablosuna göre
        // biçimlendirilmiş durumda ("--" / "yok" / "12.3!"). Ekran katmanı
        // kendi başına biçimlendirme YAPMAZ — arızalı sensörün eski değerini
        // göstermek eski projedeki en tehlikeli hataydı.
        //
        // `selectableRow` ile çiziliyor: imleç burada bir EYLEM seçmez,
        // yalnızca listedeki konumu gösterir. `confirming` her zaman false —
        // sensör ekranında onaylanacak bir şey yok.
        selectableRow(rows[drawn], k, cursor, m.navFocus != 0u, false, s.label, s.value);
    }
}

// ── CONTROL: aktüatörler + acil durdurma ───────────────────────────────────
// ── CONTROL: 5 aktüatör + ACİL DURDUR = 6 öğe, 5 satır ─────────────────────
//
// Eskiden yalnızca su ve hava pompası vardı ve üçüncü satır ACİL DURDUR'du.
// TASK-066 ile üç röle daha gerçek oldu; sabit üç satır onları erişilemez
// bırakıyordu (ISSUE-036). Liste artık kayıyor.
//
// ACİL DURDUR **her zaman son öğedir** ve kaydırmayla yeri değişmez: kritik
// bir kontrolün konumunun listeye göre oynaması kabul edilemez.
void drawControl(const UiModel& m)
{
    const int16_t rows[] = {L::ROW0, L::ROW1, L::ROW2, L::ROW3, L::ROW4};

    const uint8_t total  = static_cast<uint8_t>(UI_ACTS + 1u);   // +1 = ACİL DURDUR
    const uint8_t cursor = (m.cursor < total) ? m.cursor : static_cast<uint8_t>(total - 1u);

    // Engel nedeni ve onay satırı için bir satır ayrılır; ikisi de aynı anda
    // görünmez olduğundan tek satır yeterli.
    const uint8_t listRows = static_cast<uint8_t>(UI_VISIBLE_ROWS - 1u);
    const uint8_t start    = windowStart(cursor, total, listRows);

    uint8_t drawn = 0;
    for (uint8_t k = start; k < total && drawn < listRows; ++k, ++drawn)
    {
        if (k < UI_ACTS)
        {
            selectableRow(rows[drawn], k, cursor, m.navFocus != 0u, m.editing != 0u,
                          m.actuators[k].label, m.actuators[k].on ? "ACIK" : "kapali");
        }
        else
        {
            selectableRow(rows[drawn], k, cursor, m.navFocus != 0u, m.editing != 0u,
                          "ACIL DURDUR", "");
        }
    }

    // Engel nedeni GÖSTERİLİR. Sessizce çalışmayan bir buton, kullanıcıyı
    // "sistem bozuk" sanmaya iter (§12.2 gözlemlenebilirlik).
    const int16_t lastRow = rows[UI_VISIBLE_ROWS - 1u];

    if (m.navFocus == 0u)
    {
        // Sayfa modunda imleç kullanıcıya ait değil: seçili aktüatörün engel
        // nedenini göstermek, seçmediği bir satır hakkında konuşmak olurdu.
        text(L::COL_LABEL, lastRow, HINT_ENTER);
    }
    else if (m.editing != 0u)
    {
        text(L::COL_LABEL, lastRow, "Onaylamak icin bas");
    }
    else if (cursor < UI_ACTS && m.actuators[cursor].blocked != 0u)
    {
        text(L::COL_LABEL, lastRow, m.actuators[cursor].why);
    }
    else
    {
        text(L::COL_LABEL, lastRow, HINT_BACK);
    }
}

// ── NETWORK: durum, SSID, IP — ŞİFRE YOK ───────────────────────────────────
void drawNetwork(const UiModel& m)
{
    row(L::ROW0, "Durum", m.netState);
    row(L::ROW1, "SSID", m.ssid[0] ? m.ssid : "-");
    row(L::ROW2, "IP", m.ip);

    // Kurulum AP bilgisi: kullanıcı bu şifreyi ekrandan okuyup cihaza
    // bağlanır. Bu, cihazın KENDİ ürettiği kurulum şifresidir — kullanıcının
    // ev ağı şifresi HİÇBİR ekranda gösterilmez (§8.2).
    if (m.apActive != 0u && m.apSsid[0] != '\0')
    {
        row(L::ROW3, "Kurulum", m.apSsid);
        row(L::ROW4, "Sifre", m.apPassword);
    }
    else
    {
        char b[16];
        snprintf(b, sizeof(b), "%d dBm", static_cast<int>(m.rssi));
        row(L::ROW3, "Sinyal", m.wifiBars ? b : "-");
    }
}

// ── SYSTEM: uptime, mod, yeniden başlat ────────────────────────────────────
void drawSystem(const UiModel& m)
{
    row(L::ROW0, "Mod", m.modeText);
    row(L::ROW1, "Calisma", m.uptime);
    row(L::ROW2, "Saat", m.clock);

    selectableRow(L::ROW3, 0, m.cursor, m.navFocus != 0u, m.editing != 0u,
                  "Yeniden baslat", "");

    if (m.editing != 0u)       { text(L::COL_LABEL, L::ROW4, "Onaylamak icin bas"); }
    else if (m.navFocus == 0u) { text(L::COL_LABEL, L::ROW4, HINT_ENTER); }
    else                       { text(L::COL_LABEL, L::ROW4, HINT_BACK); }
}

// ── ALERTS: aktif hatalar ──────────────────────────────────────────────────
void drawAlerts(const UiModel& m)
{
    if (m.faultCount == 0u && m.alertText[0] == '\0')
    {
        text(L::COL_LABEL, L::ROW1, "Aktif uyari yok");
        return;
    }

    char b[24];
    snprintf(b, sizeof(b), "%u aktif hata", m.faultCount);
    text(L::COL_LABEL, L::ROW0, b);

    if (m.alertText[0] != '\0') { text(L::COL_LABEL, L::ROW1, m.alertText); }

    if (m.emergency != 0u)
    {
        text(L::COL_LABEL, L::ROW2, "ACIL DURUM AKTIF");
        text(L::COL_LABEL, L::ROW3, m.emergencyWhy);
        text(L::COL_LABEL, L::ROW4, "Detay: geri tusu");
    }
}

// ── EMERGENCY: öncelikli ekran ─────────────────────────────────────────────
//
// Yalnızca "HATA" yazmak yetersizdir. Neden + ne yapılması gerektiği +
// onayın nasıl verileceği gösterilir.
void drawEmergency(const UiModel& m)
{
    // Başlık bandı GERÇEKTEN ters renkli. Önceki hâlde zemin `drawRect(...,
    // true)` ile SİYAH dolduruluyordu — o çağrı bir alanı TEMİZLER — ve band
    // hiç görünmüyordu: sistemin en kritik başlığı sıradan bir satır gibi
    // duruyordu.
    hal::oled::fillHighlight(0, L::BODY_Y - 2, L::W, 12);
    text(24, L::ROW0, "ACIL DURUM", true);

    text(L::COL_LABEL, L::ROW1, m.emergencyWhy[0] ? m.emergencyWhy : "neden bilinmiyor");
    text(L::COL_LABEL, L::ROW2, "Aktuatorler kesildi");

    // Koşullar düzelmeden onay reddedilir (TASK-032); kullanıcıya bunu
    // önceden söylüyoruz ki reddi bir arıza sanmasın.
    text(L::COL_LABEL, L::ROW3, "Once sorunu giderin");
    selectableRow(L::ROW4, 0, m.cursor, m.navFocus != 0u, m.editing != 0u,
                  "Onayla ve temizle", "");
}

// ── SETUP: ilk açılış — telefonu cihaza bağlamak için gereken her şey ──────
//
// Bu ekran cihazın KENDİ ürettiği kurulum şifresini gösterir (TASK-038).
// Kullanıcının ev ağı şifresi burada da, başka hiçbir ekranda da GÖSTERİLMEZ
// — `UiModel` içinde onun için alan bile yoktur.
//
// Üç bilgi ve tek bir sonraki adım: ağ adı, şifre, adres. Fazlası 128×64'te
// okunmaz.
void drawSetup(const UiModel& m)
{
    hal::oled::fillHighlight(0, L::BODY_Y - 2, L::W, 12);
    text(28, L::ROW0, "KURULUM", true);

    // ── BAĞLANTI KURULDU ───────────────────────────────────────────────────
    //
    // Ağ adı ve kurulum şifresi artık ÖLÜ BİLGİ: o AP birazdan kapanacak.
    // Yerine kullanıcının bundan sonra kullanacağı adres yazılır.
    //
    // Koşul `setupReboot` DEĞİL, "bağlandı"dır. Yeniden başlatma iptal
    // edilirse (config flash'a yazılamadıysa) `setupReboot` sıfıra döner ve
    // ekran eski kurulum şifresini göstermeye GERİ DÖNÜYORDU: cihaz ev ağında,
    // ekran hâlâ kurulum diyor. Kullanıcı bağlanamadığını sanıp resetliyordu.
    if (m.staConnected != 0u || m.setupReboot != 0u)
    {
        row(L::ROW1, "Baglandi", m.ssid);
        row(L::ROW2, "Adres", m.ip);

        if (m.setupReboot != 0u)
        {
            text(L::COL_LABEL, L::ROW3, "Yeniden baslatiliyor");
            text(L::COL_LABEL, L::ROW4, "Kendi aginiza gecin");
        }
        else
        {
            text(L::COL_LABEL, L::ROW3, "Kurulum tamamlandi");
            text(L::COL_LABEL, L::ROW4, "Bas: urun sayfasi");
        }
        return;
    }

    if (m.apSsid[0] != '\0')
    {
        row(L::ROW1, "Ag", m.apSsid);
        row(L::ROW2, "Sifre", m.apPassword);
        row(L::ROW3, "Adres", m.setupUrl[0] ? m.setupUrl : "192.168.4.1");
    }
    else
    {
        // AP kapalıysa gösterilecek bir kurulum bilgisi yok. Boş satırlar
        // yerine ne olduğunu söylüyoruz.
        text(L::COL_LABEL, L::ROW1, "Kurulum agi kapali");
        text(L::COL_LABEL, L::ROW2, "Ag ekranina bakin");
    }

    // Telefonu olmayan kullanıcı için çıkış yolu: basmak ürün SAYFASINA
    // götürür. Listeye girmek için orada bir kez daha basılır — kurulum
    // ekranının kullanıcıyı iki seviye birden derine indirmesi, nerede
    // olduğunu bilmediği bir moda düşürüyordu.
    text(L::COL_LABEL, L::ROW4, "Bas: urun sayfasi");
}

// ── CROP: ürün seçimi + programı başlat ────────────────────────────────────
//
// Telefonsuz kurulumun ikinci yarısı. Katalog 6 ürün, ekran 4 satır alıyor
// (son iki satır aktif ürün ve ipucu için) — `SENSORS` ile aynı kayan
// pencere.
//
// "BASLAT" **her zaman son öğedir**: kaydırmayla yeri değişmez.
void drawCrop(const UiModel& m)
{
    const int16_t rows[] = {L::ROW0, L::ROW1, L::ROW2, L::ROW3};

    const uint8_t total  = static_cast<uint8_t>(m.cropCount + 1u);   // +1 = BASLAT
    const uint8_t cursor = (m.cursor < total) ? m.cursor : static_cast<uint8_t>(total - 1u);

    // Dört satır liste, beşinci satır durum/ipucu.
    const uint8_t listRows = static_cast<uint8_t>(UI_VISIBLE_ROWS - 1u);
    const uint8_t start    = windowStart(cursor, total, listRows);

    uint8_t drawn = 0;
    for (uint8_t k = start; k < total && drawn < listRows; ++k, ++drawn)
    {
        if (k < m.cropCount)
        {
            // Uygulanmış profil işaretlenir: kullanıcı hangisinin seçili
            // olduğunu görmeden yeniden seçmek zorunda kalmamalı.
            selectableRow(rows[drawn], k, cursor, m.navFocus != 0u, m.editing != 0u,
                          m.crops[k].name, m.crops[k].active ? "AKTIF" : "");
        }
        else
        {
            // Program yürüyorsa düğme DURDUR'a döner: tek satır hem durumu
            // hem eylemi söyler.
            selectableRow(rows[drawn], k, cursor, m.navFocus != 0u, m.editing != 0u,
                          m.autoMode ? "DURDUR" : "BASLAT",
                          m.autoMode ? "calisiyor" : "kapali");
        }
    }

    // Liste dört satır (ROW0–ROW3); beşinci satır durum/ipucu içindir.
    const int16_t lastRow = L::ROW4;

    if (m.editing != 0u)
    {
        text(L::COL_LABEL, lastRow, "Onaylamak icin bas");
    }
    else if (cursor >= m.cropCount && m.cropSelected == 0u)
    {
        // ÖNCEDEN söylüyoruz. Basıp hiçbir şey olmaması, düğmenin bozuk
        // olduğu izlenimi verirdi (§12.2).
        text(L::COL_LABEL, lastRow, "Once bir urun secin");
    }
    else if (m.navFocus != 0u)
    {
        // Sayfanın içindeyken çıkış yolu, aktif programın adından daha
        // gereklidir: uygulanmış profil zaten listede "AKTIF" ile işaretli.
        text(L::COL_LABEL, lastRow, HINT_BACK);
    }
    else
    {
        text(L::COL_LABEL, lastRow, m.cropText);
    }
}

// ── Ortak çizim ────────────────────────────────────────────────────────────
void renderScreen(const UiModel& m)
{
    if (!hal::oled::isAvailable()) { return; }

    hal::oled::clear();
    hal::oled::setTextSize(1);

    drawStatusBar(m);

    switch (m.screen)
    {
        case ScreenId::HOME:      drawHome(m);      break;
        case ScreenId::SENSORS:   drawSensors(m);   break;
        case ScreenId::CONTROL:   drawControl(m);   break;
        case ScreenId::NETWORK:   drawNetwork(m);   break;
        case ScreenId::SYSTEM:    drawSystem(m);    break;
        case ScreenId::ALERTS:    drawAlerts(m);    break;
        case ScreenId::EMERGENCY: drawEmergency(m); break;
        case ScreenId::CROP:      drawCrop(m);      break;
        case ScreenId::SETUP:     drawSetup(m);     break;
        default:                  drawHome(m);      break;
    }

    (void)hal::oled::display();
}

} // namespace ui
} // namespace interfaces
