#pragma once

// Ağ bağlantı durum makinesi — TASK-035
//
// ── FSM (ARCHITECTURE §8.1) ─────────────────────────────────────────────────
//
//                        ┌──────────────┐
//                        │     BOOT     │
//                        └──────┬───────┘
//                       credential var?
//              hayir ┌──────────┴────────┐ evet
//                    ▼                   ▼
//            ┌─────────────┐      ┌──────────────┐
//            │  AP_ONLY    │      │  CONNECTING  │◀────────┐
//            └──────┬──────┘      └──────┬───────┘         │
//                   │           basari ┌─┴─┐ basarisiz     │
//                   │                  ▼   ▼               │
//                   │        ┌───────────┐ ┌──────────────┐│
//                   │        │ CONNECTED │ │   BACKOFF    │┘
//                   │        └─────┬─────┘ └──────┬───────┘
//                   │       kopma  │              │ 90 sn gecti
//                   │              └──────────────┤
//                   │                             ▼
//                   │                   ┌────────────────────┐
//                   └──────────────────▶│    AP_FALLBACK     │
//                                       └────────────────────┘
//
// ── BLOKLAMA YASAĞI — MUTLAK ────────────────────────────────────────────────
// Hiçbir durumda `while (WiFi.status() != WL_CONNECTED) delay(...)` benzeri
// bir bekleme YOKTUR. `CONNECTING`'de task normal periyoduyla döner; sonuç
// event olarak gelir.
//
// Eski sistemde `connect(5000)` task'ı 5 saniye blokluyordu — ve bu, watchdog
// beslemesinden SONRA yapıldığı için watchdog tarafından da görülmüyordu.
//
// ── BAĞIMSIZLIK ─────────────────────────────────────────────────────────────
// `net` task'ı tamamen kilitlense bile aktüatör kontrolü ve güvenlik
// kilitleri çalışmaya devam eder: bu modül `StateStore`'a yalnızca YAZAR,
// `app_core` ondan bağımsız çalışır (ARCHITECTURE §16.3).

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/Time.h"
#include "services/network/NetworkState.h"

namespace services {
namespace net {
namespace fsm {

/// Alt modülleri (radyo, bağlantı, AP, tarama) başlatır.
core::ErrCode begin(const core::Config& cfg);

/// Bir FSM çevrimi: olayları tüket → zamanlayıcıları kontrol et → yayınla.
void tick(core::Millis now);

/// Kullanıcının "şimdi dene" komutu — backoff'u **ve** kimlik hatası
/// durdurmasını atlar. Kullanıcı sorunu düzelttiğinde 60 saniye beklemek
/// zorunda kalmamalı.
void requestRetryNow();

/// Yeni credential girildi: kimlik hatası sayacı ve durdurma sıfırlanır.
///
/// Aksi hâlde kullanıcı şifreyi düzeltir ama sistem hâlâ durmuş olur — ve
/// bunu anlamanın hiçbir yolu olmaz.
void onCredentialsChanged();

core::NetState state();

/// İç durum — tanılama ve host testi için.
const NetworkRuntime& runtime();

} // namespace fsm
} // namespace net
} // namespace services
