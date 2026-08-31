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
constexpr ScreenId ORDER[] = {
    ScreenId::HOME, ScreenId::SENSORS, ScreenId::CONTROL,
    ScreenId::NETWORK, ScreenId::SYSTEM, ScreenId::ALERTS,
};
constexpr uint8_t ORDER_LEN = sizeof(ORDER) / sizeof(ORDER[0]);

/// Her ekranda kaç seçilebilir öğe var? 0 = seçim yok.
uint8_t itemCount(ScreenId s)
{
    return (s == ScreenId::CONTROL)   ? 3u   // su pompası · hava pompası · ACİL DURDUR
         : (s == ScreenId::SYSTEM)    ? 1u   // yeniden başlat
         : (s == ScreenId::EMERGENCY) ? 1u   // onayla
                                      : 0u;
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
        if (cursor == 0u)
        {
            return {UiAction::TOGGLE_ACTUATOR,
                    static_cast<uint8_t>(core::ActuatorId::WATER_PUMP)};
        }
        if (cursor == 1u)
        {
            return {UiAction::TOGGLE_ACTUATOR,
                    static_cast<uint8_t>(core::ActuatorId::AIR_PUMP)};
        }
        return {UiAction::EMERGENCY_STOP, 0};
    }

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

            // ENCODER_PUSH: onay akışı.
            if (items == 0u) { break; }

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

ScreenId screen()     { return g_screen; }
uint8_t  cursor()     { return g_cursor; }
bool     confirming() { return g_confirm; }

} // namespace nav
} // namespace ui
} // namespace interfaces
