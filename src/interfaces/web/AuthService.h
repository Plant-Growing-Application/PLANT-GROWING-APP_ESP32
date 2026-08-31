#pragma once

// Kimlik doğrulama — TASK-042
//
// Eski sistemde web arayüzü **tamamen korumasızdı**: ağa erişen herkes
// pompayı çalıştırabiliyordu (REQUIREMENTS Kritik Problem 7).
//
// ── PAROLA SAKLAMA ──────────────────────────────────────────────────────────
// salt(16) + çok turlu SHA-256 (20 000 tur). ESP32'nin mbedtls SHA-256
// uygulaması donanım hızlandırıcısını kullanır — plandaki (c) ve (d)
// seçenekleri alternatif değil, aynı şeydir.
//
// Tur sayısı kayıtta SAKLANIR: ileride artırılırsa eski kayıtlar hâlâ
// doğrulanabilir.
//
// ── OTURUM ──────────────────────────────────────────────────────────────────
// RAM'de 4 slotluk tablo. Durumsuz imzalı token REDDEDİLDİ: iptal
// edilemezdi, parola değişince eski oturumlar açık kalırdı. Reset ile tüm
// oturumların düşmesi bir ÖZELLİKTİR.
//
// ── KABA KUVVET ─────────────────────────────────────────────────────────────
// "Artan gecikme" AsyncTCP callback'inde UYGULANAMAZ — beklemek tüm web
// sunucusunu dondurur (§14.6). Yerine zaman pencereli kilit: 5 başarısız
// denemeden sonra 60 saniye boyunca reddedilir. Callback bloklamaz.
//
// ── HTTPS YOK — BİLİNÇLİ KISIT ──────────────────────────────────────────────
// Parola ağ üzerinde AÇIK gider. ESP32'de TLS hem RAM hem CPU açısından
// pahalıdır ve sertifika yönetimi yerel bir cihaz için çözülmemiş bir
// sorundur. Cihaz YEREL AĞ CİHAZI olarak konumlanır ve internete
// açılmamalıdır. Bu kısıt arayüzde kullanıcıya bildirilir (TASK-049).

#include <stddef.h>
#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace interfaces {
namespace web {
namespace auth {

constexpr uint8_t  SALT_LEN      = 16;
constexpr uint8_t  HASH_LEN      = 32;
constexpr uint32_t HASH_ROUNDS   = 20000u;
constexpr uint8_t  TOKEN_BYTES   = 32;
constexpr uint8_t  TOKEN_HEX_LEN = TOKEN_BYTES * 2;   ///< 64 karakter

/// Eşzamanlı oturum sayısı. Bir serada 1–2 kullanıcı olur; 4 fazlasıyla yeterli.
constexpr uint8_t MAX_SESSIONS = 4;

constexpr uint32_t SESSION_TTL_MS = 12u * 3600u * 1000u;   ///< 12 saat

/// Kaba kuvvet: bu kadar başarısız denemeden sonra kilit.
constexpr uint8_t  MAX_FAILURES  = 5;
constexpr uint32_t LOCKOUT_MS    = 60000u;

core::ErrCode begin();

/// Parola belirlenmiş mi? `false` ise sistem **kurulum modundadır**:
/// yalnızca `POST /api/setup/password` açıktır.
bool configured();

/// İlk parolayı belirler. Yalnızca kurulum modunda çalışır.
core::ErrCode setupPassword(const char* password);

/// Parolayı değiştirir — mevcut parolayı DOĞRULAR.
core::ErrCode changePassword(const char* current, const char* next);

/// Giriş yapar. Başarılıysa `tokenOut`'a 64 karakterlik token yazılır.
///
/// @param tokenOut en az `TOKEN_HEX_LEN + 1` bayt
core::ErrCode login(const char* password, core::Millis now, char* tokenOut, size_t outLen);

/// Token geçerli mi? Geçerliyse oturumun son kullanım zamanı tazelenir.
bool validate(const char* token, core::Millis now);

/// Oturumu sonlandırır.
void logout(const char* token);

/// Tüm oturumları düşürür — parola değişimi ve factory reset yolundan.
void invalidateAll();

/// Kaba kuvvet kilidi aktif mi?
bool lockedOut(core::Millis now);

/// Süresi dolmuş oturumları temizler. `net` task'ından çağrılır.
void tick(core::Millis now);

/// Parolayı ve tüm oturumları siler; sistem kurulum moduna döner.
core::ErrCode factoryReset();

} // namespace auth
} // namespace web
} // namespace interfaces
