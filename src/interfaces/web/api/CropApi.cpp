// Ürün profili uç noktaları — TASK-069
//
//   GET  /api/crops         — katalog (ürünler, dönemler, hedef bantlar)
//   GET  /api/crop          — aktif seçim + o dönemin hedefleri
//   POST /api/crop/preview  — bir seçimin NE DEĞİŞTİRECEĞİ — HİÇBİR ŞEY YAZMAZ
//   PUT  /api/crop          — seçimi uygular
//
// ── NEDEN AYRI BİR ÖNİZLEME UÇ NOKTASI ──────────────────────────────────────
// Ürün seçmek mevcut kural kümesinin ÜZERİNE yazar. Onaysız yapılan böyle bir
// işlem, projenin kendi ilkesine aykırıdır (ConfigService: "sessiz varsayılana
// dönüş yok"). `preview` sayesinde arayüz "3 kural yazılacak, mevcut 2 kuralın
// üzerine" diyebilir ve kullanıcı onaylayana kadar config'e dokunulmaz.
//
// ── KATALOG NEDEN /api/config İÇİNDE DEĞİL ──────────────────────────────────
// Katalog SABİTTİR ve ~4 KB JSON tutar. Her config okumasında taşınması
// gereksizdir; ayrıca `CONFIG_JSON_MAX` tamponunu tek başına aşardı.
// Kurallar için verilen kararın (`writeRulesJson`) aynısı.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "core/Config.h"
#include "core/CropProfile.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/JsonBody.h"
#include "interfaces/web/RequestValidation.h"
#include "interfaces/web/StateJson.h"
#include "services/ConfigService.h"
#include "services/CropService.h"

