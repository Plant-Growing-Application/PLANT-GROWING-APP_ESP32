// GET /api/history — TASK-059
//
//   ?count=N   son N kayıt (varsayılan 120, üst sınır `MAX_PAGE` = 240)
//
// ── SAYFALI VE SINIRLI ──────────────────────────────────────────────────────
// Sınırsız aralık sorgusu YOK. 240 kayıt = 5 760 bayt okuma ve halka en
// fazla İKİ bitişik parçaya bölündüğü için **en fazla 2 dosya açması**.
//
// `readRange()` (tüm halkayı tarayabilir) buradan ÇAĞRILMAZ.
//
// ── BİLİNEN İSTİSNA (§14.6) ─────────────────────────────────────────────────
// Okuma AsyncTCP bağlamında yapılıyor. Sınırlı hâliyle bu kabul edilebilir
// görünüyor ama **donanımda ölçülmedi** — ISSUE-022. Ölçüm kabul edilemez
// çıkarsa çözüm, `store` task'ının bir sayfayı önceden hazırlamasıdır.

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/RequestValidation.h"
#include "interfaces/web/StateJson.h"
#include "services/HistoryStore.h"

namespace interfaces {
namespace web {
namespace api {
namespace {

using core::ErrCode;
namespace hist = services::history;

constexpr uint16_t DEFAULT_COUNT = 120;

/// Sayfa tamponu STATİK: 240 × 24 = 5 760 bayt. Yığında ayırmak AsyncTCP
/// task'ının stack'ini aşardı.
hist::Record g_page[hist::MAX_PAGE];

/// Sensör dizinini ada çevirmek için sabit sıra — `Record.values[]` sırası
/// snapshot'taki sıradır.
const char* slotName(uint8_t i)
{
    static const core::SensorId ORDER[hist::SENSOR_SLOTS] = {
        core::SensorId::WATER_TEMP, core::SensorId::WATER_FLOW, core::SensorId::PH,
        core::SensorId::EC,         core::SensorId::WATER_LEVEL, core::SensorId::HUMIDITY,
    };
    return (i < hist::SENSOR_SLOTS) ? sensorName(ORDER[i]) : "?";
}

core::SensorId slotId(uint8_t i)
{
    static const core::SensorId ORDER[hist::SENSOR_SLOTS] = {
        core::SensorId::WATER_TEMP, core::SensorId::WATER_FLOW, core::SensorId::PH,
        core::SensorId::EC,         core::SensorId::WATER_LEVEL, core::SensorId::HUMIDITY,
    };
    return (i < hist::SENSOR_SLOTS) ? ORDER[i] : core::SensorId::NONE;
}

} // namespace

void registerHistory(AsyncWebServer& server)
{
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        uint16_t count = DEFAULT_COUNT;
        if (req->hasParam("count"))
        {
            const long v = req->getParam("count")->value().toInt();
            if (v <= 0) { sendError(req, ErrCode::WEB_INVALID_REQUEST, "count"); return; }
            count = (v > hist::MAX_PAGE) ? hist::MAX_PAGE : static_cast<uint16_t>(v);
        }

        const uint16_t n = hist::readRecent(g_page, count);

        // Yanıt boyutu: 240 kayıt × ~90 bayt JSON ≈ 22 KB. Önceden
        // boyutlandırılmış bir tampona sığmaz; bu yüzden `AsyncResponseStream`
        // kullanılıyor — kütüphane parçalar hâlinde gönderir.
        AsyncResponseStream* res = req->beginResponseStream("application/json");

        JsonDocument doc;
        doc["count"]   = n;
        doc["stored"]  = hist::storedCount();
        doc["total"]   = hist::totalWritten();
        doc["corrupt"] = hist::corruptCount();
        doc["errors"]  = hist::writeErrors();

        JsonArray fields = doc["fields"].to<JsonArray>();
        for (uint8_t i = 0; i < hist::SENSOR_SLOTS; ++i) { fields.add(slotName(i)); }

        JsonArray rows = doc["rows"].to<JsonArray>();
        for (uint16_t k = 0; k < n; ++k)
        {
            JsonObject o = rows.add<JsonObject>();
            o["t"] = g_page[k].epoch;

            // Saat geçersizken kaydedilen satır İŞARETLİ gelir; istemci bu
            // noktaları zaman ekseninde güvenilmez göstermeli.
            o["tv"] = (g_page[k].flags & hist::FLAG_TIME_VALID) != 0u;
            o["a"]  = g_page[k].actuatorMask;
            o["q"]  = g_page[k].qualityMask;

            JsonArray v = o["v"].to<JsonArray>();
            for (uint8_t i = 0; i < hist::SENSOR_SLOTS; ++i)
            {
                v.add(hist::unscale(slotId(i), g_page[k].values[i]));
            }
        }

        serializeJson(doc, *res);
        req->send(res);
    });
}

} // namespace api
} // namespace web
} // namespace interfaces
