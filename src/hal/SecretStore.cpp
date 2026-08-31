#include "SecretStore.h"

#include <string.h>

#include "NvsStore.h"
#include "core/Diagnostics.h"

namespace hal {
namespace secrets {
namespace {

using core::ErrCode;

// Anahtar adları 15 karakter sınırına uyar (NVS_KEY_NAME_MAX_SIZE = 16).
constexpr const char* KEY_WIFI_PASS = "wifi_pass";
constexpr const char* KEY_AUTH_HASH = "auth_hash";
constexpr const char* KEY_AP_PASS   = "ap_pass";

} // namespace

ErrCode setWifiPassword(const char* password)
{
    if (password == nullptr)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    if (strnlen(password, WIFI_PASSWORD_MAX) >= WIFI_PASSWORD_MAX)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }

    const ErrCode rc = nvsstore::setString(NS_SECRET, KEY_WIFI_PASS, password);

    // Log satırında ŞİFRE YOK — yalnızca işlemin sonucu.
    core::diag::log(rc == ErrCode::OK ? core::LogLevel::INFO : core::LogLevel::ERROR, rc, 0,
                    "wifi sifresi kaydedildi");
    return rc;
}

ErrCode getWifiPassword(char* buf, size_t& len)
{
    return nvsstore::getString(NS_SECRET, KEY_WIFI_PASS, buf, len);
}

bool hasWifiPassword()
{
    return nvsstore::exists(NS_SECRET, KEY_WIFI_PASS);
}

ErrCode clearWifiPassword()
{
    return nvsstore::eraseKey(NS_SECRET, KEY_WIFI_PASS);
}

ErrCode setAuthHash(const void* hash, size_t len)
{
    if (hash == nullptr || len == 0 || len > AUTH_HASH_MAX)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    return nvsstore::setBlob(NS_SECRET, KEY_AUTH_HASH, hash, len);
}

ErrCode getAuthHash(void* out, size_t& len)
{
    return nvsstore::getBlob(NS_SECRET, KEY_AUTH_HASH, out, len);
}

bool hasAuthHash()
{
    return nvsstore::exists(NS_SECRET, KEY_AUTH_HASH);
}

// --- AP şifresi (TASK-038) ---------------------------------------------

ErrCode setApPassword(const char* password)
{
    if (password == nullptr || strnlen(password, WIFI_PASSWORD_MAX) >= WIFI_PASSWORD_MAX)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    const ErrCode rc = nvsstore::setString(NS_SECRET, KEY_AP_PASS, password);
    // Log satırında ŞİFRE YOK.
    core::diag::log(rc == ErrCode::OK ? core::LogLevel::INFO : core::LogLevel::ERROR, rc, 0,
                    "ap sifresi uretildi");
    return rc;
}

ErrCode getApPassword(char* buf, size_t& len)
{
    return nvsstore::getString(NS_SECRET, KEY_AP_PASS, buf, len);
}

bool hasApPassword()
{
    return nvsstore::exists(NS_SECRET, KEY_AP_PASS);
}

ErrCode clearAll()
{
    const ErrCode rc = nvsstore::eraseNamespace(NS_SECRET);
    core::diag::log(core::LogLevel::WARNING, rc, 0, "tum sirlar silindi");
    return rc;
}

} // namespace secrets
} // namespace hal
