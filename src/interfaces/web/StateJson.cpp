#include "interfaces/web/StateJson.h"

#include <ArduinoJson.h>
#include <string.h>

#include "core/Diagnostics.h"
#include "core/TaskRegistry.h"
#include "hal/FileStore.h"
#include "hal/SecretStore.h"
#include "services/ConfigService.h"
#include "services/CropService.h"
#include "services/HistoryStore.h"
#include "services/StorageService.h"
#include "services/network/ScanService.h"

namespace interfaces {
namespace web {
namespace {

using core::ActuatorId;
using core::NetState;
using core::SensorId;
using core::SensorQuality;
using core::SystemMode;

/// Ham IPv4 → metin. Sunum katmanının işi; `core/` ham `uint32` taşır.
void ipToStr(uint32_t raw, char* out, size_t len)
{
    snprintf(out, len, "%u.%u.%u.%u", static_cast<unsigned>(raw & 0xFFu),
             static_cast<unsigned>((raw >> 8) & 0xFFu),
             static_cast<unsigned>((raw >> 16) & 0xFFu),
             static_cast<unsigned>((raw >> 24) & 0xFFu));
}

} // namespace

const char* sensorName(SensorId id)
{
    switch (id)
    {
        case SensorId::WATER_TEMP:  return "waterTemp";
        case SensorId::WATER_FLOW:  return "flow";
        case SensorId::PH:          return "ph";
        case SensorId::EC:          return "ec";
        case SensorId::WATER_LEVEL: return "level";
        case SensorId::HUMIDITY:    return "humidity";
        case SensorId::AMBIENT_TEMP: return "airTemp";
        case SensorId::LIGHT:        return "light";
        default:                    return "unknown";
    }
}

const char* actuatorName(ActuatorId id)
{
    switch (id)
    {
        case ActuatorId::WATER_PUMP:    return "waterPump";
        case ActuatorId::AIR_PUMP:      return "airPump";
        case ActuatorId::GROW_LIGHT:    return "growLight";
        case ActuatorId::HEATER:        return "heater";
        case ActuatorId::NUTRIENT_PUMP: return "nutrientPump";
        default:                        return "unknown";
    }
}

const char* qualityName(SensorQuality q)
{
    switch (q)
    {
        case SensorQuality::OK:           return "ok";
        case SensorQuality::STALE:        return "stale";
        case SensorQuality::OUT_OF_RANGE: return "outOfRange";
        case SensorQuality::FAULT:        return "fault";
        case SensorQuality::NOT_PRESENT:  return "notPresent";
        default:                          return "unknown";
    }
}

const char* modeName(SystemMode m)
{
    switch (m)
    {
        case SystemMode::BOOTING:   return "booting";
        case SystemMode::RUNNING:   return "running";
        case SystemMode::DEGRADED:  return "degraded";
        case SystemMode::SAFE:      return "safe";
        case SystemMode::EMERGENCY: return "emergency";
        default:                    return "unknown";
    }
}

const char* netStateName(NetState s)
{
    switch (s)
    {
        case NetState::BOOT:        return "boot";
        case NetState::AP_ONLY:     return "apOnly";
        case NetState::CONNECTING:  return "connecting";
        case NetState::CONNECTED:   return "connected";
        case NetState::BACKOFF:     return "backoff";
        case NetState::AP_FALLBACK: return "apFallback";
        default:                    return "unknown";
    }
}

size_t writeStateJson(const core::SystemState& s, char* out, size_t outLen)
{
    JsonDocument doc;

    doc["type"] = "state";
    doc["v"]    = s.version;

    JsonObject sys = doc["system"].to<JsonObject>();
    sys["mode"]        = modeName(s.system.mode);
    sys["uptimeMs"]    = s.system.uptimeMs;
    sys["freeHeap"]    = s.system.freeHeapBytes;
    sys["minFreeHeap"] = s.system.minFreeHeapBytes;
    sys["faults"]      = s.system.activeFaultCount;
    sys["faultMask"]   = s.system.faultSubsystemMask;
    sys["resetReason"] = s.system.resetReason;

    JsonArray sensors = doc["sensors"].to<JsonArray>();
    const uint8_t sn = (s.sensors.count <= core::MAX_SENSORS) ? s.sensors.count
                                                              : core::MAX_SENSORS;
    for (uint8_t i = 0; i < sn; ++i)
    {
        JsonObject o = sensors.add<JsonObject>();
        o["id"]      = sensorName(s.sensors.samples[i].id);
        o["quality"] = qualityName(s.sensors.samples[i].quality);
        // Değer, kalitesi OK DEĞİLKEN de taşınır ama arayüz kaliteyi
        // görmeden göstermemelidir (§9.2). Değeri saklamak, kullanıcının
        // "sensör ne diyor" sorusunu cevapsız bırakırdı.
        o["value"]   = s.sensors.samples[i].value;
        o["faults"]  = s.sensors.samples[i].faultCount;
    }

    JsonArray acts = doc["actuators"].to<JsonArray>();
    const uint8_t an = (s.actuators.count <= core::MAX_ACTUATORS) ? s.actuators.count
                                                                  : core::MAX_ACTUATORS;
    for (uint8_t i = 0; i < an; ++i)
    {
        JsonObject o = acts.add<JsonObject>();
        o["id"]     = actuatorName(s.actuators.items[i].id);
        // GERÇEK pin durumu — talep edilen değil (§2.6).
        o["on"]     = s.actuators.items[i].isOn != 0u;
        o["source"] = static_cast<uint8_t>(s.actuators.items[i].source);
        o["block"]  = static_cast<uint16_t>(s.actuators.items[i].blockReason);
        o["runMs"]  = s.actuators.items[i].totalRunMs;
        o["cycles"] = s.actuators.items[i].cycleCount;
    }

    JsonObject safety = doc["safety"].to<JsonObject>();
    safety["interlocks"] = s.safety.interlockMask;
    safety["latched"]    = s.safety.emergencyLatched != 0u;
    safety["reason"]     = static_cast<uint16_t>(s.safety.emergencyReason);

    JsonObject net = doc["network"].to<JsonObject>();
    net["state"]     = netStateName(s.network.state);
    net["ssid"]      = s.network.ssid.c_str();   // ŞİFRE YOK
    net["rssi"]      = s.network.rssi;
    net["apActive"]  = s.network.apActive != 0u;
    net["apClients"] = s.network.apClients;
    net["retries"]   = s.network.retryCount;
    net["lastError"] = static_cast<uint16_t>(s.network.lastError);

    // ── KURULUM DURUMU (§8.4) ──────────────────────────────────────────────
    // `setupReboot` arayüzün bağlantı kopmasını AÇIKLAYABİLMESİ için vardır:
    // telefon kurulum AP'sindeyken cihaz yeniden başlayacak ve WebSocket
    // kopacaktır. Bunu önceden söylemek ile söylememek arasındaki fark,
    // "kurulum tamamlandı" ile "cihaz bozuldu" arasındaki farktır.
    // Bekleme SAYILIR: "Yeniden Denenecek" yazip sessiz kalmak, kullaniciya
    // cihazin pes ettigini dusundurur. Kalan saniye ve "Simdi Dene" birlikte,
    // beklemeyi katlanilir kilar.
    net["retryIn"]      = s.network.retryIn;

    net["provisioning"] = s.network.provisioning != 0u;
    net["setupReboot"]  = s.network.setupReboot != 0u;
    net["rebootIn"]     = s.network.rebootIn;

    char ipbuf[16];
    ipToStr(s.network.ipv4, ipbuf, sizeof(ipbuf));
    net["ip"] = ipbuf;

    JsonObject tm = doc["time"].to<JsonObject>();
    tm["valid"] = s.time.valid != 0u;
    // Geçersizken SAHTE DEĞER YOK — arayüz "saat geçersiz" gösterir.
    tm["epoch"] = s.time.valid ? static_cast<long>(s.time.epoch.s) : 0L;

    // ── AKTİF ÜRÜN (TASK-069) ──────────────────────────────────────────────
    //
    // KOMPAKT: yalnızca kimlik, dönem ve gün. Hedef bantlar ve katalog burada
    // DEĞİL — onlar `/api/crop` ile bir kez alınır. Bu paket saniyede bir
    // yayınlanıyor; değişmeyen 5 KB'lık katalogu her turda taşımak, telemetri
    // hızını düşürmekten başka bir işe yaramazdı.
    //
    // Panelin "Çilek · 34. gün · Meyve dönemi" satırını çizmesi için bu üç
    // alan yeterlidir ve ayrı bir istek gerektirmez.
    {
        const core::CropConfig& cc = services::config::get().crop;
        JsonObject              cr = doc["crop"].to<JsonObject>();
        cr["key"]   = core::cropKeyOf(cc.crop);
        cr["stage"] = core::stageKeyOf(cc.stage);
        cr["day"]   = services::crop::daysSincePlanting();
    }

    JsonObject au = doc["automation"].to<JsonObject>();
    au["mode"]            = static_cast<uint8_t>(s.automation.mode);
    au["schedulesPaused"] = s.automation.schedulesPaused != 0u;

    const size_t n = serializeJson(doc, out, outLen);
    return (n > 0 && n < outLen) ? n : 0u;
}

size_t writeDiagnosticsJson(char* out, size_t outLen)
{
    JsonDocument doc;

    core::FaultSummary faults{};
    core::diag::activeFaults(faults);

    doc["activeCount"]  = core::diag::activeFaultCount();
    doc["totalRecords"] = core::diag::totalRecords();
    doc["droppedIsr"]   = core::diag::droppedFromIsr();

    JsonArray active = doc["active"].to<JsonArray>();
    for (uint8_t i = 0; i < faults.count && i < core::MAX_ACTIVE_FAULTS; ++i)
    {
        active.add(static_cast<uint16_t>(faults.faults[i]));
    }

    // Depolama istatistikleri: eski projede `Task_SensorLogger` loglama
    // yapmıyordu ve kimse fark etmemişti. Sayaçlar tam olarak bu yüzden var.
    JsonObject st = doc["storage"].to<JsonObject>();
    st["stored"]    = services::history::storedCount();
    st["total"]     = services::history::totalWritten();
    st["corrupt"]   = services::history::corruptCount();
    st["fsErrors"]  = services::history::writeErrors();
    st["queueDrop"] = services::storage::droppedRequests();
    st["processed"] = services::storage::processedRequests();

    hal::FsStats fs{};
    hal::fs::stats(fs);
    st["fsUsed"] = fs.usedBytes;
    st["fsFree"] = fs.freeBytes;

    // ── TASK SAĞLIĞI (TASK-062) ────────────────────────────────────────────
    // Veri `TaskRunner` tarafından zaten toplanıyordu ama HİÇBİR YERDE
    // sunulmuyordu — ölçülemeyen bir şey doğrulanamaz.
    //
    // `minStack` ESP-IDF'te BAYT cinsindendir (vanilla FreeRTOS'tan farklı);
    // stack boyutlarının düzeltilmesi bu değere dayanacak.
    static const char* TASK_NAMES[] = {"app_core", "io_sense", "net", "ui", "store"};

    JsonArray tasks = doc["tasks"].to<JsonArray>();
    for (uint8_t i = 0; i < static_cast<uint8_t>(core::TaskId::COUNT); ++i)
    {
        core::TaskHealth h{};
        core::taskreg::health(static_cast<core::TaskId>(i), h);

        JsonObject o = tasks.add<JsonObject>();
        o["name"]       = TASK_NAMES[i];
        o["registered"] = h.registered != 0u;
        o["beats"]      = h.beatCount;
        o["maxLoopUs"]  = h.maxLoopUs;
        o["overruns"]   = h.overrunCount;
        o["minStack"]   = h.minFreeStackBytes;
        o["lastBeat"]   = h.lastBeatAt.v;
    }

    core::LogRecord recent[16];
    const uint8_t   rn = core::diag::recent(recent, 16);

    JsonArray events = doc["events"].to<JsonArray>();
    for (uint8_t i = 0; i < rn; ++i)
    {
        JsonObject o = events.add<JsonObject>();
        o["t"]     = recent[i].timestamp.v;
        o["code"]  = static_cast<uint16_t>(recent[i].code);
        o["level"] = static_cast<uint8_t>(recent[i].level);
        o["d"]     = recent[i].detail;
    }

    const size_t n = serializeJson(doc, out, outLen);
    return (n > 0 && n < outLen) ? n : 0u;
}

void fillConfigJson(JsonDocument& doc)
{
    const core::Config& c = services::config::get();

    JsonObject net = doc["network"].to<JsonObject>();
    net["ssid"]   = c.network.ssid.c_str();
    net["ipMode"] = (c.network.ipMode == core::IpMode::STATIC) ? "static" : "dhcp";
    char b[16];
    ipToStr(c.network.staticIp, b, sizeof(b)); net["staticIp"] = b;
    ipToStr(c.network.gateway, b, sizeof(b));  net["gateway"]  = b;
    ipToStr(c.network.subnet, b, sizeof(b));   net["subnet"]   = b;
    ipToStr(c.network.dns, b, sizeof(b));      net["dns"]      = b;

    // SIRLAR MASKELİ: değer değil, YALNIZCA "ayarlı mı" bilgisi.
    // `hasWifiPassword()` değeri belleğe hiç getirmez.
    net["passwordSet"] = hal::secrets::hasWifiPassword();

    JsonObject safety = doc["safety"].to<JsonObject>();
    safety["flowVerifyDelayMs"]   = c.safety.flowVerifyDelayMs;
    safety["flowMinRate"]         = c.safety.flowMinRate;
    safety["maxRuntimeGraceMs"]   = c.safety.maxRuntimeGraceMs;
    safety["maxRuntimeViolations"] = c.safety.maxRuntimeViolations;
    safety["requireLevelSensor"]  = c.safety.requireLevelSensor != 0u;

    // ── SENSÖRLER (ISSUE-035) ──────────────────────────────────────────────
    //
    // Bu bölüm EKSİKTİ. `PUT /api/config/sensors` uç noktası da yoktu ve
    // `ConfigService::updateSensor()` hiçbir yerden çağrılmıyordu: pH, EC,
    // nem, hava sıcaklığı ve ışık `enabled = 0` doğuyor ve **hiçbir şekilde
    // açılamıyordu**. Kullanıcı sensörü fiziksel olarak taksa bile arayüzde
    // sonsuza kadar "takılı değil" yazıyordu.
    //
    // Kalibrasyon (`offset`/`scale`) de aynı boşluktaydı: ARCHITECTURE §9.3'ün
    // anlattığı 2 nokta pH kalibrasyonunun API karşılığı yoktu.
    JsonArray sens = doc["sensors"].to<JsonArray>();
    for (uint8_t i = 0; i < core::MAX_SENSORS; ++i)
    {
        const char* name = sensorName(static_cast<SensorId>(i));
        if (strcmp(name, "unknown") == 0) { continue; }   // tanımsız slot

        JsonObject o = sens.add<JsonObject>();
        o["id"]              = name;
        o["enabled"]         = c.sensors[i].enabled != 0u;
        o["offset"]          = c.sensors[i].offset;
        o["scale"]           = c.sensors[i].scale;
        o["filterStrength"]  = c.sensors[i].filterStrength;
        o["maxChangePerSec"] = c.sensors[i].maxChangePerSec;

        JsonObject vr = o["validRange"].to<JsonObject>();
        vr["min"] = c.sensors[i].validRange.min;
        vr["max"] = c.sensors[i].validRange.max;
    }

    JsonArray acts = doc["actuators"].to<JsonArray>();
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        JsonObject o = acts.add<JsonObject>();
        o["id"]         = actuatorName(static_cast<ActuatorId>(i));
        o["enabled"]    = c.actuators[i].enabled != 0u;
        o["minRunMs"]   = c.actuators[i].minRunMs;
        o["maxRunMs"]   = c.actuators[i].maxRunMs;
        o["cooldownMs"] = c.actuators[i].cooldownMs;
    }