namespace interfaces {
namespace web {
namespace api {
namespace {

using core::CropId;
using core::CropProfile;
using core::CropStage;
using core::ErrCode;
using core::GrowthStage;
using core::Intensity;

/// Aktif seçim ve plan yanıtlarının paylaştığı tampon.
///
/// Katalog BURADA DEĞİL: o, `AsyncResponseStream` ile akıtılır (aşağıda) —
/// 6 KB'lık statik bir tampon firmware'in en büyük `.bss` nesnesiydi ve
/// doğrudan boş heap'ten düşüyordu (TASK-072).
///
/// Kalan iki yanıt 2 KB'ın altındadır: aktif seçim ~700 bayt, plan (5 kural)
/// ~1,2 KB.
///
/// ── NEDEN TEK TAMPON ────────────────────────────────────────────────────────
/// HTTP işleyicileri tek bağlamda (AsyncTCP) sırayla koşar ve yanıt aynı çağrı
/// içinde gönderilir; ikinci bir okuyucu yoktur. Bu, `ruleField()`'in
/// `ConfigApi.cpp`'deki gerekçesiyle aynıdır.
constexpr size_t CROP_JSON_MAX = 2048;

char g_json[CROP_JSON_MAX];

/// Yoğunluk ↔ API adı. Kablo üzerindeki sözlük tek yerde.
const char* intensityKey(Intensity i)
{
    return (i == Intensity::SPARSE)   ? "sparse"
         : (i == Intensity::ABUNDANT) ? "abundant"
                                      : "normal";
}

bool intensityFromKey(const char* key, Intensity& out)
{
    if (key == nullptr) { return false; }
    if (strcmp(key, "sparse") == 0)   { out = Intensity::SPARSE;   return true; }
    if (strcmp(key, "normal") == 0)   { out = Intensity::NORMAL;   return true; }
    if (strcmp(key, "abundant") == 0) { out = Intensity::ABUNDANT; return true; }
    return false;
}

/// `{"min":x,"max":y}` — arayüzün "iyi / dikkat" bandını çizmesi için.
void writeRange(JsonObject parent, const char* key, const core::Range<float>& r)
{
    JsonObject o = parent[key].to<JsonObject>();
    o["min"] = r.min;
    o["max"] = r.max;
}

/// Bir dönemi JSON'a yazar.
void writeStage(JsonObject o, const CropStage& s, GrowthStage id)
{
    o["stage"] = core::stageKeyOf(id);
    writeRange(o, "ph", s.ph);
    writeRange(o, "ec", s.ec);
    writeRange(o, "waterTemp", s.waterTemp);
    writeRange(o, "airTemp", s.airTemp);
    writeRange(o, "humidity", s.humidity);
    o["lightMinutes"] = s.lightMinutesPerDay;
    o["durationDays"] = s.durationDays;
}

/// Üretilmiş bir kural kümesini önizleme yanıtına yazar.
///
/// `writeRulesJson` KULLANILMIYOR: o, config'teki KAYITLI kümeyi yazar.
/// Önizleme henüz yazılmamış bir kümeyi anlatır ve ikisini aynı fonksiyona
/// bağlamak, önizlemenin config'i okumasına yol açardı.
void writeGeneratedRules(JsonArray arr, const core::RuleSet& rs)
{
    for (uint8_t i = 0; i < rs.count && i < core::MAX_RULES; ++i)
    {
        const core::Rule& r = rs.rules[i];
        JsonObject        o = arr.add<JsonObject>();

        o["kind"]   = ruleKindName(r.kind);
        o["target"] = actuatorName(r.target);

        if (r.kind == core::RuleKind::THRESHOLD)
        {
            o["sensor"]       = sensorName(r.sensor);
            o["onThreshold"]  = r.onThreshold;
            o["offThreshold"] = r.offThreshold;
        }
        else if (r.kind == core::RuleKind::SCHEDULE_WINDOW)
        {
            o["startMin"] = r.startMin;
            o["endMin"]   = r.endMin;
        }
        else if (r.kind == core::RuleKind::SCHEDULE_CYCLE)
        {
            o["cycleOnS"]     = r.cycleOnS;
            o["cyclePeriodS"] = r.cyclePeriodS;
        }
    }
}

/// Aktif seçimi ve o dönemin hedeflerini yazar.
size_t writeCurrentCropJson(char* out, size_t outLen)
{
    const core::CropConfig& cc = services::config::get().crop;

    JsonDocument doc;
    doc["crop"]      = core::cropKeyOf(cc.crop);
    doc["stage"]     = core::stageKeyOf(cc.stage);
    doc["intensity"] = intensityKey(cc.intensity);
    doc["autoStage"] = cc.autoStage != 0u;

    // `plantedAt` metin DEĞİL sayı: saat dilimi dönüşümü sunum katmanının
    // (tarayıcı) işidir ve cihazın yerel saatiyle tarayıcınınki farklı olabilir.
    doc["plantedAt"] = cc.plantedAtEpoch;

    doc["daysSincePlanting"] = services::crop::daysSincePlanting();

    // Arayüz "dönem ilerlemesi duraklatıldı — cihaz saati geçersiz" diyebilsin.
    // Yalnızca `autoStage` bayrağını göstermek yanıltıcı olurdu: bayrak açık
    // ama saat geçersizken hiçbir ilerleme olmaz.
    doc["autoStageActive"] = services::crop::autoStageActive();

    if (cc.crop == CropId::CUSTOM)
    {
        doc["derivedFrom"] = core::cropKeyOf(cc.derivedFrom);
    }

    // ── HEDEF BANTLAR BİTKİYİ ANLATIR, KURALLARI DEĞİL ──────────────────────
    //
    // Kullanıcı bir kuralı elle değiştirdiğinde profil `CUSTOM`'a düşer, ama
    // saksıdaki bitki hâlâ meyve dönemindeki bir çilektir ve hâlâ pH 5.5–6.2
    // ister. Bantları da düşürseydik, tek bir çevrim süresini değiştirmenin
    // bedeli TÜM ölçüm yorumlarını ve tavsiyeleri kaybetmek olurdu — arayüz
    // sayıları gösterir ama "iyi mi kötü mü" diyemezdi.
    //
    // Bu yüzden `CUSTOM` iken bantlar TÜREDİĞİ profilden okunur. `custom`
    // bayrağı yanıtta ayrıca taşınır; arayüz kuralların artık profile ait
    // olmadığını söylemeye devam eder.
    const CropProfile* p = core::cropById(cc.crop);
    if (p == nullptr && cc.crop == CropId::CUSTOM)
    {
        p = core::cropById(cc.derivedFrom);
        doc["targetsFromProfile"] = (p != nullptr);
    }

    if (p != nullptr)
    {
        doc["name"]       = p->name;
        doc["stageCount"] = p->stageCount;
        doc["difficulty"] = p->difficulty;

        const uint8_t si =
            (static_cast<uint8_t>(cc.stage) < p->stageCount) ? static_cast<uint8_t>(cc.stage) : 0u;

        JsonObject targets = doc["targets"].to<JsonObject>();
        writeStage(targets, p->stages[si], static_cast<GrowthStage>(si));
    }

    const size_t n = serializeJson(doc, out, outLen);
    return (n > 0 && n < outLen) ? n : 0;
}

/// Gövdeden seçim alanlarını okur. Eksik alanlar MEVCUT değerden devralınır.
///
/// @return alan adıyla hata; `code == OK` ise başarılı
core::ConfigError readSelection(JsonDocument& json, CropId& id, GrowthStage& stage,
                                Intensity& intensity, int64_t& plantedAt, bool& autoStage)
{
    const core::CropConfig& cur = services::config::get().crop;

    id        = cur.crop;
    stage     = cur.stage;
    intensity = cur.intensity;
    plantedAt = cur.plantedAtEpoch;
    autoStage = cur.autoStage != 0u;

    if (json["crop"].is<const char*>())
    {
        if (!core::cropIdFromKey(json["crop"] | "", id))
        {
            return core::ConfigError{ErrCode::WEB_INVALID_REQUEST, "crop"};
        }
    }

    if (json["stage"].is<const char*>())
    {
        if (!core::stageFromKey(json["stage"] | "", stage))
        {
            return core::ConfigError{ErrCode::WEB_INVALID_REQUEST, "stage"};
        }
    }

    if (json["intensity"].is<const char*>())
    {
        if (!intensityFromKey(json["intensity"] | "", intensity))
        {
            return core::ConfigError{ErrCode::WEB_INVALID_REQUEST, "intensity"};
        }
    }

    // SESSİZ YOK SAYMA YOK: alan varsa ya geçerli bir tam sayıdır ya da
    // istek reddedilir. Eskiden `is<int64_t>()` yanlışsa değer sessizce
    // mevcut hâlinde kalıyordu ve kullanıcı yazdığı tarihin kaydedildiğini
    // sanıyordu (TASK-072).
    if (!json["plantedAt"].isNull())
    {
        if (!json["plantedAt"].is<int64_t>())
        {
            return core::ConfigError{ErrCode::WEB_INVALID_REQUEST, "plantedAt"};
        }
        plantedAt = json["plantedAt"].as<int64_t>();
        if (plantedAt < 0)
        {
            return core::ConfigError{ErrCode::WEB_INVALID_REQUEST, "plantedAt"};
        }
    }

    if (json["autoStage"].is<bool>())
    {
        autoStage = json["autoStage"].as<bool>();
    }

    return core::configOk();
}

/// Planı yanıt gövdesine yazar — önizleme ve uygulama AYNI şemayı döner.
///
/// Aynı şekli döndürmeleri bilinçli: arayüz önizleme ekranını ve sonuç
/// ekranını tek bir kodla çizer, ikisi arasında sessizce ayrışamaz.
size_t writePlanJson(const services::crop::CropPlan& plan, bool applied, char* out,
                     size_t outLen)
{
    JsonDocument doc;
    doc["applied"]       = applied;
    doc["stage"]         = core::stageKeyOf(plan.stage);
    doc["ruleCount"]     = plan.ruleCount;
    doc["replacedCount"] = plan.replacedCount;

    // ── ARAYÜZÜN SÖYLEMESİ GEREKEN ŞEY ─────────────────────────────────────
    // Kurallar etkin doğar ama otomasyon motoru MANUAL modda hiçbirini
    // değerlendirmez. Bu iki gerçeği ayrı ayrı taşıyoruz ki arayüz
    // "kurallar hazır, ancak sistem OTOMATİK moda alınana kadar çalışmaz"
    // diyebilsin — kullanıcı kuralları görüp de çalıştığını sanmasın.
    doc["automationMode"] =
        (services::config::get().automation.mode == core::AutomationMode::AUTO) ? "auto"
                                                                                : "manual";

    JsonArray rules = doc["rules"].to<JsonArray>();
    writeGeneratedRules(rules, plan.rules);

    const size_t n = serializeJson(doc, out, outLen);
    return (n > 0 && n < outLen) ? n : 0;
}

} // namespace

void registerCrop(AsyncWebServer& server)
{
    // --- Katalog ------------------------------------------------------------
    server.on("/api/crops", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        JsonDocument doc;
        JsonArray    arr = doc["crops"].to<JsonArray>();

        for (uint8_t i = 0; i < core::cropCount(); ++i)
        {
            const CropProfile* p = core::cropAt(i);
            if (p == nullptr) { continue; }

            JsonObject o    = arr.add<JsonObject>();
            o["key"]        = p->key;
            o["name"]       = p->name;
            o["difficulty"] = p->difficulty;
            o["stageCount"] = p->stageCount;

            JsonArray stages = o["stages"].to<JsonArray>();
            for (uint8_t s = 0; s < p->stageCount; ++s)
            {
                writeStage(stages.add<JsonObject>(), p->stages[s], static_cast<GrowthStage>(s));
            }
        }

        // ── AKIŞLA GÖNDERİLİR, TAMPONA YAZILMAZ (TASK-072) ─────────────────
        // Katalog ~5 KB'lık en büyük yanıttır ve onun için ayrılmış 6 KB'lık
        // statik tampon, firmware'in EN BÜYÜK `.bss` nesnesiydi — doğrudan
        // boş heap'ten düşen kalıcı bir maliyet. `AsyncResponseStream`
        // kütüphaneye parçalar hâlinde yazdırır; `HistoryApi`'nin 22 KB'lık
        // yanıtı için verilen kararın aynısı.
        AsyncResponseStream* res = req->beginResponseStream("application/json");
        serializeJson(doc, *res);
        req->send(res);
    });

