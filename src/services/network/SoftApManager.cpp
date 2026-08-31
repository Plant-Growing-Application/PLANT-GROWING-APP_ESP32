#include "services/network/SoftApManager.h"

#include <esp_random.h>
#include <stdio.h>
#include <string.h>

#include "core/Diagnostics.h"
#include "hal/SecretStore.h"
#include "hal/WifiRadio.h"

namespace services {
namespace net {
namespace softap {
namespace {

using core::ErrCode;
using core::Millis;

char     g_ssid[SSID_MAX]              = {0};
char     g_pass[hal::WIFI_PASSWORD_MAX] = {0};
bool     g_active                      = false;
Millis   g_staUpSince{0};
bool     g_staWasUp                    = false;

/// Karışması kolay karakterler (`0/O`, `1/l/I`) bilinçli olarak YOK:
/// kullanıcı bu şifreyi OLED ekranından okuyup elle yazacak.
constexpr char ALPHABET[] = "abcdefghijkmnpqrstuvwxyz23456789";
constexpr uint8_t ALPHABET_LEN = sizeof(ALPHABET) - 1;

void buildSsid()
{
    const uint32_t id = hal::wifi::deviceId();
    snprintf(g_ssid, sizeof(g_ssid), "Sera-%02X%02X%02X",
             static_cast<unsigned>((id >> 16) & 0xFFu),
             static_cast<unsigned>((id >> 8) & 0xFFu),
             static_cast<unsigned>(id & 0xFFu));
}

/// Şifreyi rastgele üretir ve saklar. Yalnızca hiç yoksa çağrılır.
ErrCode generatePassword()
{
    for (uint8_t i = 0; i < PASS_LEN; ++i)
    {
        g_pass[i] = ALPHABET[esp_random() % ALPHABET_LEN];
    }
    g_pass[PASS_LEN] = '\0';

    return hal::secrets::setApPassword(g_pass);
}

} // namespace

core::ErrCode begin()
{
    buildSsid();

    size_t len = sizeof(g_pass);
    if (hal::secrets::hasApPassword() &&
        hal::secrets::getApPassword(g_pass, len) == ErrCode::OK && g_pass[0] != '\0')
    {
        return ErrCode::OK;
    }

    // İlk boot (veya sırlar silinmiş): yeni şifre üret.
    return generatePassword();
}

core::ErrCode start(Millis now)
{
    (void)now;
    if (g_active) { return ErrCode::OK; }

    const ErrCode rc = hal::wifi::apStart(g_ssid, g_pass, AP_IP, AP_SUBNET);
    if (rc == ErrCode::OK)
    {
        g_active = true;
        // Log satırında ŞİFRE YOK — yalnızca SSID.
        core::diag::log(core::LogLevel::INFO, ErrCode::OK, 0, "softap acildi");
    }
    else
    {
        core::diag::log(core::LogLevel::ERROR, rc, 0, "softap acilamadi");
    }
    return rc;
}

core::ErrCode stop()
{
    if (!g_active) { return ErrCode::OK; }

    const ErrCode rc = hal::wifi::apStop();
    g_active         = false;
    g_staWasUp       = false;
    core::diag::log(core::LogLevel::INFO, rc, 0, "softap kapatildi");
    return rc;
}

bool        active()      { return g_active; }
const char* ssid()        { return g_ssid; }
const char* password()    { return g_pass; }
uint8_t     clientCount() { return hal::wifi::apClientCount(); }

bool canCloseNow(Millis now, bool staConnected)
{
    if (!g_active || !staConnected) { g_staWasUp = false; return false; }

    // STA'nın ne zaman ayağa kalktığını burada takip ediyoruz: linger süresi
    // "STA bağlandığından beri" sayılır, "AP açıldığından beri" değil.
    if (!g_staWasUp)
    {
        g_staWasUp   = true;
        g_staUpSince = now;
        return false;
    }

    // Bağlı istemci varsa AP AÇIK KALIR — kullanıcı tam da ayarları
    // kaydettiği anda ortada bırakılmamalı.
    if (hal::wifi::apClientCount() > 0u) { return false; }

    return core::hasElapsed(now, g_staUpSince, core::millisecs(LINGER_MS));
}

bool shouldFallback(Millis now, Millis disconnectedSince)
{
    return core::hasElapsed(now, disconnectedSince, core::millisecs(FALLBACK_AFTER_MS));
}

} // namespace softap
} // namespace net
} // namespace services
