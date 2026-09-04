#include "interfaces/web/WsProtocol.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <memory>
#include <vector>
#include <string.h>

#include "core/Command.h"
#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "interfaces/web/AuthService.h"
#include "interfaces/web/StateJson.h"
#include "services/ConfigService.h"

namespace interfaces {
namespace web {
namespace ws {
namespace {

using core::Command;
using core::CommandSource;
using core::CommandType;
using core::ErrCode;
using core::Millis;

AsyncWebSocket g_ws("/ws");

/// Bağlantı başına durum. Yetki ve parça birleştirme burada.
struct ClientSlot
{
    uint32_t id;
    uint16_t assembled;
    char     buf[ASSEMBLY_MAX];
    bool     used;
    bool     authorized;
};

ClientSlot g_slots[MAX_CLIENTS];

uint32_t g_lastVersion  = 0;
Millis   g_lastPublish{0};
uint32_t g_dropped      = 0;

ClientSlot* slotOf(uint32_t id)
{
    for (uint8_t i = 0; i < MAX_CLIENTS; ++i)
    {
        if (g_slots[i].used && g_slots[i].id == id) { return &g_slots[i]; }
    }
    return nullptr;
}

ClientSlot* claimSlot(uint32_t id)
{
    for (uint8_t i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!g_slots[i].used)
        {
            memset(&g_slots[i], 0, sizeof(g_slots[i]));
            g_slots[i].used = true;
            g_slots[i].id   = id;
            return &g_slots[i];
        }
    }
    return nullptr;
}

void releaseSlot(uint32_t id)
{
    ClientSlot* s = slotOf(id);
    if (s != nullptr) { memset(s, 0, sizeof(*s)); }
}

/// `ack` gönderir. **ASLA DÜŞÜRÜLMEZ** — tekrarlanmaz; düşürülen bir ack
/// istemciyi sonsuza kadar "BEKLİYOR" durumunda bırakır.
void sendAck(AsyncWebSocketClient* c, const char* reqId, core::CommandResult r,
             core::ErrCode reason)
{
    if (c == nullptr) { return; }

    char body[160];
    snprintf(body, sizeof(body),
             "{\"type\":\"ack\",\"reqId\":\"%s\",\"result\":%u,\"reason\":%u}",
             (reqId != nullptr) ? reqId : "", static_cast<unsigned>(r),
             static_cast<unsigned>(reason));
    c->text(body);
}

/// BAĞLANTI anındaki tam state paketini gönderir.
///
/// **Yalnızca AsyncTCP bağlamından** (`WS_EVT_CONNECT`) çağrılır; orada
/// `client` işaretçisi kütüphanenin kendi olay akışından gelir ve o çağrı
/// süresince geçerlidir. Periyodik yayın bu yolu KULLANMAZ — `tick()`
/// `textAll()` kullanır (bkz. oradaki gerekçe).
///
/// Bu paket DÜŞÜRÜLMEZ: sayfa yenilendiğinde gerçek durum anında görünmeli.
void sendFullState(AsyncWebSocketClient* c, const char* json, size_t len)
{
    if (c != nullptr) { c->text(json, len); }
}

/// Bir komut mesajını işler: DOĞRULA → KUYRUĞA KOY → DÖN.
///
/// Bu fonksiyon AsyncTCP task bağlamında çalışır. Burada GPIO sürülmez,
/// flash okunmaz, beklenmez. Eski sistemde tam bu noktada `digitalWrite()`
/// çağrılıyordu.
void handleCommand(AsyncWebSocketClient* c, const char* payload, size_t len, Millis now)
{
    JsonDocument doc;

    // GERÇEK JSON ayrıştırıcı. Eski sistemdeki `msg.indexOf("\"id\":")`
    // yaklaşımı geçersiz mesajda tanımsız davranış üretiyordu.
    const DeserializationError e = deserializeJson(doc, payload, len);
    if (e)
    {
        sendAck(c, "", core::CommandResult::REJECTED_INVALID, ErrCode::WEB_INVALID_REQUEST);
        return;
    }

    const char* type  = doc["type"] | "";
    const char* reqId = doc["reqId"] | "";

    if (strcmp(type, "cmd") != 0)
    {
        sendAck(c, reqId, core::CommandResult::REJECTED_INVALID, ErrCode::WEB_INVALID_REQUEST);
        return;
    }

    Command cmd{};
    cmd.issuedAt = now;
    cmd.source   = CommandSource::WEB;
    cmd.reqId    = static_cast<uint32_t>(doc["seq"] | 0);

    const char* target = doc["target"] | "";
    const char* action = doc["action"] | "";

    if (strcmp(action, "on") == 0 || strcmp(action, "off") == 0)
    {
        cmd.type  = CommandType::SET_ACTUATOR;
        cmd.param = (strcmp(action, "on") == 0) ? 1 : 0;

        // TEK SÖZLÜK (StateJson.h): burada elle yazılmış bir isim zinciri
        // vardı ve `actuatorName()` ile İKİ AYRI doğruluk kaynağı oluşuyordu.
        // Yeni bir aktüatör eklendiğinde biri güncellenip diğeri unutulursa
        // hata ancak sahada "buton hiçbir şey yapmıyor" olarak görünürdü.
        core::ActuatorId id = core::ActuatorId::NONE;
        cmd.target = actuatorIdFromName(target, id)
                         ? static_cast<uint8_t>(id)
                         : static_cast<uint8_t>(core::ActuatorId::NONE);

        if (cmd.target == static_cast<uint8_t>(core::ActuatorId::NONE))
        {
            sendAck(c, reqId, core::CommandResult::REJECTED_INVALID,
                    ErrCode::WEB_INVALID_REQUEST);
            return;
        }
    }
    else if (strcmp(action, "emergencyStop") == 0)
    {
        // GARANTİLİ YOL: kuyruğu tamamen atlar (TASK-008). Kuyruk doluyken
        // bile acil durdurma ulaşır.
        core::cmdq::postEmergencyStop(CommandSource::WEB, ErrCode::SAFETY_EMERGENCY_LATCHED);
        sendAck(c, reqId, core::CommandResult::ACCEPTED, ErrCode::OK);
        return;
    }
    else if (strcmp(action, "emergencyClear") == 0)
    {
        cmd.type = CommandType::EMERGENCY_CLEAR;
    }
    else if (strcmp(action, "restart") == 0)
    {
        cmd.type = CommandType::SYSTEM_RESTART;
    }
    else
    {
        sendAck(c, reqId, core::CommandResult::REJECTED_INVALID, ErrCode::WEB_INVALID_REQUEST);
        return;
    }

    // Kuyruk doluysa SESSİZ YUTMA YOK — istemci `BUSY` görür.
    const core::CommandResult posted = core::cmdq::post(cmd);
    if (posted != core::CommandResult::ACCEPTED)
    {
        sendAck(c, reqId, posted, ErrCode::WEB_BUSY);
        return;
    }

    // `ack` KABULÜ bildirir. UYGULANMAYI `state` paketi bildirir.
    // İstemci kartı yalnızca `state` gelince değiştirir (TASK-048).
    sendAck(c, reqId, core::CommandResult::ACCEPTED, ErrCode::OK);
}

void onFrame(AsyncWebSocketClient* c, AwsFrameInfo* info, const uint8_t* data, size_t len)
{
    ClientSlot* s = slotOf(c->id());
    if (s == nullptr || !s->authorized) { return; }
    if (info->opcode != WS_TEXT) { return; }

    // PARÇA BİRLEŞTİRME gerçekten yapılıyor. Eski sistemde `info->final`
    // kontrol ediliyordu ama parçalar birleştirilmiyordu: uzun bir mesaj
    // sessizce bozuluyordu.
    if (s->assembled + len >= ASSEMBLY_MAX)
    {
        s->assembled = 0;
        sendAck(c, "", core::CommandResult::REJECTED_INVALID, ErrCode::WEB_PAYLOAD_TOO_LARGE);
        return;
    }

    memcpy(s->buf + s->assembled, data, len);
    s->assembled += static_cast<uint16_t>(len);

    const bool complete = (info->final != 0) && (info->index + len == info->len);
    if (!complete) { return; }

    s->buf[s->assembled] = '\0';
    handleCommand(c, s->buf, s->assembled, Millis{millis()});
    s->assembled = 0;
}

void onEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg,
             uint8_t* data, size_t len)
{
    switch (type)
    {
        case WS_EVT_CONNECT:
        {
            ClientSlot* s = claimSlot(client->id());
            if (s == nullptr)
            {
                // Sınır aşıldı: REDDET. Kabul edip sonra düşürmek istemciyi
                // yeniden bağlanma döngüsüne sokar.
                client->close(1013, "too many clients");
                return;
            }

            // YETKİ EL SIKIŞMASINDA doğrulandı (`handleHandshake`); yetkisiz
            // bağlantı buraya HİÇ ULAŞMAZ — kurulmadan reddedilir.
            s->authorized = true;

            // Kuyruk dolduğunda bağlantıyı KAPATMA, paketi DÜŞÜR.
            // Varsayılan zaten `false` ama bu davranış pazarlıksız:
            // yavaş bir istemcinin bağlantısını koparmak, onu yeniden
            // bağlanma döngüsüne sokar ve durumu daha da kötüleştirir.
            client->setCloseClientOnQueueFull(false);

            // BAĞLANTIDA TAM STATE: sayfa yenilendiğinde gerçek durum ANINDA
            // görünmeli. Bu paket düşürülmez.
            core::SystemState snap{};
            (void)core::state::snapshot(snap);

            static char json[STATE_JSON_MAX];
            const size_t n = writeStateJson(snap, json, sizeof(json));
            if (n > 0) { sendFullState(client, json, n); }
            break;
        }

        case WS_EVT_DISCONNECT:
        case WS_EVT_ERROR:
            releaseSlot(client->id());
            break;

        case WS_EVT_DATA:
            onFrame(client, static_cast<AwsFrameInfo*>(arg), data, len);
            break;

        default:
            break;
    }
}

/// Kritik değişim mi? Aktüatör, güvenlik, mod veya ağ durumu değiştiyse
/// yayın ANINDA yapılır — hız sınırı beklenmez.
///
/// Kullanıcı pompayı açtığında kartın 1 saniye sonra güncellenmesi,
/// komutun çalışmadığı izlenimi verir ve kullanıcı tekrar basar.
bool criticalChanged(const core::SystemState& a, const core::SystemState& b)
{
    if (a.system.mode != b.system.mode) { return true; }
    if (a.safety.interlockMask != b.safety.interlockMask) { return true; }
    if (a.safety.emergencyLatched != b.safety.emergencyLatched) { return true; }
    if (a.network.state != b.network.state) { return true; }

    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        if (a.actuators.items[i].isOn != b.actuators.items[i].isOn) { return true; }
        if (a.actuators.items[i].blockReason != b.actuators.items[i].blockReason)
        {
            return true;
        }
    }
    return false;
}

core::SystemState g_lastSnap{};

} // namespace

