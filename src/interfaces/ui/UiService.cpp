#include "interfaces/ui/UiService.h"

#include <Arduino.h>
#include <string.h>

#include "core/BoardPins.h"
#include "core/Command.h"
#include "core/CommandQueue.h"
#include "core/CropProfile.h"
#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "services/ConfigService.h"
#include "hal/InputDevices.h"
#include "hal/OledPanel.h"
#include "interfaces/ui/Navigation.h"
#include "interfaces/ui/ScreenFramework.h"
#include "interfaces/ui/ViewModelBuilder.h"

namespace interfaces {
namespace ui {
namespace {

using core::ErrCode;
using core::Millis;

UiModel g_model{};
UiModel g_lastDrawn{};
bool    g_hasDrawn   = false;
bool    g_lastEmerg  = false;
uint32_t g_redraws   = 0;

/// Kurulum ekranı BİR KEZ açılır.
///
/// Her turda "kurulum gerekli mi" diye bakıp ekranı oraya taşımak, kullanıcı
/// başka bir ekrana geçtiği anda onu geri çekerdi: cihaz kullanılamaz hâle
/// gelirdi. Latch, "ilk açılışta göster" ile "sürekli dayat" arasındaki farkı
/// tutan şeydir.
bool g_setupShown = false;

char g_apSsid[TEXT_MAX]    = {0};
char g_apPassword[16]      = {0};

// --- Wi-Fi durum LED'i (ayrı task DEĞİL — §6.4) ----------------------------

Millis  g_ledAt{0};
bool    g_ledOn = false;

/// LED desenini ağ durumundan türetir.
///
///   bağlı       → sürekli yanık
///   bağlanıyor  → 500 ms yanıp söner
///   AP modu     → 1500 ms'de bir kısa çakma
///   bağlı değil → sönük
void updateLed(const core::SystemState& s, Millis now)
{
    const core::NetState st = s.network.state;

    if (st == core::NetState::CONNECTED)
    {
        if (!g_ledOn) { digitalWrite(board::STATUS_LED, HIGH); g_ledOn = true; }
        return;
    }

    if (st == core::NetState::AP_ONLY || s.network.apActive != 0u)
    {
        // Kısa çakma: 1500 ms'de bir 80 ms yanar.
        const uint32_t e = core::elapsed(now, g_ledAt).ms;
        if (!g_ledOn && e >= 1500u)
        {
            digitalWrite(board::STATUS_LED, HIGH); g_ledOn = true; g_ledAt = now;
        }
        else if (g_ledOn && e >= 80u)
        {
            digitalWrite(board::STATUS_LED, LOW); g_ledOn = false; g_ledAt = now;
        }
        return;
    }

    if (st == core::NetState::CONNECTING || st == core::NetState::BACKOFF)
    {
        if (core::hasElapsed(now, g_ledAt, core::millisecs(500)))
        {
            g_ledOn = !g_ledOn;
            digitalWrite(board::STATUS_LED, g_ledOn ? HIGH : LOW);
            g_ledAt = now;
        }
        return;
    }

    if (g_ledOn) { digitalWrite(board::STATUS_LED, LOW); g_ledOn = false; }
}

// --- Kullanıcı eylemi → KOMUT ----------------------------------------------

/// UI'nin dış dünyaya TEK ÇIKIŞI. Röleye, config'e veya radyoya dokunmaz.
void dispatch(const ActionRequest& a, const core::SystemState& s, Millis now)
{
    if (a.action == UiAction::NONE) { return; }

    // Acil durdurma GARANTİLİ YOL'u kullanır: kuyruk doluyken bile ulaşır.
    if (a.action == UiAction::EMERGENCY_STOP)
    {
        core::cmdq::postEmergencyStop(core::CommandSource::UI,
                                      ErrCode::SAFETY_EMERGENCY_LATCHED);
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::SAFETY_EMERGENCY_LATCHED, 0,
                        "OLED uzerinden acil durdurma");
        return;
    }

    core::Command cmd{};
    cmd.issuedAt = now;
    cmd.source   = core::CommandSource::UI;

    switch (a.action)
    {
        case UiAction::TOGGLE_ACTUATOR:
        {
            cmd.type   = core::CommandType::SET_ACTUATOR;
            cmd.target = a.param;

            // Mevcut GERÇEK duruma göre tersine çevir. İstenen durumu
            // kendimiz üretmiyoruz — snapshot'tan okuyoruz.
            cmd.param = 1;
            for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
            {
                if (static_cast<uint8_t>(s.actuators.items[i].id) == a.param)
                {
                    cmd.param = (s.actuators.items[i].isOn != 0u) ? 0 : 1;
                    break;
                }
            }
            break;
        }

        case UiAction::APPLY_CROP:
        {
            // `param` KATALOG İNDEKSİDİR; kimliğe burada çevriliyor.
            // Navigasyonun ürün tablosunu tanımaması bilinçli (bkz.
            // `UiAction::APPLY_CROP`).
            const core::CropProfile* p = core::cropAt(a.param);
            if (p == nullptr)
            {
                core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_INVALID_REQUEST,
                                a.param, "OLED: katalogda olmayan urun");
                return;
            }
            cmd.type   = core::CommandType::CROP_APPLY;
            cmd.target = static_cast<uint8_t>(p->id);
            break;
        }

