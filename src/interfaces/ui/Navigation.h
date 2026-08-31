#pragma once

// Ekran gezinme ve onay akışı — TASK-051
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
// Acil durum olunca ekran OTOMATİK `EMERGENCY`'ye geçer. Kullanıcı diğer
// ekranlara geçebilir (teşhis için gerekli), ama durum çubuğunda kalıcı
// "ACİL" rozeti kalır ve BACK her yerden `EMERGENCY`'ye döner.

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
/// YOKTUR ve onay bekleyen bir işlem varsa sayaç işlemez.
constexpr uint32_t IDLE_RETURN_MS = 60000u;

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

/// Acil duruma girildiğinde ekranı öncelikli ekrana taşır.
void onEmergency(core::Millis now);

} // namespace nav
} // namespace ui
} // namespace interfaces