void attach(AsyncWebServer& server)
{
    memset(g_slots, 0, sizeof(g_slots));

    // YETKİ BAĞLANTI SEVİYESİNDE — ve el sıkışmasında. Yetkisiz bağlantı
    // KABUL EDİLMEZ: kurulup sonra kapatılmaz, hiç kurulmaz.
    //
    // Token sorgu parametresiyle taşınıyor: tarayıcının WebSocket API'si
    // özel başlık göndermeye izin vermez. Her mesajda token taşımak bant
    // genişliği ve sabit zamanlı karşılaştırma maliyeti getirir, güvenlik
    // kazancı yoktur.
    g_ws.handleHandshake([](AsyncWebServerRequest* req) -> bool {
        if (req == nullptr || !req->hasParam("token")) { return false; }
        const bool ok = auth::validate(req->getParam("token")->value().c_str(),
                                       Millis{millis()});
        if (!ok)
        {
            core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_UNAUTHORIZED, 0,
                            "yetkisiz ws el sikismasi reddedildi");
        }
        return ok;
    });

    g_ws.onEvent(onEvent);
    server.addHandler(&g_ws);
}

void tick(Millis now)
{
    // Kopan istemcileri temizle. Eski sistemde bu `loop()` içindeydi;
    // artık `net` task'ının işi.
    g_ws.cleanupClients();

    if (g_ws.count() == 0u) { return; }

    core::SystemState snap{};
    (void)core::state::snapshot(snap);

    if (snap.version == g_lastVersion) { return; }

    uint32_t interval = services::config::get().system.telemetryIntervalMs;

    // §16.3: heap kritikse telemetri hızı DÜŞÜRÜLÜR. Her paket bir
    // `shared_ptr<vector>` tahsisi demek; heap daralırken bunu saniyede
    // bir yapmak durumu kötüleştirir.
    if (snap.system.freeHeapBytes < core::LOW_HEAP_BYTES) { interval *= 4u; }
    const bool     urgent   = criticalChanged(snap, g_lastSnap);

    // HİBRİT TETİKLEME: kritik değişim anında, sensör değerleri hız sınırlı.
    if (!urgent && !core::hasElapsed(now, g_lastPublish, core::millisecs(interval)))
    {
        return;
    }

    static char json[STATE_JSON_MAX];
    const size_t n = writeStateJson(snap, json, sizeof(json));
    if (n == 0)
    {
        core::diag::log(core::LogLevel::ERROR, ErrCode::WEB_PAYLOAD_TOO_LARGE, 0,
                        "state json tampona sigmadi");
        return;
    }

    // ── YAYIN `textAll()` İLE — kendi döngümüzle DEĞİL ─────────────────────
    //
    // `AsyncWebSocket::client(id)` kilidi ALIP BIRAKTIKTAN sonra
    // `std::list<AsyncWebSocketClient>` içine HAM İŞARETÇİ döndürür. Bu
    // fonksiyon `net` task'ından çağrılıyor; aradan AsyncTCP task'ı
    // `_handleDisconnect()` → `_clients.erase()` çalıştırırsa işaretçi
    // SERBEST BIRAKILMIŞ belleği gösterir (kütüphane kaynağı
    // `AsyncWebSocket.cpp:942-951`).
    //
    // `textAll()` tüm iterasyonu `_ws_clients_lock` ALTINDA yapar ve
    // `c.text(buffer)`'ı orada çağırır — kopma yarışı yapısal olarak
    // imkânsız hâle gelir.
    //
    // Backpressure korunuyor: `_queueMessage()` kuyruk doluyken paketi
    // DÜŞÜRÜR ve `false` döner (`_closeWhenFull = false`); `SendStatus`
    // bunu `PARTIALLY_ENQUEUED`/`DISCARDED` olarak bildirir. Beklemiyoruz —
    // AsyncTCP task'ında beklemek tüm web sunucusunu dondururdu (§14.6).
    auto buf = std::make_shared<std::vector<uint8_t>>(
        reinterpret_cast<const uint8_t*>(json), reinterpret_cast<const uint8_t*>(json) + n);

    const AsyncWebSocket::SendStatus st = g_ws.textAll(buf);
    if (st != AsyncWebSocket::ENQUEUED) { ++g_dropped; }

    g_lastVersion = snap.version;
    g_lastSnap    = snap;
    g_lastPublish = now;
}

uint8_t  clientCount()      { return static_cast<uint8_t>(g_ws.count()); }
uint32_t droppedTelemetry() { return g_dropped; }

} // namespace ws
} // namespace web
} // namespace interfaces
