// Yapılandırma uç noktaları — TASK-044
//
//   GET /api/config                 — SIRLAR MASKELİ
//   PUT /api/config/network         — SSID/şifre/IP
//   PUT /api/config/safety          — güvenlik eşikleri
//   PUT /api/config/actuators       — aktüatör kısıtları (index GÖVDEDE)
//   PUT /api/config/automation      — mod + manuel müdahale süresi
//   GET /api/config/rules           — otomasyon kural kümesi
//   PUT /api/config/rules           — kural kümesi (BÜTÜN olarak)
//   PUT /api/config/system          — timezone, telemetri, log seviyesi
//
// ── İSTEMCİYE GÜVENİLMEZ ────────────────────────────────────────────────────
// Her gövde `ConfigValidation` (TASK-014) üzerinden geçer. İstemci tarafı
// doğrulama bir KOLAYLIKTIR: `curl` ile gönderilen istek istemci kodunu hiç
// çalıştırmaz. Doğrulama hatası ALAN ADIYLA döner — `ConfigError` zaten
// alan adını taşıyor.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <stdio.h>
#include <string.h>

#include "core/Config.h"
#include "hal/SecretStore.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/JsonBody.h"
#include "interfaces/web/RequestValidation.h"
#include "interfaces/web/StateJson.h"
#include "services/ConfigService.h"
#include "services/network/NetworkFsm.h"

namespace interfaces {
namespace web {
namespace api {
namespace {

using core::ErrCode;

constexpr size_t CONFIG_JSON_MAX = 2048;

/// Kütüphanenin `AsyncCallbackJsonWebHandler` sınıfı KULLANILMIYOR —
/// gerekçe `JsonBody.h` başlığında.
void onJsonPut(AsyncWebServer& server, const char* path, JsonBodyFn fn)
{
    onJsonBody(server, path, HTTP_PUT, fn);
}

/// `ConfigError`'ı ortak hata şemasına çevirir.
void sendConfigResult(AsyncWebServerRequest* req, const core::ConfigError& e)
{
    if (e.code == ErrCode::OK) { sendOk(req); return; }
    sendError(req, e.code, e.field);
}

/// Kural kümesi yanıtı için tampon. 8 kural × ~200 bayt + sarmalayıcı.
/// `CONFIG_JSON_MAX`'tan ayrı: kurallar `/api/config` yanıtına konsaydı iki
/// içerik tek tamponu paylaşır ve biri büyüdüğünde diğeri sessizce taşardı.
constexpr size_t RULES_JSON_MAX = 2560;

/// Kural alanı için hata adı: `rules[3].sensor`.
///
/// `ConfigError::field` normalde .rodata sabitidir (ayırma yok). Burada
/// indeks gerektiği için tek bir dosya-statik tampon kullanılır: yanıt aynı
/// çağrı içinde gönderilir ve HTTP işleyicileri tek bağlamda (AsyncTCP)
/// koşar, dolayısıyla ikinci bir okuyucu yoktur.
const char* ruleField(uint8_t index, const char* leaf)
{
    static char buf[40];
    snprintf(buf, sizeof(buf), "rules[%u].%s", static_cast<unsigned>(index), leaf);
    return buf;
}

/// JSON'dan sınırlı bir tam sayı okur.
///
/// Aralık dışı değeri **kırpmaz, reddeder**: 70000 dakikalık bir pencere
/// sessizce 4464'e dönseydi, kullanıcı yazdığından bambaşka bir kuralı
/// kaydedilmiş sanırdı.
bool readBounded(JsonVariantConst v, int32_t dflt, int32_t hi, int32_t& out)
{
    if (v.isNull()) { out = dflt; return true; }
    if (!v.is<int32_t>()) { return false; }

    const int32_t raw = v.as<int32_t>();
    if (raw < 0 || raw > hi) { return false; }
    out = raw;
    return true;
}

} // namespace

void registerConfig(AsyncWebServer& server)
{
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        static char json[CONFIG_JSON_MAX];
        const size_t n = writeConfigJson(json, sizeof(json));
        if (n == 0) { sendError(req, ErrCode::WEB_PAYLOAD_TOO_LARGE); return; }
        sendJson(req, 200, json);
    });

