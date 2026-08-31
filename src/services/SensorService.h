#pragma once

// Sensör servisi — TASK-027
//
// ── KESİN YASAKLAR (ARCHITECTURE §2.5) ─────────────────────────────────────
// Bu servis HİÇBİR KOŞULDA:
//   · ekrana çizmez
//   · ağa bağlanmaz
//   · aktüatör tetiklemez
//   · karar vermez
//
// Mevcut sistemdeki `Sensor::SensorValues()` üçünü birden ihlal ediyordu:
// sensör okuma ile OLED çizimi aynı fonksiyondaydı (REQUIREMENTS §6.3).
// Bu servis o desenin doğrudan karşıtıdır.
// ────────────────────────────────────────────────────────────────────────────
//
// TEK YAZAR (ARCHITECTURE P1): `sensors` alt-state'ine yalnızca bu servis
// yazar, yalnızca `io_sense` task'ından.
//
// ADC TEK SAHİPLİ (P2): ADC1'e yalnızca bu servis erişir.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/Time.h"
#include "sensors/SensorPipeline.h"

namespace services {

namespace sensorsvc {

/// Sensörleri ve donanımlarını hazırlar.
///
/// Config yüklenmiş olmalıdır: kalibrasyon oradan gelir. Yüklenmemişse
/// `scale = 0` bir yapıyla her ölçüm sıfırlanırdı.
core::ErrCode begin();

/// Bir örnekleme turu. `io_sense` task döngüsünden çağrılır.
///
/// Her sensör KENDİ periyoduna göre örneklenir; hepsi her döngüde okunmaz
/// (pH/EC probları DC beslemede polarize olur, gereksiz okuma prob ömrünü
/// kısaltır).
///
/// GÜVENLİK GARANTİSİ: `isSafetyCritical` işaretli sensörler (su seviyesi,
/// akış) periyodu geldiğinde **asla atlanmaz** — yük dağıtma onları
/// erteleyemez.
///
/// Tüm sensörler işlendikten sonra **tek** `publishSensors()` çağrısı yapılır:
/// sensör başına ayrı yayınlama, okuyucuya tutarsız bir ara görüntü verirdi
/// (seviye yeni, akış eski) ve güvenlik kararı karışık veriyle alınırdı.
void tick(core::Millis now);

/// Bir sensörün son işlenmiş örneği — teşhis için.
void lastSample(core::SensorId id, core::SensorSample& out);

/// Akış sensörünün açılıştan beri ölçtüğü toplam hacim (litre).
float totalLiters();

/// Su seviyesi şamandıraları tutarsız mı? (Fiziksel olarak imkânsız
/// kombinasyon — en az biri arızalı.) `SafetyMonitor` (TASK-030) okuyacak.
bool levelSensorsInconsistent();

/// Son turda işlenen sensör sayısı — döngü yükü teşhisi.
uint8_t lastCycleSampleCount();

} // namespace sensorsvc
} // namespace services
