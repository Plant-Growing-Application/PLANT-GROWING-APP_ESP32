#pragma once

// Sır deposu — TASK-013
//
// Wi-Fi şifresi ve arayüz parola hash'i, normal konfigürasyondan **AYRI** bir
// NVS namespace'inde tutulur.
//
// NEDEN AYRI NAMESPACE: config okumaları sırlara erişemez. Bir JSON
// serileştirici (TASK-043) yanlışlıkla config namespace'inin tamamını
// dolaşırsa şifre sızmaz — orada değildir.
//
// MASKELEME: `has*()` fonksiyonları "ayarlı mı?" sorusunu **değeri belleğe
// getirmeden** yanıtlar. API yanıtları ve OLED bu yolu kullanır; gerçek değeri
// yalnızca bağlantı kuran modül okur.
//
// MUTLAK KURAL (ARCHITECTURE §8.2): Wi-Fi şifresi `SystemState`'e, log'a,
// API yanıtına ve OLED'e ASLA girmez. Mevcut sistemde şifre hem EEPROM'da
// düz metin hem OLED'de açıkça görünüyordu — ikisi de burada kapatıldı.

#include <stddef.h>
#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {

/// Wi-Fi şifresi için üst sınır (WPA2 PSK: en fazla 63 karakter).
constexpr size_t WIFI_PASSWORD_MAX = 64;

/// Parola hash'i + salt için üst sınır (TASK-042 formatı belirleyecek).
constexpr size_t AUTH_HASH_MAX = 96;

namespace secrets {

// --- Wi-Fi şifresi ----------------------------------------------------------

core::ErrCode setWifiPassword(const char* password);

/// Şifreyi okur. YALNIZCA bağlantı kuran modül (TASK-036) çağırmalıdır.
/// Okunan değer loglanmaz, state'e yazılmaz, API yanıtına konmaz.
///
/// @param len giriş: tampon boyutu · çıkış: okunan uzunluk
core::ErrCode getWifiPassword(char* buf, size_t& len);

/// Şifre ayarlı mı? **Değeri döndürmez.**
/// API (`GET /api/config`) ve OLED bu yolu kullanır.
bool hasWifiPassword();

core::ErrCode clearWifiPassword();

// --- Arayüz parolası (hash + salt) ------------------------------------------

core::ErrCode setAuthHash(const void* hash, size_t len);
core::ErrCode getAuthHash(void* out, size_t& len);

/// Arayüz parolası belirlenmiş mi? `false` ise sistem **kurulum modundadır**
/// (TASK-042): yalnızca AP üzerinden ve yalnızca kurulum uç noktaları açıktır.
bool hasAuthHash();

// --- AP şifresi (TASK-038) ---------------------------------------------

/// SoftAP şifresi. **MAC'ten türetilmez** — SoftAP kendi BSSID'sini yayınlar
/// ve bu cihazın MAC adresidir; MAC'ten türetilen bir şifreyi menzildeki
/// herkes hesaplayabilirdi. İlk boot'ta rastgele üretilip buraya yazılır ve
/// OLED'de gösterilir.
core::ErrCode setApPassword(const char* password);
core::ErrCode getApPassword(char* buf, size_t& len);
bool hasApPassword();

// --- Tümü -------------------------------------------------------------------

/// Tüm sırları siler. Factory reset akışının parçasıdır (TASK-015);
/// config temizliğiyle **birlikte** çağrılmalıdır.
core::ErrCode clearAll();

} // namespace secrets
} // namespace hal