    onJsonPut(server, "/api/config/network", [](AsyncWebServerRequest* req, JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        core::NetworkConfig n = services::config::get().network;

        if (json["ssid"].is<const char*>())
        {
            if (!n.ssid.assign(json["ssid"] | ""))
            {
                sendError(req, ErrCode::CFG_VALIDATION_FAILED, "network.ssid");
                return;
            }
        }

        if (json["ipMode"].is<const char*>())
        {
            n.ipMode = (strcmp(json["ipMode"] | "dhcp", "static") == 0) ? core::IpMode::STATIC
                                                                       : core::IpMode::DHCP;
        }

        // Her IP alanı ayrı ayrı doğrulanır; biri geçersizse HANGİSİ
        // olduğunu söyleriz.
        struct { const char* key; const char* field; uint32_t* dst; } ips[] = {
            {"staticIp", "network.staticIp", &n.staticIp},
            {"gateway",  "network.gateway",  &n.gateway},
            {"subnet",   "network.subnet",   &n.subnet},
            {"dns",      "network.dns",      &n.dns},
        };
        for (const auto& f : ips)
        {
            if (!json[f.key].is<const char*>()) { continue; }
            if (!parseIp(json[f.key] | "", *f.dst))
            {
                sendError(req, ErrCode::NET_IP_CONFIG_INVALID, f.field);
                return;
            }
        }

        const core::ConfigError e = services::config::updateNetwork(n);
        if (e.code != ErrCode::OK) { sendConfigResult(req, e); return; }

        // Şifre gövdede gelir, `SecretStore`'a yazılır, YANITTA DÖNMEZ.
        if (json["password"].is<const char*>())
        {
            const ErrCode rc = hal::secrets::setWifiPassword(json["password"] | "");
            if (rc != ErrCode::OK) { sendError(req, rc, "network.password"); return; }
        }

        // Kimlik hatası sayacını sıfırla: kullanıcı şifreyi düzeltmiş
        // olabilir. Sıfırlanmazsa sistem "durdu" durumunda kalır ve
        // kullanıcının bunu anlamasının hiçbir yolu olmaz (TASK-037 Karar 3).
        services::net::fsm::onCredentialsChanged();

        sendOk(req);
    });

    onJsonPut(server, "/api/config/safety", [](AsyncWebServerRequest* req, JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        core::SafetyConfig s = services::config::get().safety;
        s.flowVerifyDelayMs    = json["flowVerifyDelayMs"]    | s.flowVerifyDelayMs;
        s.flowMinRate          = json["flowMinRate"]          | s.flowMinRate;
        s.maxRuntimeGraceMs    = json["maxRuntimeGraceMs"]    | s.maxRuntimeGraceMs;
        s.maxRuntimeViolations = json["maxRuntimeViolations"] | s.maxRuntimeViolations;
        s.requireLevelSensor   = (json["requireLevelSensor"] | (s.requireLevelSensor != 0u))
                                     ? 1u : 0u;

        sendConfigResult(req, services::config::updateSafety(s));
    });

    onJsonPut(server, "/api/config/actuators", [](AsyncWebServerRequest* req,
                                                  JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        const int idx = json["index"] | -1;
        if (idx < 0 || idx >= static_cast<int>(core::MAX_ACTUATORS))
        {
            sendError(req, ErrCode::WEB_INVALID_REQUEST, "index");
            return;
        }

        core::ActuatorConfig a = services::config::get().actuators[idx];
        a.minRunMs   = json["minRunMs"]   | a.minRunMs;
        a.maxRunMs   = json["maxRunMs"]   | a.maxRunMs;
        a.cooldownMs = json["cooldownMs"] | a.cooldownMs;
        a.enabled    = (json["enabled"] | (a.enabled != 0u)) ? 1u : 0u;

        sendConfigResult(req,
                         services::config::updateActuator(static_cast<uint8_t>(idx), a));
    });

    onJsonPut(server, "/api/config/automation", [](AsyncWebServerRequest* req,
                                                   JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        core::AutomationConfig a = services::config::get().automation;

        if (json["mode"].is<const char*>())
        {
            a.mode = (strcmp(json["mode"] | "manual", "auto") == 0)
                         ? core::AutomationMode::AUTO
                         : core::AutomationMode::MANUAL;
        }
        a.manualOverrideMs = json["manualOverrideMs"] | a.manualOverrideMs;

        sendConfigResult(req, services::config::updateAutomation(a));
    });

    server.on("/api/config/rules", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        static char json[RULES_JSON_MAX];
        const size_t n = writeRulesJson(json, sizeof(json));
        if (n == 0) { sendError(req, ErrCode::WEB_PAYLOAD_TOO_LARGE); return; }
        sendJson(req, 200, json);
    });