    // --- Aktif seçim --------------------------------------------------------
    server.on("/api/crop", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }


        if (writeCurrentCropJson(g_json, sizeof(g_json)) == 0)
        {
            sendError(req, ErrCode::WEB_PAYLOAD_TOO_LARGE);
            return;
        }
        sendJson(req, 200, g_json);
    });

    // --- Önizleme: HİÇBİR ŞEY YAZMAZ ---------------------------------------
    onJsonBody(server, "/api/crop/preview", HTTP_POST,
               [](AsyncWebServerRequest* req, JsonDocument& json) {
                   if (!requireAuth(req)) { return; }

                   CropId      id{};
                   GrowthStage stage{};
                   Intensity   intensity{};
                   int64_t     plantedAt = 0;
                   bool        autoStage = false;

                   const core::ConfigError re =
                       readSelection(json, id, stage, intensity, plantedAt, autoStage);
                   if (!re.ok()) { sendError(req, re.code, re.field); return; }

                   services::crop::CropPlan plan{};
                   const ErrCode rc = services::crop::preview(id, stage, intensity, plan);
                   if (rc != ErrCode::OK)
                   {
                       sendError(req, rc, plan.error.field);
                       return;
                   }


                   if (writePlanJson(plan, false, g_json, sizeof(g_json)) == 0)
                   {
                       sendError(req, ErrCode::WEB_PAYLOAD_TOO_LARGE);
                       return;
                   }
                   sendJson(req, 200, g_json);
               });