        case UiAction::TOGGLE_AUTOMATION:
        {
            // ── ÜRÜN YOKKEN PROGRAM BAŞLATILMAZ ────────────────────────────
            // Kural kümesi boşken otomatik moda geçmek hiçbir şey çalıştırmaz
            // ama ekranda "calisiyor" yazar — kullanıcı sulamanın kurulduğunu
            // sanır. Ekran bu durumda zaten "Once bir urun secin" diyor;
            // burada da komutu üretmiyoruz.
            if (services::config::get().crop.crop == core::CropId::NONE)
            {
                core::diag::log(core::LogLevel::INFO, ErrCode::WEB_INVALID_REQUEST, 0,
                                "OLED: urun secilmeden program baslatilamaz");
                return;
            }

            cmd.type = core::CommandType::SET_AUTOMATION_MODE;
            // Cihazın BİLDİRDİĞİ moda göre tersine çevir — istenen durumu
            // kendimiz üretmiyoruz (aktüatörlerdeki kararın aynısı).
            cmd.param = (s.automation.mode == core::AutomationMode::AUTO) ? 0u : 1u;
            break;
        }

        case UiAction::EMERGENCY_CLEAR: cmd.type = core::CommandType::EMERGENCY_CLEAR; break;
        case UiAction::RESTART:         cmd.type = core::CommandType::SYSTEM_RESTART;  break;

        default: return;
    }

    if (core::cmdq::post(cmd) != core::CommandResult::ACCEPTED)
    {
        // Sessiz yutma yok: kuyruk doluysa kaydedilir. Kullanıcı sonucu
        // zaten bir sonraki state'te (ya da değişmemesinde) görür.
        core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_BUSY,
                        static_cast<int32_t>(cmd.type), "ui komutu kuyruga alinamadi");
    }
}

} // namespace

core::ErrCode begin()
{
    nav::begin();
    memset(&g_lastDrawn, 0, sizeof(g_lastDrawn));
    g_hasDrawn   = false;
    g_setupShown = false;

    pinMode(board::STATUS_LED, OUTPUT);
    digitalWrite(board::STATUS_LED, LOW);

    if (!hal::oled::isAvailable())
    {
        // Ekranın ölmesi cihazın kontrol edilemez hâle gelmesi demek
        // olmamalı: girdi ve komut yolu çalışmaya devam eder (P4).
        core::diag::log(core::LogLevel::WARNING, ErrCode::UI_DISPLAY_UNAVAILABLE, 0,
                        "OLED yok - arayuz cizimsiz devam ediyor");
    }
    return ErrCode::OK;
}

void setApInfo(const char* ssid, const char* password)
{
    strncpy(g_apSsid, (ssid != nullptr) ? ssid : "", sizeof(g_apSsid) - 1);
    strncpy(g_apPassword, (password != nullptr) ? password : "", sizeof(g_apPassword) - 1);
    g_apSsid[sizeof(g_apSsid) - 1]         = '\0';
    g_apPassword[sizeof(g_apPassword) - 1] = '\0';
}

void tick(Millis now)
{
    // 2) Snapshot — eylem üretimi de bu görüntüye dayanır.
    core::SystemState s{};
    (void)core::state::snapshot(s);

    const bool emergency = s.safety.emergencyLatched != 0u;

    // Acil duruma GEÇİŞ kenarında ekran otomatik değişir; operatörün
    // uyarıyı görmek için ekran değiştirmesi gerekmez.
    if (emergency && !g_lastEmerg) { nav::onEmergency(now); }
    g_lastEmerg = emergency;

    // Navigasyon `CROP` ekranında kaç satır olduğunu bilmeli. Katalog
    // sabit ama sayıyı BURADAN veriyoruz: navigasyon ürün tablosunu
    // tanımıyor ve katalog büyürse orada değişecek bir şey olmuyor.
    nav::setCropCount(core::cropCount());

    // ── İLK AÇILIŞ: KURULUM EKRANI ─────────────────────────────────────────
    //
    // Koşul "AP açık VE ürün seçilmemiş": ikisi birden ancak kutudan yeni
    // çıkmış (ya da fabrika ayarlarına dönmüş) bir cihazda doğrudur.
    // Yalnızca AP'ye baksaydık, ev ağı düşüp AP'ye geri dönen KURULU bir
    // cihaz da kurulum ekranına atlar ve kullanıcının baktığı ölçümü
    // ekrandan silerdi.
    //
    // Acil durum bu ekranı EZER: acil durumdayken kurulum göstermek,
    // uyarıyı gizlemek olurdu.
    if (!g_setupShown && !emergency && s.network.apActive != 0u &&
        services::config::get().crop.crop == core::CropId::NONE)
    {
        g_setupShown = true;
        nav::onSetupNeeded(now);
    }

    // 1) Girdi olayları → navigasyon → (varsa) komut.
    hal::InputEvent ev;
    while (hal::input::poll(ev))
    {
        dispatch(nav::handle(ev, now, emergency), s, now);
    }
    nav::tick(now, emergency);

    // 3) ViewModel — SAF dönüşüm.
    build(s, nav::screen(), nav::cursor(), nav::confirming(), g_apSsid, g_apPassword,
          g_model);

    // 4) YALNIZCA değişince çiz. Her döngüde çizmek I2C busunu ve CPU'yu
    //    boşuna harcar; girdi olayı zaten modeli değiştirir.
    if (!g_hasDrawn || !sameAs(g_model, g_lastDrawn))
    {
        renderScreen(g_model);
        g_lastDrawn = g_model;
        g_hasDrawn  = true;
        ++g_redraws;
    }

    // 5) LED — ayrı task değil, sayaç.
    updateLed(s, now);
}

uint32_t redrawCount() { return g_redraws; }

} // namespace ui
} // namespace interfaces