    // Otomasyon. `PUT /api/config/automation` bu iki alanı yazar; GET'te
    // dönmediği sürece arayüz mevcut modu GÖSTEREMEZ ve kullanıcı kendi
    // yazdığı değeri geri okuyamaz — yazılabilir ama okunamaz bir ayar,
    // kullanıcının neyi değiştirdiğini bilmediği ayardır.
    JsonObject aut = doc["automation"].to<JsonObject>();
    aut["mode"] = (c.automation.mode == core::AutomationMode::AUTO) ? "auto" : "manual";
    aut["manualOverrideMs"] = c.automation.manualOverrideMs;

    JsonObject sys = doc["system"].to<JsonObject>();
    sys["timezone"]            = c.system.timezone.c_str();
    sys["telemetryIntervalMs"] = c.system.telemetryIntervalMs;
    sys["logLevel"]            = c.system.logLevel;
    sys["authSet"]             = hal::secrets::hasAuthHash();
}


const char* ruleKindName(core::RuleKind k)
{
    switch (k)
    {
        case core::RuleKind::THRESHOLD:       return "threshold";
        case core::RuleKind::SCHEDULE_WINDOW: return "window";
        case core::RuleKind::SCHEDULE_CYCLE:  return "cycle";
        case core::RuleKind::INACTIVE:        return "inactive";
        default:                              return "inactive";
    }
}

bool sensorIdFromName(const char* name, SensorId& out)
{
    if (name == nullptr || name[0] == '\0') { return false; }

    for (uint8_t i = 0; i < core::MAX_SENSORS; ++i)
    {
        const char* n = sensorName(static_cast<SensorId>(i));
        // Tanımsız slotlar "unknown" döner; "unknown" bir ad DEĞİLDİR ve
        // eşleşmesine izin verilirse ilk tanımsız sensöre çözülürdü.
        if (strcmp(n, "unknown") == 0) { continue; }
        if (strcmp(n, name) == 0)
        {
            out = static_cast<SensorId>(i);
            return true;
        }
    }
    return false;
}

bool actuatorIdFromName(const char* name, ActuatorId& out)
{
    if (name == nullptr || name[0] == '\0') { return false; }

    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        const char* n = actuatorName(static_cast<ActuatorId>(i));
        if (strcmp(n, "unknown") == 0) { continue; }
        if (strcmp(n, name) == 0)
        {
            out = static_cast<ActuatorId>(i);
            return true;
        }
    }
    return false;
}