    // Kural kümesi BÜTÜN olarak yazılır — tek kural yazan bir uç nokta YOK.
    // Çakışma denetimi (aynı aktüatör + aynı öncelik) yalnızca küme
    // bütününde anlamlıdır; slot slot yazmak aradaki adımlarda geçersiz bir
    // küme bırakırdı (gerekçe: `ConfigService::updateRules`).
    onJsonPut(server, "/api/config/rules", [](AsyncWebServerRequest* req, JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        JsonArrayConst arr = json["rules"].as<JsonArrayConst>();
        if (arr.isNull())
        {
            sendError(req, ErrCode::WEB_INVALID_REQUEST, "rules");
            return;
        }
        if (arr.size() > static_cast<size_t>(core::MAX_RULES))
        {
            sendError(req, ErrCode::CFG_VALIDATION_FAILED, "rules.count");
            return;
        }

        // Sıfırlanmış küme: yazılmayan her slot INACTIVE kalır. Eski bir
        // kuralın artığının yeni kümede yaşamaya devam etmesi imkânsız.
        core::RuleSet rs{};

        uint8_t i = 0;
        for (JsonVariantConst item : arr)
        {
            // Dizinin her elemanı NESNE olmalı. `rules:[1,2]` gibi bir gövde
            // aksi hâlde sessizce varsayılan kurallara dönüşürdü.
            JsonObjectConst o = item.as<JsonObjectConst>();
            if (o.isNull())
            {
                sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "object"));
                return;
            }

            core::Rule& r = rs.rules[i];
            int32_t     v = 0;

            if (!ruleKindFromName(o["kind"] | "", r.kind))
            {
                sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "kind"));
                return;
            }

            r.enabled = (o["enabled"] | true) ? 1u : 0u;

            if (!readBounded(o["priority"], 0, 255, v))
            {
                sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "priority"));
                return;
            }
            r.priority = static_cast<uint8_t>(v);

            if (!readBounded(o["minTriggerIntervalS"], 0, 65535, v))
            {
                sendError(req, ErrCode::WEB_INVALID_REQUEST,
                          ruleField(i, "minTriggerIntervalS"));
                return;
            }
            r.minTriggerIntervalS = static_cast<uint16_t>(v);

            // Boş slotun aktüatör adı taşıması zorunlu değil: INACTIVE için
            // hedef ve tür parametreleri hiç okunmaz.
            if (r.kind == core::RuleKind::INACTIVE) { ++i; continue; }

            if (!actuatorIdFromName(o["target"] | "", r.target))
            {
                sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "target"));
                return;
            }

            if (r.kind == core::RuleKind::THRESHOLD)
            {
                if (!sensorIdFromName(o["sensor"] | "", r.sensor))
                {
                    sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "sensor"));
                    return;
                }
                if (!o["onThreshold"].is<float>() || !o["offThreshold"].is<float>())
                {
                    sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "threshold"));
                    return;
                }
                r.onThreshold  = o["onThreshold"].as<float>();
                r.offThreshold = o["offThreshold"].as<float>();
            }
            else if (r.kind == core::RuleKind::SCHEDULE_WINDOW)
            {
                if (!readBounded(o["startMin"], 0, 1439, v))
                {
                    sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "startMin"));
                    return;
                }
                r.startMin = static_cast<uint16_t>(v);

                if (!readBounded(o["endMin"], 0, 1439, v))
                {
                    sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "endMin"));
                    return;
                }
                r.endMin = static_cast<uint16_t>(v);
            }
            else  // SCHEDULE_CYCLE
            {
                if (!readBounded(o["cycleOnS"], 0, 65535, v))
                {
                    sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "cycleOnS"));
                    return;
                }
                r.cycleOnS = static_cast<uint16_t>(v);

                if (!readBounded(o["cyclePeriodS"], 0, 65535, v))
                {
                    sendError(req, ErrCode::WEB_INVALID_REQUEST, ruleField(i, "cyclePeriodS"));
                    return;
                }
                r.cyclePeriodS = static_cast<uint16_t>(v);
            }

            ++i;
        }

        rs.count = i;

        // Anlamsal denetim (histerezis bandı, gece yarısını aşan pencere,
        // çevrim oranı, çakışan öncelikler) `ConfigValidation`'da yapılır ve
        // BURADA TEKRARLANMAZ — sınırların tek doğruluk kaynağı orasıdır.
        sendConfigResult(req, services::config::updateRules(rs));
    });

    onJsonPut(server, "/api/config/system", [](AsyncWebServerRequest* req, JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        core::SystemConfig s = services::config::get().system;

        if (json["timezone"].is<const char*>() && !s.timezone.assign(json["timezone"] | ""))
        {
            sendError(req, ErrCode::CFG_VALIDATION_FAILED, "system.timezone");
            return;
        }
        s.telemetryIntervalMs = json["telemetryIntervalMs"] | s.telemetryIntervalMs;
        s.logLevel            = json["logLevel"]            | s.logLevel;

        sendConfigResult(req, services::config::updateSystem(s));
    });
}

} // namespace api
} // namespace web
} // namespace interfaces
