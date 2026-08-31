#pragma once

// Röle çıkış sürücüsü — TASK-017
//
// BU DOSYA KURU ÇALIŞMAYA KARŞI İLK SAVUNMA HATTIDIR.
//
// Rölelerin boot'un ilk milisaniyelerinden itibaren güvenli konumda olmasını
// sağlar ve röle sürüşünü tek bir kapı arkasına alır (ARCHITECTURE P2).
//
// SÜRÜCÜDE İŞ KURALI YOKTUR (D6): min/max çalışma süresi, cooldown, tahkim
// ve güvenlik kilitleri burada DEĞİL, `ActuatorManager` (TASK-029) ve
// `SafetyMonitor` (TASK-030) içindedir.
//
// ── ISSUE-003: YAZILIMLA KAPATILAMAYAN PENCERE ──────────────────────────────
// ESP32 GPIO'ları reset'ten sonra ve bootloader boyunca (yüzlerce ms) yüksek
// empedanstadır. Aktif-düşük bir röle modülünde bu, RÖLENİN ÇEKİLİ olması
// yani POMPANIN ÇALIŞMASI demek olabilir.
//
// Yazılım en erken `setup()` başında müdahale edebilir. Bu pencere ancak
// DONANIMLA kapatılır: röle giriş hattına harici pull-up (aktif-düşük modül)
// veya pull-down (aktif-yüksek modül) gerekir. `docs/HARDWARE.md`'de zorunlu
// madde olarak kayıtlıdır.
// ────────────────────────────────────────────────────────────────────────────

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/SystemState.h"

namespace hal {

namespace relay {

/// Röleleri güvenli seviyeye alır ve çıkış olarak yapılandırır.
///
/// **Boot Aşama 1'de, diğer her şeyden önce çağrılmalıdır** (ARCHITECTURE §7.1)
/// — log altyapısı bile henüz hazır olmayabilir. Pompanın korunması, log
/// altyapısının hazır olmasından önceliklidir.
///
/// GLITCH'SİZ SIRA: önce çıkış yazmacına güvenli seviye yazılır, SONRA pin
/// çıkışa alınır. Ters sırada pin bir an önceki (tanımsız) seviyeyi sürerdi.
core::ErrCode begin();

/// Röleyi açar veya kapatır.
///
/// Fiziksel seviye `RELAY_ACTIVE_LOW` sabitine göre çevrilir; çağıran
/// polariteyi bilmez, yalnızca "açık/kapalı" der.
///
/// Bu fonksiyon **yalnızca `ActuatorManager` tarafından, `app_core` task'ından**
/// çağrılmalıdır (ARCHITECTURE P2 — donanıma tek kapı).
core::ErrCode set(core::ActuatorId id, bool on);

/// Rölenin GERÇEK durumunu okur.
///
/// Yazılımda tutulan gölge değişkene değil, `digitalRead()` ile fiziksel pin
/// seviyesine bakar. Talep ile gerçek arasındaki fark bir hata göstergesidir
/// ve `ActuatorManager` bunu `ACTUATOR_STATE_MISMATCH` olarak raporlar.
bool isOn(core::ActuatorId id);

/// TÜM röleleri güvenli duruma alır.
///
/// Acil durum yolundan çağrılır (TASK-012, TASK-032). Hızlıdır, bloklamaz ve
/// **hiçbir kısıt tanımaz** — `minRunTime` acil durumda geçersizdir.
core::ErrCode allSafe();

/// Bu aktüatör kimliğinin fiziksel bir rölesi var mı?
bool isMapped(core::ActuatorId id);

} // namespace relay
} // namespace hal