bool ruleKindFromName(const char* name, core::RuleKind& out)
{
    if (name == nullptr) { return false; }

    const core::RuleKind kinds[] = {
        core::RuleKind::INACTIVE,
        core::RuleKind::THRESHOLD,
        core::RuleKind::SCHEDULE_WINDOW,
        core::RuleKind::SCHEDULE_CYCLE,
    };
    for (const core::RuleKind k : kinds)
    {
        if (strcmp(ruleKindName(k), name) == 0) { out = k; return true; }
    }
    return false;
}

size_t writeRulesJson(char* out, size_t outLen)
{
    const core::Config& c = services::config::get();

    JsonDocument doc;
    doc["count"] = c.rules.count;
    doc["max"]   = core::MAX_RULES;

    JsonArray arr = doc["rules"].to<JsonArray>();

    const uint8_t n = (c.rules.count <= core::MAX_RULES) ? c.rules.count : core::MAX_RULES;
    for (uint8_t i = 0; i < n; ++i)
    {
        const core::Rule& r = c.rules.rules[i];
        JsonObject o = arr.add<JsonObject>();

        // Kural yapısı `union` DEĞİL (Rule.h): her tür için TÜM alanlar
        // taşınır. JSON de aynı seçimi izler — istemci `kind` değiştirdiğinde
        // diğer türün değerleri kaybolmaz ve kullanıcı geri dönebilir.
        o["kind"]                = ruleKindName(r.kind);
        o["target"]              = actuatorName(r.target);
        o["enabled"]             = r.enabled != 0u;
        o["priority"]            = r.priority;
        o["minTriggerIntervalS"] = r.minTriggerIntervalS;

        o["sensor"]       = sensorName(r.sensor);
        o["onThreshold"]  = r.onThreshold;
        o["offThreshold"] = r.offThreshold;

        o["startMin"] = r.startMin;
        o["endMin"]   = r.endMin;

        o["cycleOnS"]     = r.cycleOnS;
        o["cyclePeriodS"] = r.cyclePeriodS;
    }

    const size_t written = serializeJson(doc, out, outLen);
    return (written > 0 && written < outLen) ? written : 0u;
}

