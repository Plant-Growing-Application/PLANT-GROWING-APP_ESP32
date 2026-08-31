#include "interfaces/web/AuthService.h"

#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <string.h>

#include "core/Diagnostics.h"
#include "hal/SecretStore.h"

namespace interfaces {
namespace web {
namespace auth {
namespace {

using core::ErrCode;
using core::Millis;

/// Saklanan kayıt: salt + hash + tur sayısı = 52 bayt.
///
/// Tur sayısı kayıtta tutulur ki ileride artırıldığında eski kayıtlar hâlâ
/// doğrulanabilsin.
struct StoredCredential
{
    uint8_t  salt[SALT_LEN];
    uint8_t  hash[HASH_LEN];
    uint32_t rounds;
};

struct Session
{
    char   token[TOKEN_HEX_LEN + 1];
    Millis createdAt;
    Millis lastSeenAt;
    bool   used;
};

StoredCredential g_cred{};
Session          g_sessions[MAX_SESSIONS];
bool             g_configured = false;

uint8_t g_failures = 0;
Millis  g_lockedAt{0};

/// Sabit zamanlı karşılaştırma.
///
/// `memcmp` ilk farklı baytta döner ve süresiyle "kaç bayt eşleşti"
/// bilgisini sızdırır. XOR biriktirme her zaman tam uzunluğu tarar.
bool constantTimeEquals(const void* a, const void* b, size_t len)
{
    const uint8_t* x   = static_cast<const uint8_t*>(a);
    const uint8_t* y   = static_cast<const uint8_t*>(b);
    uint8_t        acc = 0;
    for (size_t i = 0; i < len; ++i) { acc |= static_cast<uint8_t>(x[i] ^ y[i]); }
    return acc == 0;
}

/// salt + parola üzerinden çok turlu SHA-256.
///
/// ESP32'nin mbedtls uygulaması donanım hızlandırıcısını kullanır.
void deriveHash(const char* password, const uint8_t* salt, uint32_t rounds, uint8_t out[HASH_LEN])
{
    uint8_t buf[SALT_LEN + HASH_LEN] = {0};
    memcpy(buf, salt, SALT_LEN);

    // İlk tur: salt || parola
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, salt, SALT_LEN);
    mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const uint8_t*>(password),
                              strnlen(password, 128));
    mbedtls_sha256_finish_ret(&ctx, out);
    mbedtls_sha256_free(&ctx);

    // Kalan turlar: salt || önceki hash
    for (uint32_t r = 1; r < rounds; ++r)
    {
        memcpy(buf + SALT_LEN, out, HASH_LEN);
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, buf, sizeof(buf));
        mbedtls_sha256_finish_ret(&ctx, out);
        mbedtls_sha256_free(&ctx);
    }
}

void toHex(const uint8_t* in, size_t len, char* out)
{
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i)
    {
        out[i * 2]     = HEX[(in[i] >> 4) & 0x0Fu];
        out[i * 2 + 1] = HEX[in[i] & 0x0Fu];
    }
    out[len * 2] = '\0';
}

core::ErrCode persist()
{
    return hal::secrets::setAuthHash(&g_cred, sizeof(g_cred));
}

/// Parolayı üretir ve saklar. Çağıranın yetki kontrolü yapmış olması gerekir.
core::ErrCode storePassword(const char* password)
{
    if (password == nullptr || strnlen(password, 9) < 8)
    {
        // 8 karakter alt sınırı: daha kısa bir parola, 20 000 turluk hash'i
        // anlamsız kılacak kadar küçük bir arama uzayı bırakır.
        return ErrCode::CFG_VALIDATION_FAILED;
    }

    for (uint8_t i = 0; i < SALT_LEN; ++i)
    {
        g_cred.salt[i] = static_cast<uint8_t>(esp_random() & 0xFFu);
    }
    g_cred.rounds = HASH_ROUNDS;
    deriveHash(password, g_cred.salt, g_cred.rounds, g_cred.hash);

    const ErrCode rc = persist();
    if (rc == ErrCode::OK) { g_configured = true; }
    return rc;
}

bool verify(const char* password)
{
    if (!g_configured || password == nullptr) { return false; }

    uint8_t candidate[HASH_LEN] = {0};
    deriveHash(password, g_cred.salt, g_cred.rounds, candidate);

    const bool ok = constantTimeEquals(candidate, g_cred.hash, HASH_LEN);
    memset(candidate, 0, sizeof(candidate));
    return ok;
}

} // namespace

core::ErrCode begin()
{
    memset(g_sessions, 0, sizeof(g_sessions));
    g_failures = 0;

    size_t len = sizeof(g_cred);
    if (hal::secrets::hasAuthHash() &&
        hal::secrets::getAuthHash(&g_cred, len) == ErrCode::OK && len == sizeof(g_cred) &&
        g_cred.rounds > 0u)
    {
        g_configured = true;
    }
    else
    {
        // Kurulum modu: yalnızca parola belirleme uç noktası açık.
        g_configured = false;
        core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_UNAUTHORIZED, 0,
                        "parola belirlenmemis - kurulum modu");
    }
    return ErrCode::OK;
}

