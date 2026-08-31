#include "interfaces/web/HttpResponses.h"

#include <stdio.h>

namespace interfaces {
namespace web {

using core::ErrCode;

const char* errorName(ErrCode c)
{
    switch (c)
    {
        case ErrCode::OK:                        return "OK";
        case ErrCode::CFG_VALIDATION_FAILED:     return "CFG_VALIDATION_FAILED";
        case ErrCode::CFG_NOT_FOUND:             return "CFG_NOT_FOUND";
        case ErrCode::STORAGE_FS_MOUNT_FAILED:   return "STORAGE_FS_MOUNT_FAILED";
        case ErrCode::STORAGE_WRITE_FAILED:      return "STORAGE_WRITE_FAILED";
        case ErrCode::ACTUATOR_MIN_RUNTIME:      return "ACTUATOR_MIN_RUNTIME";
        case ErrCode::ACTUATOR_COOLDOWN:         return "ACTUATOR_COOLDOWN";
        case ErrCode::ACTUATOR_MAX_RUNTIME:      return "ACTUATOR_MAX_RUNTIME";
        case ErrCode::SAFETY_LEVEL_INSUFFICIENT: return "SAFETY_LEVEL_INSUFFICIENT";
        case ErrCode::SAFETY_LEVEL_SENSOR_FAULT: return "SAFETY_LEVEL_SENSOR_FAULT";
        case ErrCode::SAFETY_DRY_RUN:            return "SAFETY_DRY_RUN";
        case ErrCode::SAFETY_FLOW_VERIFY_FAILED: return "SAFETY_FLOW_VERIFY_FAILED";
        case ErrCode::SAFETY_EMERGENCY_LATCHED:  return "SAFETY_EMERGENCY_LATCHED";
        case ErrCode::SAFETY_BLOCKED:            return "SAFETY_BLOCKED";
        case ErrCode::NET_NO_CREDENTIALS:        return "NET_NO_CREDENTIALS";
        case ErrCode::NET_AUTH_FAILED:           return "NET_AUTH_FAILED";
        case ErrCode::NET_SCAN_FAILED:           return "NET_SCAN_FAILED";
        case ErrCode::NET_IP_CONFIG_INVALID:     return "NET_IP_CONFIG_INVALID";
        case ErrCode::TIME_NOT_SYNCED:           return "TIME_NOT_SYNCED";
        case ErrCode::WEB_UNAUTHORIZED:          return "WEB_UNAUTHORIZED";
        case ErrCode::WEB_INVALID_REQUEST:       return "WEB_INVALID_REQUEST";
        case ErrCode::WEB_BUSY:                  return "WEB_BUSY";
        case ErrCode::WEB_PAYLOAD_TOO_LARGE:     return "WEB_PAYLOAD_TOO_LARGE";
        default:                                 return "ERROR";
    }
}

uint16_t httpStatusFor(ErrCode c)
{
    switch (c)
    {
        case ErrCode::OK:                    return 200;
        case ErrCode::WEB_UNAUTHORIZED:      return 401;
        case ErrCode::WEB_PAYLOAD_TOO_LARGE: return 413;
        case ErrCode::WEB_BUSY:              return 503;
        case ErrCode::CFG_NOT_FOUND:         return 404;

        // Güvenlik vetosu 403: istek geçerli, izin yok. 400 döndürmek
        // istemciye "isteğini düzelt" der — oysa düzeltilecek bir şey yok,
        // koşulların değişmesi gerekiyor.
        case ErrCode::SAFETY_BLOCKED:
        case ErrCode::SAFETY_LEVEL_INSUFFICIENT:
        case ErrCode::SAFETY_LEVEL_SENSOR_FAULT:
        case ErrCode::SAFETY_DRY_RUN:
        case ErrCode::SAFETY_FLOW_VERIFY_FAILED:
        case ErrCode::SAFETY_EMERGENCY_LATCHED: return 403;

        // Kısıt ertelemesi 409: durum çakışması, istek yanlış değil.
        case ErrCode::ACTUATOR_MIN_RUNTIME:
        case ErrCode::ACTUATOR_COOLDOWN:     return 409;

        default:                             return 400;
    }
}

void sendError(AsyncWebServerRequest* req, ErrCode code, const char* field)
{
    if (req == nullptr) { return; }

    char body[192];
    if (field != nullptr)
    {
        snprintf(body, sizeof(body),
                 "{\"error\":{\"code\":%u,\"message\":\"%s\",\"field\":\"%s\"}}",
                 static_cast<unsigned>(code), errorName(code), field);
    }
    else
    {
        snprintf(body, sizeof(body), "{\"error\":{\"code\":%u,\"message\":\"%s\"}}",
                 static_cast<unsigned>(code), errorName(code));
    }

    req->send(httpStatusFor(code), "application/json", body);
}

void sendJson(AsyncWebServerRequest* req, uint16_t status, const char* json)
{
    if (req != nullptr) { req->send(status, "application/json", json); }
}

void sendOk(AsyncWebServerRequest* req)
{
    if (req != nullptr) { req->send(200, "application/json", "{\"ok\":true}"); }
}

} // namespace web
} // namespace interfaces