size_t writeScanJson(core::Millis now, char* out, size_t outLen)
{
    using services::net::scan::ScanState;

    JsonDocument doc;

    // ŞEMA TÜM DURUMLARDA AYNI. Eski sistemde tarama sürerken farklı
    // şekilli bir yanıt dönüyordu ve frontend onu dizi sanıp `forEach`
    // çağırıyordu → "Ağ taraması yapılamadı!" → ilk tıklama HER ZAMAN
    // başarısız.
    const ScanState st = services::net::scan::state();
    doc["status"]    = (st == ScanState::IDLE)    ? "idle"
                     : (st == ScanState::RUNNING) ? "running"
                     : (st == ScanState::DONE)    ? "done"
                                                  : "failed";
    doc["age"]       = services::net::scan::ageMs(now);
    doc["truncated"] = services::net::scan::truncated();

    // `networks` HER ZAMAN dizi — boş olsa bile. `null` veya eksik alan
    // istemcide tam olarak yukarıdaki hatayı üretir.
    JsonArray nets = doc["networks"].to<JsonArray>();
    const services::net::scan::ScanEntry* r = services::net::scan::results();
    const uint8_t n = services::net::scan::count();
    for (uint8_t i = 0; i < n; ++i)
    {
        JsonObject o = nets.add<JsonObject>();
        o["ssid"]    = r[i].ssid;
        o["rssi"]    = r[i].rssi;
        o["channel"] = r[i].channel;
        o["open"]    = r[i].encType == 0u;
    }

    const size_t written = serializeJson(doc, out, outLen);
    return (written > 0 && written < outLen) ? written : 0u;
}

} // namespace web
} // namespace interfaces
