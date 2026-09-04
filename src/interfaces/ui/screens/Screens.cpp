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

void text(int16_t x, int16_t y, const char* s) { hal::oled::drawText(x, y, s); }

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
/// uzerine binecek kadar uzunsa etiketin hemen sagindan baslar (kirpilir
/// ama etiket okunur kalir).
void row(int16_t y, const char* label, const char* value)
{
    text(L::COL_LABEL, y, label);

    if (value == nullptr || *value == 0) { return; }

    const int16_t labelEnd = static_cast<int16_t>(
        L::COL_LABEL + hal::oled::textWidth(label, 1) + 4);
    const int16_t valueW = static_cast<int16_t>(hal::oled::textWidth(value, 1));

    int16_t x = static_cast<int16_t>(L::W - valueW);
    if (x < labelEnd) { x = labelEnd; }

    text(x, y, value);
}

/// Seçilebilir satır. İmleç `>` ile gösterilir; onay bekleniyorsa `?`.
void selectableRow(int16_t y, uint8_t index, uint8_t cursor, bool confirming,
                   const char* label, const char* value)
{
    if (index == cursor)
    {
        text(L::CURSOR_X, y, confirming ? "?" : ">");
    }
    row(y, label, value);
}

/// Wi-Fi sinyal çubukları — 0 çubuk = bağlı değil.
void drawBars(int16_t x, int16_t y, uint8_t bars)
{
    for (uint8_t i = 0; i < 4u; ++i)
    {
        const int16_t h = static_cast<int16_t>(2 + i * 2);
        hal::oled::drawRect(static_cast<int16_t>(x + i * 3), static_cast<int16_t>(y + 8 - h),
                            2, h, i < bars);
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
    if (m.emergency != 0u)
    {
        const int16_t w = 34;
        const int16_t x = static_cast<int16_t>(rightEdge - w);
        hal::oled::drawRect(x, L::STATUS_Y - 1, w, 10, true);
        text(static_cast<int16_t>(x + 2), L::STATUS_Y, "ACIL");
    }
    else
    {
        const int16_t mw = static_cast<int16_t>(hal::oled::textWidth(m.modeText, 1));
        int16_t       x  = static_cast<int16_t>(rightEdge - mw);
        if (x < 62) { x = 62; }   // "AP" göstergesinin üzerine binmesin
        text(x, L::STATUS_Y, m.modeText);
    }

    hal::oled::drawLine(0, L::SEP_Y, L::W - 1, L::SEP_Y);
}

// ── HOME: özet ──────────────────────────────────────────────────────────────
void drawHome(const UiModel& m)
{
    // ── KURULUM MODU: bağlantı bilgisi HOME'DA ─────────────────────────────
    //
    // AP açıkken kullanıcının tek ihtiyacı bu bilgidir. Onu yalnızca NETWORK
    // ekranına koymak, kullanıcıyı önce gezinmeye zorlar — encoder çalışmıyorsa
    // cihaza girmenin HİÇBİR YOLU KALMAZ. İlk sahada tam olarak bu yaşandı.
    if (m.apActive != 0u && m.apSsid[0] != '\0')
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
        selectableRow(rows[drawn], k, cursor, false, s.label, s.value);
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
            selectableRow(rows[drawn], k, cursor, m.editing != 0u, m.actuators[k].label,
                          m.actuators[k].on ? "ACIK" : "kapali");
        }
        else
        {
            selectableRow(rows[drawn], k, cursor, m.editing != 0u, "ACIL DURDUR", "");
        }
    }

    // Engel nedeni GÖSTERİLİR. Sessizce çalışmayan bir buton, kullanıcıyı
    // "sistem bozuk" sanmaya iter (§12.2 gözlemlenebilirlik).
    const int16_t lastRow = rows[UI_VISIBLE_ROWS - 1u];

    if (m.editing != 0u)
    {
        text(L::COL_LABEL, lastRow, "Onaylamak icin bas");
    }
    else if (cursor < UI_ACTS && m.actuators[cursor].blocked != 0u)
    {
        text(L::COL_LABEL, lastRow, m.actuators[cursor].why);
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

    selectableRow(L::ROW3, 0, m.cursor, m.editing != 0u, "Yeniden baslat", "");
    if (m.editing != 0u) { text(L::COL_LABEL, L::ROW4, "Onaylamak icin bas"); }
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
    hal::oled::drawRect(0, L::BODY_Y - 2, L::W, 12, true);
    text(24, L::ROW0, "ACIL DURUM");

    text(L::COL_LABEL, L::ROW1, m.emergencyWhy[0] ? m.emergencyWhy : "neden bilinmiyor");
    text(L::COL_LABEL, L::ROW2, "Aktuatorler kesildi");

    // Koşullar düzelmeden onay reddedilir (TASK-032); kullanıcıya bunu
    // önceden söylüyoruz ki reddi bir arıza sanmasın.
    text(L::COL_LABEL, L::ROW3, "Once sorunu giderin");
    selectableRow(L::ROW4, 0, m.cursor, m.editing != 0u, "Onayla ve temizle", "");
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
        default:                  drawHome(m);      break;
    }

    (void)hal::oled::display();
}

} // namespace ui
} // namespace interfaces
