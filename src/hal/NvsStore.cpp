#include "NvsStore.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>

#include <atomic>

#include "core/Diagnostics.h"

namespace hal {
namespace nvsstore {
namespace {

using core::ErrCode;

bool                  g_ready       = false;
bool                  g_reformatted = false;
std::atomic<uint32_t> g_writeErrors{0};

/// `esp_err_t` → `ErrCode`. Neden bilgisi KAYBOLMAZ: dolu bölüm ile
/// yazma hatası farklı davranışlar gerektirir.
ErrCode mapError(esp_err_t e)
{
    switch (e)
    {
    case ESP_OK:                    return ErrCode::OK;
    case ESP_ERR_NVS_NOT_FOUND:     return ErrCode::CFG_NOT_FOUND;
    case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
    case ESP_ERR_NVS_PAGE_FULL:     return ErrCode::STORAGE_FULL;
    case ESP_ERR_NVS_INVALID_LENGTH:
    case ESP_ERR_NVS_INVALID_NAME:
    case ESP_ERR_NVS_KEY_TOO_LONG:  return ErrCode::CFG_VALIDATION_FAILED;
    case ESP_ERR_NVS_NOT_INITIALIZED:
    case ESP_ERR_NVS_PART_NOT_FOUND: return ErrCode::STORAGE_NVS_INIT_FAILED;
    default:                        return ErrCode::STORAGE_WRITE_FAILED;
    }
}

/// Anahtar adı NVS sınırına uyuyor mu? Uzun anahtar sessizce kesilmez,
/// açıkça reddedilir.
bool keyValid(const char* key)
{
    return key != nullptr && key[0] != '\0' && strnlen(key, NVS_KEY_MAX_LEN + 1) <= NVS_KEY_MAX_LEN;
}

/// Handle açar. Çağıran her zaman kapatmakla yükümlüdür.
ErrCode openHandle(const char* ns, nvs_open_mode_t mode, nvs_handle_t& out)
{
    if (!g_ready || ns == nullptr)
    {
        return ErrCode::STORAGE_NVS_INIT_FAILED;
    }
    const esp_err_t e = nvs_open(ns, mode, &out);
    return mapError(e);
}

/// Yazma sonrası commit + hata sayacı. Sessiz başarısızlık olmaz.
ErrCode finishWrite(nvs_handle_t h, esp_err_t writeErr)
{
    if (writeErr != ESP_OK)
    {
        nvs_close(h);
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return mapError(writeErr);
    }

    const esp_err_t c = nvs_commit(h);
    nvs_close(h);

    if (c != ESP_OK)
    {
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return mapError(c);
    }
    return ErrCode::OK;
}

} // namespace

ErrCode begin()
{
    if (g_ready)
    {
        return ErrCode::OK;
    }

    esp_err_t e = nvs_flash_init();

    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Bölüm kullanılamaz durumda. Biçimlendirmek TÜM AYARLARI SİLER —
        // Wi-Fi bilgileri, kalibrasyon, güvenlik eşikleri. Yine de yapılır,
        // aksi halde sistem hiç çalışamaz.
        //
        // AMA SESSİZCE DEĞİL: kullanıcı ayarlarının neden gittiğini bilmelidir
        // (ARCHITECTURE §16.4).
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::STORAGE_NVS_INIT_FAILED,
                        static_cast<int32_t>(e),
                        "NVS bolumu kullanilamaz — YENIDEN BICIMLENDIRILIYOR, ayarlar silinecek");

        const esp_err_t eraseErr = nvs_flash_erase();
        if (eraseErr != ESP_OK)
        {
            core::diag::log(core::LogLevel::CRITICAL, ErrCode::STORAGE_NVS_INIT_FAILED,
                            static_cast<int32_t>(eraseErr), "NVS silinemedi");
            return ErrCode::STORAGE_NVS_INIT_FAILED;
        }

        g_reformatted = true;
        e             = nvs_flash_init();
    }

