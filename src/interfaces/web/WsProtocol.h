#pragma once

// WebSocket protokolü — TASK-045 / TASK-046
//
// ── İYİMSER GÜNCELLEME YOK ──────────────────────────────────────────────────
// Eski sistemde kullanıcı butona bastığında arayüz kartı HEMEN değiştiriyordu;
// cihaz komutu reddetse bile arayüz "açık" gösteriyordu. Kullanıcı pompanın
// çalıştığını SANIYORDU — bu bir güvenlik sorunudur.
//
//     cmd   → istemci "BEKLİYOR"a geçer, DURUMU DEĞİŞTİRMEZ
//     ack   → komut KABUL EDİLDİ (veya reddedildi + neden)
//     state → komut UYGULANDI; kart YALNIZCA bunu görünce değişir
//
// `ack` kabulü, `state` uygulanmayı bildirir.
//
// ── YAYIN TETİKLEME (hibrit) ────────────────────────────────────────────────
//   kritik değişim (aktüatör/güvenlik/mod/ağ) → ANINDA
//   sensör değerleri                          → hız sınırlı
//
// ── BACKPRESSURE ────────────────────────────────────────────────────────────
//   `state` → DÜŞÜRÜLÜR (tam görüntüdür, sonraki paketle yetişilir)
//   `ack` / `event` → DÜŞÜRÜLMEZ (tekrarlanmaz; düşürülen ack istemciyi
//                     sonsuza kadar "BEKLİYOR"da bırakır)

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

class AsyncWebServer;

namespace interfaces {
namespace web {
namespace ws {

/// Eşzamanlı WS istemcisi üst sınırı. Her istemci RAM tüketir (yazma kuyruğu
/// + birleştirme tamponu). Sınır aşılınca yeni bağlantı REDDEDİLİR — kabul
/// edip sonra düşürmek istemciyi yeniden bağlanma döngüsüne sokar.
constexpr uint8_t MAX_CLIENTS = 4;

/// Parça birleştirme tamponu. Aşan mesaj `WEB_PAYLOAD_TOO_LARGE`.
constexpr uint16_t ASSEMBLY_MAX = 512;

/// Sunucunun ürettiği state paketinin üst sınırı.
constexpr uint16_t STATE_JSON_MAX = 2048;

/// WS uç noktasını sunucuya bağlar. `WebService::begin()` içinden çağrılır.
void attach(AsyncWebServer& server);

/// Periyodik iş: kopan istemcileri temizle, telemetri yayınla.
///
/// Eski sistemde `cleanupClients()` `loop()` içindeydi; artık `net`
/// task'ının sorumluluğunda (§6.4 — ayrı task açılmadı).
void tick(core::Millis now);

/// Bağlı istemci sayısı.
uint8_t clientCount();

/// Backpressure nedeniyle düşürülen telemetri paketi sayısı — tanılama.
uint32_t droppedTelemetry();

} // namespace ws
} // namespace web
} // namespace interfaces