    // --- Uygula -------------------------------------------------------------
    onJsonBody(server, "/api/crop", HTTP_PUT,
               [](AsyncWebServerRequest* req, JsonDocument& json) {
                   if (!requireAuth(req)) { return; }

                   CropId      id{};
                   GrowthStage stage{};
                   Intensity   intensity{};
                   int64_t     plantedAt = 0;
                   bool        autoStage = false;

                   const core::ConfigError re =
                       readSelection(json, id, stage, intensity, plantedAt, autoStage);
                   if (!re.ok()) { sendError(req, re.code, re.field); return; }

                   services::crop::CropPlan plan{};
                   const ErrCode            rc = services::crop::apply(
                       id, stage, intensity, plantedAt, autoStage, plan);
                   if (rc != ErrCode::OK)
                   {
                       sendError(req, rc, plan.error.field);
                       return;
                   }


                   if (writePlanJson(plan, true, g_json, sizeof(g_json)) == 0)
                   {
                       // Kurallar YAZILDI ama yanıt sığmadı. `sendOk` ile
                       // başarıyı bildiriyoruz: istemciye hata dönmek,
                       // uygulanmış bir değişikliği uygulanmamış göstermek
                       // olurdu ve kullanıcı işlemi tekrarlardı.
                       sendOk(req);
                       return;
                   }
                   sendJson(req, 200, g_json);
               });
}

} // namespace api
} // namespace web
} // namespace interfaces