    if (e != ESP_OK)
    {
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::STORAGE_NVS_INIT_FAILED,
                        static_cast<int32_t>(e), "NVS baslatilamadi");
        return ErrCode::STORAGE_NVS_INIT_FAILED;
    }

    g_ready = true;
    return ErrCode::OK;
}

bool wasReformatted()
{
    return g_reformatted;
}

ErrCode setU32(const char* ns, const char* key, uint32_t value)
{
    if (!keyValid(key))
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READWRITE, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    return finishWrite(h, nvs_set_u32(h, key, value));
}

ErrCode getU32(const char* ns, const char* key, uint32_t& out)
{
    if (!keyValid(key))
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READONLY, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    const esp_err_t e = nvs_get_u32(h, key, &out);
    nvs_close(h);
    return mapError(e);
}

ErrCode setString(const char* ns, const char* key, const char* value)
{
    if (!keyValid(key) || value == nullptr)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READWRITE, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    return finishWrite(h, nvs_set_str(h, key, value));
}

ErrCode getString(const char* ns, const char* key, char* buf, size_t& bufLen)
{
    if (!keyValid(key) || buf == nullptr || bufLen == 0)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READONLY, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    size_t          len = bufLen;
    const esp_err_t e   = nvs_get_str(h, key, buf, &len);
    nvs_close(h);
    if (e == ESP_OK)
    {
        bufLen = len;
    }
    return mapError(e);
}

ErrCode setBlob(const char* ns, const char* key, const void* data, size_t len)
{
    if (!keyValid(key) || data == nullptr || len == 0)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READWRITE, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    return finishWrite(h, nvs_set_blob(h, key, data, len));
}

ErrCode getBlob(const char* ns, const char* key, void* out, size_t& len)
{
    if (!keyValid(key) || out == nullptr || len == 0)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READONLY, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    size_t          n = len;
    const esp_err_t e = nvs_get_blob(h, key, out, &n);
    nvs_close(h);
    if (e == ESP_OK)
    {
        len = n;
    }
    return mapError(e);
}

bool exists(const char* ns, const char* key)
{
    if (!keyValid(key))
    {
        return false;
    }
    nvs_handle_t h;
    if (openHandle(ns, NVS_READONLY, h) != ErrCode::OK)
    {
        return false;
    }
    // Boyut sorgusu: DEĞER OKUNMAZ. Sır maskelemesinin temeli budur —
    // "şifre ayarlı mı?" sorusu şifreyi belleğe getirmeden yanıtlanır.
    size_t          len = 0;
    const esp_err_t e   = nvs_get_blob(h, key, nullptr, &len);
    if (e == ESP_ERR_NVS_INVALID_LENGTH || e == ESP_OK)
    {
        nvs_close(h);
        return true;
    }
    size_t          slen = 0;
    const esp_err_t se   = nvs_get_str(h, key, nullptr, &slen);
    nvs_close(h);
    return se == ESP_OK || se == ESP_ERR_NVS_INVALID_LENGTH;
}

ErrCode eraseKey(const char* ns, const char* key)
{
    if (!keyValid(key))
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READWRITE, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    const esp_err_t e = nvs_erase_key(h, key);
    // Zaten yoksa hata değil.
    return finishWrite(h, (e == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : e);
}

ErrCode eraseNamespace(const char* ns)
{
    nvs_handle_t h;
    const ErrCode rc = openHandle(ns, NVS_READWRITE, h);
    if (rc != ErrCode::OK)
    {
        return rc;
    }
    const ErrCode result = finishWrite(h, nvs_erase_all(h));
    core::diag::log(core::LogLevel::WARNING, ErrCode::OK, 0, "NVS namespace temizlendi");
    return result;
}

uint32_t writeErrorCount()
{
    return g_writeErrors.load(std::memory_order_relaxed);
}

} // namespace nvsstore
} // namespace hal
