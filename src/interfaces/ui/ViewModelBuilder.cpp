#include "interfaces/ui/ViewModelBuilder.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/models/SafetyState.h"
#include "services/sensors/WaterLevelSensor.h"

namespace interfaces {
namespace ui {
namespace {

using core::ActuatorId;
using core::SensorId;
using core::SensorQuality;
using core::SystemMode;

void copyTo(char* dst, size_t cap, const char* src)
{
    if (cap == 0) { return; }
    strncpy(dst, (src != nullptr) ? src : "", cap - 1);
    dst[cap - 1] = '\0';
}

const char* labelOf(SensorId id)
{
    switch (id)
    {
        case SensorId::WATER_TEMP:  return "Su C";
        case SensorId::WATER_FLOW:  return "Debi";
        case SensorId::PH:          return "pH";
        case SensorId::EC:          return "EC";
        case SensorId::WATER_LEVEL: return "Seviye";
        case SensorId::HUMIDITY:    return "Nem";
        case SensorId::AMBIENT_TEMP: return "Hava C";
        case SensorId::LIGHT:        return "Isik";
        default:                    return "?";
    }
}

/// Sensör değerini biçimlendirir — KALİTE TABLOSU (TASK-050 Karar 3).
///
///   OK                        → değer + birim
///   FAULT                     → "—"   (**eski değer ASLA gösterilmez**)
///   NOT_PRESENT               → "yok"
///   STALE / OUT_OF_RANGE      → değer + "!"
///
/// Arızalı sensörün eski değerini göstermek, eski projedeki en tehlikeli
/// gösterim hatasıydı: operatör 20 dakika önce donmuş bir pH değerine bakıp
/// gübre ekleyebilir.
void formatSensor(const core::SensorSample& s, SensorLine& out)
{
    copyTo(out.label, sizeof(out.label), labelOf(s.id));
    out.present  = 1u;
    out.degraded = (s.quality != SensorQuality::OK) ? 1u : 0u;

    if (s.quality == SensorQuality::NOT_PRESENT)
    {
        copyTo(out.value, sizeof(out.value), "yok");
        return;
    }
    if (s.quality == SensorQuality::FAULT)
    {
        copyTo(out.value, sizeof(out.value), "--");
        return;
    }

    const char* mark = (s.quality == SensorQuality::OK) ? "" : "!";

    if (s.id == SensorId::WATER_LEVEL)
    {
        using services::sensors::WaterLevelState;
        const int v = static_cast<int>(s.value + 0.5f);
        const char* t = (v == static_cast<int>(WaterLevelState::SUFFICIENT)) ? "TAM"
                      : (v == static_cast<int>(WaterLevelState::LOW_LEVEL))  ? "DUSUK"
                                                                             : "KRITIK";
        snprintf(out.value, sizeof(out.value), "%s%s", t, mark);
        return;
    }

    // Sabit format — yerel ayara bağlı DEĞİL.
    const int digits = (s.id == SensorId::HUMIDITY) ? 0
                     : (s.id == SensorId::WATER_TEMP) ? 1 : 2;
    snprintf(out.value, sizeof(out.value), "%.*f%s", digits,
             static_cast<double>(s.value), mark);
}

const char* modeText(SystemMode m)
{
    switch (m)
    {
        case SystemMode::BOOTING:   return "ACILIYOR";
        case SystemMode::RUNNING:   return "CALISIYOR";
        case SystemMode::DEGRADED:  return "KISITLI";
        case SystemMode::SAFE:      return "GUVENLI";
        case SystemMode::EMERGENCY: return "ACIL";
        default:                    return "?";
    }
}

const char* netText(core::NetState s)
{
    switch (s)
    {
        case core::NetState::BOOT:        return "baslıyor";
        case core::NetState::AP_ONLY:     return "kurulum AP";
        case core::NetState::CONNECTING:  return "baglaniyor";
        case core::NetState::CONNECTED:   return "bagli";
        case core::NetState::BACKOFF:     return "bekliyor";
        case core::NetState::AP_FALLBACK: return "AP + deniyor";
        default:                          return "?";
    }
}

/// RSSI → 0–4 çubuk. Bağlı değilken 0.
uint8_t barsFor(int8_t rssi, bool connected)
{
    if (!connected) { return 0u; }
    return (rssi >= -55) ? 4u : (rssi >= -65) ? 3u : (rssi >= -75) ? 2u : 1u;
}

/// Aktüatör kısa adı. `ActuatorLine::label` 12 bayt — sığmalı (ISSUE-036).
const char* actLabel(ActuatorId id)
{
    switch (id)
    {
        case ActuatorId::WATER_PUMP:    return "Su pompa";
        case ActuatorId::AIR_PUMP:      return "Hava pmp";
        case ActuatorId::GROW_LIGHT:    return "Isik";
        case ActuatorId::HEATER:        return "Isitici";
        case ActuatorId::NUTRIENT_PUMP: return "Besin pmp";
        default:                        return "?";
    }
}

/// `ErrCode` → kısa Türkçe. 128 px genişliğe sığmalı.
const char* shortReason(core::ErrCode c)
{
    switch (c)
    {
        case core::ErrCode::OK:                        return "";
        case core::ErrCode::ACTUATOR_MIN_RUNTIME:      return "min sure";
        case core::ErrCode::ACTUATOR_COOLDOWN:         return "bekleme";
        case core::ErrCode::ACTUATOR_MAX_RUNTIME:      return "sure asimi";
        case core::ErrCode::ACTUATOR_STATE_MISMATCH:   return "durum farki";
        case core::ErrCode::SAFETY_LEVEL_INSUFFICIENT: return "su yetersiz";
        case core::ErrCode::SAFETY_LEVEL_SENSOR_FAULT: return "seviye ariza";
        case core::ErrCode::SAFETY_DRY_RUN:            return "kuru calisma";
        case core::ErrCode::SAFETY_FLOW_VERIFY_FAILED: return "akis yok";
        case core::ErrCode::SAFETY_EMERGENCY_LATCHED:  return "ACIL DURUM";
        case core::ErrCode::SAFETY_BLOCKED:            return "guvenlik";
        case core::ErrCode::TIME_NOT_SYNCED:           return "saat yok";
        case core::ErrCode::NET_AUTH_FAILED:           return "wifi parola";
        case core::ErrCode::NET_AP_NOT_FOUND:          return "ag bulunamadi";
        default:                                       return "hata";
    }
}

void formatUptime(uint32_t ms, char* out, size_t cap)
{
    const uint32_t s = ms / 1000u;
    const uint32_t d = s / 86400u;
    const uint32_t h = (s % 86400u) / 3600u;
    const uint32_t m = (s % 3600u) / 60u;

    if (d > 0u)      { snprintf(out, cap, "%ug %usa", d, h); }
    else if (h > 0u) { snprintf(out, cap, "%usa %udk", h, m); }
    else             { snprintf(out, cap, "%udk %usn", m, s % 60u); }
}

void ipToText(uint32_t raw, char* out, size_t cap)
{
    if (raw == 0u) { copyTo(out, cap, "bagli degil"); return; }
    snprintf(out, cap, "%u.%u.%u.%u", static_cast<unsigned>(raw & 0xFFu),
             static_cast<unsigned>((raw >> 8) & 0xFFu),
             static_cast<unsigned>((raw >> 16) & 0xFFu),
             static_cast<unsigned>((raw >> 24) & 0xFFu));
}

} // namespace

void build(const core::SystemState& s, ScreenId screen, uint8_t cursor, bool editing,
           const char* apSsid, const char* apPassword, UiModel& out)
{
    memset(&out, 0, sizeof(out));   // `memcmp` karşılaştırması için dolgu da sıfır olmalı

    out.screen  = screen;
    out.cursor  = cursor;
    out.editing = editing ? 1u : 0u;

    // --- Durum çubuğu ---
    // Zaman geçersizken SAHTE DEĞER YOK. Eski `getFormattedTime()`
    // senkronize değilken sessizce "00:00:00" döndürüyordu ve bir çizelge
    // bunu gece yarısı sanabilirdi.
    if (s.time.valid != 0u)
    {
        const time_t t = static_cast<time_t>(s.time.epoch.s);
        tm         lt{};
        localtime_r(&t, &lt);
        snprintf(out.clock, sizeof(out.clock), "%02d:%02d", lt.tm_hour, lt.tm_min);
    }
    else
    {
        copyTo(out.clock, sizeof(out.clock), "--:--");
    }

    copyTo(out.modeText, sizeof(out.modeText), modeText(s.system.mode));
    out.faultCount = s.system.activeFaultCount;
    out.emergency  = s.safety.emergencyLatched;
    out.rssi       = s.network.rssi;
    out.apActive   = s.network.apActive;
    out.wifiBars   = barsFor(s.network.rssi, s.network.state == core::NetState::CONNECTED);

    // --- Sensörler ---
    const uint8_t sn = (s.sensors.count <= core::MAX_SENSORS) ? s.sensors.count
                                                              : core::MAX_SENSORS;
    uint8_t w = 0;
    for (uint8_t i = 0; i < sn && w < UI_SENSORS; ++i)
    {
        formatSensor(s.sensors.samples[i], out.sensors[w]);
        ++w;
    }

    // --- Aktüatörler ---
    //
    // FİLTRE KALDIRILDI (ISSUE-036). Eskiden yalnızca su ve hava pompası
    // alınıyordu; o zaman doğruydu çünkü `AUX_1`/`AUX_2` eşlenmemişti ve
    // OLED'de olmayan bir röleyi göstermek anlamsızdı. TASK-066 ile beş
    // rölenin de fiziksel pini var; ışık, ısıtıcı ve dozaj pompasını
    // gizlemek OLED'i sistemin gerisinde bırakıyordu.
    const uint8_t an = (s.actuators.count <= core::MAX_ACTUATORS) ? s.actuators.count
                                                                  : core::MAX_ACTUATORS;
    uint8_t a = 0;
    for (uint8_t i = 0; i < an && a < UI_ACTS; ++i)
    {
        const core::ActuatorStatus& x = s.actuators.items[i];

        copyTo(out.actuators[a].label, sizeof(out.actuators[a].label), actLabel(x.id));
        out.actuators[a].on      = x.isOn;   // GERÇEK pin durumu
        out.actuators[a].blocked = (x.blockReason != core::ErrCode::OK) ? 1u : 0u;
        copyTo(out.actuators[a].why, sizeof(out.actuators[a].why),
               shortReason(x.blockReason));
        ++a;
    }

    // --- Ağ ---
    copyTo(out.ssid, sizeof(out.ssid), s.network.ssid.c_str());   // ŞİFRE YOK
    ipToText(s.network.ipv4, out.ip, sizeof(out.ip));
    copyTo(out.netState, sizeof(out.netState), netText(s.network.state));

    // Kurulum AP bilgisi — kullanıcı bu şifreyi ekrandan okuyup cihaza bağlanır.
    if (s.network.apActive != 0u)
    {
        copyTo(out.apSsid, sizeof(out.apSsid), apSsid);
        copyTo(out.apPassword, sizeof(out.apPassword), apPassword);
    }

    // --- Sistem ---
    formatUptime(s.system.uptimeMs, out.uptime, sizeof(out.uptime));

    // --- Uyarılar ---
    if (s.safety.emergencyLatched != 0u)
    {
        copyTo(out.emergencyWhy, sizeof(out.emergencyWhy),
               shortReason(s.safety.emergencyReason));
    }
    if (s.safety.interlockMask != 0u)
    {
        copyTo(out.alertText, sizeof(out.alertText),
               shortReason(domain::safety::firstReason(s.safety.interlockMask)));
    }
    else if (s.system.activeFaultCount > 0u)
    {
        snprintf(out.alertText, sizeof(out.alertText), "%u aktif hata",
                 s.system.activeFaultCount);
    }
}

bool sameAs(const UiModel& a, const UiModel& b)
{
    // POD ve `build()` başında tamamen sıfırlanıyor → dolgu baytları da
    // deterministik. `memcmp` güvenli.
    return memcmp(&a, &b, sizeof(UiModel)) == 0;
}

} // namespace ui
} // namespace interfaces
