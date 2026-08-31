#pragma once

// OLED arayüz servisi — TASK-053
//
// ── DÖNGÜ ───────────────────────────────────────────────────────────────────
//   1. girdi olaylarını işle → navigasyon güncelle
//   2. snapshot al
//   3. ViewModel üret
//   4. önceki ViewModel'den FARKLIYSA çiz
//   5. Wi-Fi LED durumunu güncelle
//   6. heartbeat · watchdog (TaskRunner)
//
// ── UI'NİN TEK ÇIKIŞI ───────────────────────────────────────────────────────
// `CommandQueue.post()`. Kullanıcı OLED'den acil durdurma yaparsa bu bir
// KOMUTTUR; `ui` task'ı röleye dokunmaz. Mod değiştirirse bu bir komuttur;
// `ui` task'ı config'e yazmaz (§13.2).
//
// ── LED AYRI TASK DEĞİL ─────────────────────────────────────────────────────
// Eski projede `Task_WifiLed` 2 KB stack ile ayrı bir task'tı. Bir LED
// yakıp söndürmek için task açmak israftır (§6.4); burada bir sayaç.
//
// ── OLED YOKSA SİSTEM ETKİLENMEZ ────────────────────────────────────────────
// `isAvailable() == false` ise çizim atlanır ama döngü döner: girdi
// işlenir, komut üretilir, heartbeat beslenir (P4 — fail-degraded).

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace interfaces {
namespace ui {

core::ErrCode begin();

/// Bir çevrim.
void tick(core::Millis now);

/// Kurulum AP bilgilerini bildirir (TASK-038).
///
/// `ui` radyoya dokunamaz; bu değerler `net` tarafından doldurulur ve
/// yalnızca gösterim için taşınır.
void setApInfo(const char* ssid, const char* password);

/// Kaç kez yeniden çizildi — tanılama.
uint32_t redrawCount();

} // namespace ui
} // namespace interfaces
