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

#include <ArduinoJson.h>

#include <stddef.h>
#include <stdint.h>

#include "core/Rule.h"
#include "core/SystemState.h"

namespace interfaces {
namespace web {

/// Tam durumu JSON'a yazar.
///
/// @return yazılan bayt sayısı; tampon yetmezse 0
size_t writeStateJson(const core::SystemState& s, char* out, size_t outLen);

/// Tanılama görünümü: aktif hatalar, son olaylar, boot bilgisi.
size_t writeDiagnosticsJson(char* out, size_t outLen);

/// Yapılandırmayı bir belgeye doldurur — **sırlar maskeli**.
///
/// Tampona değil belgeye yazar, çünkü yanıt artık AKITILIYOR: sensör bölümü
/// eklenince (ISSUE-035) içerik ~2,1 KB'a çıktı ve 2 KB'lık sabit tampon
/// taşacaktı — `writeConfigJson` 0 döner, arayüz "istek çok büyük" hatası
/// alır ve **tüm ayarlar ekranı çalışmazdı**.
///
/// Tamponu 4 KB'a çıkarmak yerine akıtmayı seçtik: katalog ve geçmiş için
/// verilen kararın aynısı, ve 2 KB kalıcı `.bss` geri kazanıldı.
void fillConfigJson(JsonDocument& doc);

/// Otomasyon kural kümesi (ISSUE-021).
///
/// `/api/config` içine KONMADI: 8 kural tek başına ~1,7 KB tutar ve config
/// yanıtını ortak tamponun üstüne taşırdı. Kurallar yalnızca ayar ekranında
/// gerekiyor; her durum yenilemesinde taşınmaları da gereksiz olurdu.
size_t writeRulesJson(char* out, size_t outLen);

/// Kural türü → ad.
const char* ruleKindName(core::RuleKind k);

// --- Ad → kimlik ------------------------------------------------------------
//
// Kablo üzerindeki sözlük TEK YERDE: bu çeviriciler yukarıdaki `*Name`
// fonksiyonlarını tarar. İki yön ayrı tablolarla yazılsaydı, birine eklenen
// bir ad diğerinde unutulur ve hata ancak çalışma anında görünürdü.

bool sensorIdFromName(const char* name, core::SensorId& out);
bool actuatorIdFromName(const char* name, core::ActuatorId& out);
bool ruleKindFromName(const char* name, core::RuleKind& out);

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