bool configured() { return g_configured; }

core::ErrCode setupPassword(const char* password)
{
    // Yalnızca kurulum modunda. Aksi hâlde bu uç nokta parolayı doğrulamadan
    // değiştirmenin yolu olurdu.
    if (g_configured) { return ErrCode::WEB_UNAUTHORIZED; }
    return storePassword(password);
}

core::ErrCode changePassword(const char* current, const char* next)
{
    if (!g_configured) { return ErrCode::WEB_UNAUTHORIZED; }
    if (!verify(current)) { return ErrCode::WEB_UNAUTHORIZED; }

    const ErrCode rc = storePassword(next);
    if (rc == ErrCode::OK)
    {
        // Parola değişti: eski oturumlar DÜŞER. Aksi hâlde parolayı
        // değiştirmenin amacı (erişimi kesmek) gerçekleşmez.
        invalidateAll();
    }
    return rc;
}

bool lockedOut(Millis now)
{
    return g_failures >= MAX_FAILURES &&
           !core::hasElapsed(now, g_lockedAt, core::millisecs(LOCKOUT_MS));
}

core::ErrCode login(const char* password, Millis now, char* tokenOut, size_t outLen)
{
    if (!g_configured) { return ErrCode::WEB_UNAUTHORIZED; }
    if (tokenOut == nullptr || outLen <= TOKEN_HEX_LEN) { return ErrCode::WEB_INVALID_REQUEST; }

    // Kilit: callback BLOKLAMAZ, yalnızca reddeder.
    if (lockedOut(now)) { return ErrCode::WEB_UNAUTHORIZED; }

    if (g_failures >= MAX_FAILURES) { g_failures = 0; }   // kilit süresi doldu

    if (!verify(password))
    {
        if (g_failures < MAX_FAILURES) { ++g_failures; }
        if (g_failures >= MAX_FAILURES)
        {
            g_lockedAt = now;
            core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_UNAUTHORIZED, g_failures,
                            "kaba kuvvet kilidi devrede");
        }
        return ErrCode::WEB_UNAUTHORIZED;
    }

    g_failures = 0;

    // En eski slotu bul (boş yoksa en eskisini düşür).
    uint8_t slot = 0;
    for (uint8_t i = 0; i < MAX_SESSIONS; ++i)
    {
        if (!g_sessions[i].used) { slot = i; break; }
        if (g_sessions[i].createdAt.v < g_sessions[slot].createdAt.v) { slot = i; }
    }

    // Token DONANIM RNG'sinden. `millis()` veya `random()` tabanlı token
    // tahmin edilebilir ve kabul edilemez.
    uint8_t raw[TOKEN_BYTES];
    for (uint8_t i = 0; i < TOKEN_BYTES; ++i)
    {
        raw[i] = static_cast<uint8_t>(esp_random() & 0xFFu);
    }
    toHex(raw, TOKEN_BYTES, g_sessions[slot].token);
    memset(raw, 0, sizeof(raw));

    g_sessions[slot].createdAt  = now;
    g_sessions[slot].lastSeenAt = now;
    g_sessions[slot].used       = true;

    memcpy(tokenOut, g_sessions[slot].token, TOKEN_HEX_LEN + 1);
    return ErrCode::OK;
}

bool validate(const char* token, Millis now)
{
    if (token == nullptr || !g_configured) { return false; }
    if (strnlen(token, TOKEN_HEX_LEN + 1) != TOKEN_HEX_LEN) { return false; }

    for (uint8_t i = 0; i < MAX_SESSIONS; ++i)
    {
        if (!g_sessions[i].used) { continue; }
        if (core::hasElapsed(now, g_sessions[i].createdAt, core::millisecs(SESSION_TTL_MS)))
        {
            continue;
        }
        // Sabit zamanlı: `strcmp` token'ın kaç karakterinin doğru olduğunu
        // süresiyle sızdırır.
        if (constantTimeEquals(token, g_sessions[i].token, TOKEN_HEX_LEN))
        {
            g_sessions[i].lastSeenAt = now;
            return true;
        }
    }
    return false;
}

void logout(const char* token)
{
    if (token == nullptr) { return; }
    for (uint8_t i = 0; i < MAX_SESSIONS; ++i)
    {
        if (g_sessions[i].used && constantTimeEquals(token, g_sessions[i].token, TOKEN_HEX_LEN))
        {
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
            return;
        }
    }
}

void invalidateAll() { memset(g_sessions, 0, sizeof(g_sessions)); }

void tick(Millis now)
{
    for (uint8_t i = 0; i < MAX_SESSIONS; ++i)
    {
        if (g_sessions[i].used &&
            core::hasElapsed(now, g_sessions[i].createdAt, core::millisecs(SESSION_TTL_MS)))
        {
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
        }
    }
}

core::ErrCode factoryReset()
{
    invalidateAll();
    memset(&g_cred, 0, sizeof(g_cred));
    g_configured = false;
    g_failures   = 0;
    return ErrCode::OK;
}

} // namespace auth
} // namespace web
} // namespace interfaces
