#pragma once

// `SystemState` → JSON dönüşümü — TASK-043 / TASK-045
//
// TEK SERİLEŞTİRİCİ: hem `GET /api/state` hem WS `state` paketi burayı
// kullanır. İki ayrı serileştirici yazmak, ikisinin sessizce birbirinden
// ayrılmasıyla biter — ve arayüz hangi yoldan geldiğine göre farklı davranır.
//
// ── SIRLAR BURADA YOK ───────────────────────────────────────────────────────
// Wi-Fi şifresi `SystemState` içinde zaten bulunmuyor (TASK-006 kararı);
// bu dosya `SecretStore`'a ERİŞMEZ. Sızıntı için iki bağımsız engel var.
//
// ── ÖNCEDEN BOYUTLANDIRILMIŞ ────────────────────────────────────────────────
// §14.6: büyük JSON önceden boyutlandırılır, heap parçalanması önlenir.
// Çıktı çağıranın verdiği sabit tampona yazılır; dinamik `String` yok.

#include <stddef.h>
#include <stdint.h>

#include "core/SystemState.h"

namespace interfaces {
namespace web {

/// Tam durumu JSON'a yazar.
///
/// @return yazılan bayt sayısı; tampon yetmezse 0
size_t writeStateJson(const core::SystemState& s, char* out, size_t outLen);

/// Tanılama görünümü: aktif hatalar, son olaylar, boot bilgisi.
size_t writeDiagnosticsJson(char* out, size_t outLen);

/// Yapılandırma görünümü — **sırlar maskeli**.
size_t writeConfigJson(char* out, size_t outLen);

/// Tarama sonucu. Şema **tüm durumlarda aynı**: `{status, age, truncated,
/// networks[]}`. Eski sistemin "tarama sürerken farklı şekilli yanıt"
/// hatası istemci tarafında "ilk tıklama her zaman başarısız" olarak
/// görünüyordu.
size_t writeScanJson(core::Millis now, char* out, size_t outLen);

/// Sensör kimliği → kısa ad. Sunum katmanının işi.
const char* sensorName(core::SensorId id);

/// Aktüatör kimliği → kısa ad.
const char* actuatorName(core::ActuatorId id);

/// Sensör kalitesi → ad.
const char* qualityName(core::SensorQuality q);

/// Sistem modu → ad.
const char* modeName(core::SystemMode m);

/// Ağ durumu → ad.
const char* netStateName(core::NetState s);

} // namespace web
} // namespace interfaces
