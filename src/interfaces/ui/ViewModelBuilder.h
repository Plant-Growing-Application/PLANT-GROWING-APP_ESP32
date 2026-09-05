#pragma once

// Snapshot → UiModel dönüşümü — TASK-050
//
// ── SAF ─────────────────────────────────────────────────────────────────────
//   build(snapshot, screen, cursor, editing) → UiModel
//
// Yan etki yok · donanıma dokunmaz · global duruma bakmaz · aynı girdi aynı
// çıktı. Bu sayede ekran mantığı HOST TARAFINDA test edilebilir (TASK-064);
// eski projede ekran mantığı doğrudan `oled` nesnesine yazıyordu ve test
// edilmesi imkânsızdı.

#include <stdint.h>

#include "core/SystemState.h"
#include "interfaces/ui/ViewModels.h"

namespace interfaces {
namespace ui {

/// Görünüm modelini üretir.
///
/// @param nav  navigasyonun bildirdiği ekran, imleç ve mod (TASK-075)
/// @param apSsid / apPassword  kurulum AP bilgileri (TASK-038); `nullptr`
///        verilebilir — yalnızca AP açıkken anlamlıdır
void build(const core::SystemState& s, const NavState& nav, const char* apSsid,
           const char* apPassword, UiModel& out);

/// İki model aynı mı? Aynıysa **çizilmez** (I2C yükü ve titreme).
bool sameAs(const UiModel& a, const UiModel& b);

} // namespace ui
} // namespace interfaces
