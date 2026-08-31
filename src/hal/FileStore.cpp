#include "FileStore.h"

#include <FS.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdio.h>
#include <string.h>

#include <atomic>

#include "core/Diagnostics.h"

namespace hal {
namespace fs {

/// `partitions.csv`'deki bölümün ADI. Alt tipi "spiffs" ama adı bu —
/// Arduino LittleFS bölümü ADIYLA arar, alt tipiyle değil.
constexpr const char* PARTITION_LABEL = "littlefs";

namespace {

using core::ErrCode;

SemaphoreHandle_t     g_mutex = nullptr;
StaticSemaphore_t     g_mutexStruct;
bool                  g_mounted   = false;
bool                  g_formatted = false;
std::atomic<uint32_t> g_writeErrors{0};

/// Sınırlı bekleme — sonsuz bekleme yasak (CODING_STANDARDS §7).
/// Dosya işlemleri uzun sürebildiği için NVS'ten cömert bir değer.
constexpr TickType_t LOCK_TIMEOUT = pdMS_TO_TICKS(2000);

inline bool lockAcquire()
{
    return g_mutex != nullptr && xSemaphoreTake(g_mutex, LOCK_TIMEOUT) == pdTRUE;
}

inline void lockRelease()
{
    xSemaphoreGive(g_mutex);
}

} // namespace

ErrCode begin()
{
    if (g_mounted)
    {
        // Mevcut sistemde LittleFS İKİ KEZ mount ediliyordu (`setup()` +
        // `WebServerManager::begin()`). Burada tekrar çağrı zararsızdır.
        return ErrCode::OK;
    }

    if (g_mutex == nullptr)
    {
        g_mutex = xSemaphoreCreateMutexStatic(&g_mutexStruct);
    }

    // ÖNCE biçimlendirmeden dene: geçici bir mount hatasında veri kurtarılabilir.
    // BÖLÜM ETİKETİ AÇIKÇA VERİLİR.
    //
    // `LittleFS.begin()` varsayılan olarak "spiffs" ADLI bölümü arar
    // (`LittleFS.h:27`), ama `partitions.csv`'deki bölümün ADI "littlefs"
    // (alt tipi "spiffs"). Etiket verilmezse mount
    //     `partition "spiffs" could not be found`
    // ile başarısız olur ve sistem her boot'ta DEGRADED'a düşer.
    if (LittleFS.begin(false, "/littlefs", 10, PARTITION_LABEL))
    {
        g_mounted = true;
        return ErrCode::OK;
    }

    // Kalıcı bozulma. Biçimlendirmek tek seçenek — ama SESSİZCE DEĞİL:
    // bu işlem web varlıklarını ve geçmiş veriyi siler.
    core::diag::log(core::LogLevel::CRITICAL, ErrCode::STORAGE_FS_MOUNT_FAILED, 0,
                    "LittleFS mount edilemedi — BICIMLENDIRILIYOR, web varliklari silinecek");

    if (LittleFS.begin(true, "/littlefs", 10, PARTITION_LABEL))
    {
        g_mounted   = true;
        g_formatted = true;
        return ErrCode::OK;
    }

    // Biçimlendirme de başarısız. Sistem DURMAZ: web statiği olmadan çalışır
    // (ARCHITECTURE §16.3). `isMounted()` false döner.
    core::diag::log(core::LogLevel::ERROR, ErrCode::STORAGE_FS_MOUNT_FAILED, 0,
                    "LittleFS bicimlendirilemedi — dosya sistemi kullanilamiyor");
    return ErrCode::STORAGE_FS_MOUNT_FAILED;
}

bool isMounted()
{
    return g_mounted;
}

bool wasFormatted()
{
    return g_formatted;
}

bool exists(const char* path)
{
    if (!g_mounted || path == nullptr)
    {
        return false;
    }
    if (!lockAcquire())
    {
        return false;
    }
    const bool r = LittleFS.exists(path);
    lockRelease();
    return r;
}

ErrCode readFile(const char* path, void* buf, size_t& len)
{
    if (!g_mounted)
    {
        return ErrCode::STORAGE_FS_MOUNT_FAILED;
    }
    if (path == nullptr || buf == nullptr || len == 0)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    if (!lockAcquire())
    {
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    File f = LittleFS.open(path, "r");
    if (!f)
    {
        lockRelease();
        return ErrCode::CFG_NOT_FOUND;
    }

    const size_t n = f.read(static_cast<uint8_t*>(buf), len);
    f.close();  // tanıtıcı aynı kapsamda kapatılır — sızıntı yok
    lockRelease();

    len = n;
    return ErrCode::OK;
}

ErrCode writeFile(const char* path, const void* data, size_t len)
{
    if (!g_mounted)
    {
        return ErrCode::STORAGE_FS_MOUNT_FAILED;
    }
    if (path == nullptr || data == nullptr || len == 0)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    if (!lockAcquire())
    {
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    File f = LittleFS.open(path, "w");
    if (!f)
    {
        lockRelease();
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    const size_t written = f.write(static_cast<const uint8_t*>(data), len);
    f.close();
    lockRelease();

    if (written != len)
    {
        // Kısmi yazma genellikle dolu dosya sistemi demektir. Sessizce
        // geçilmez — ayarların kaydedilmediğini gizlerdi.
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        core::diag::log(core::LogLevel::ERROR, ErrCode::STORAGE_FULL,
                        static_cast<int32_t>(written), "dosya kismi yazildi");
        return ErrCode::STORAGE_FULL;
    }
    return ErrCode::OK;
}

ErrCode appendFile(const char* path, const void* data, size_t len)
{
    if (!g_mounted)
    {
        return ErrCode::STORAGE_FS_MOUNT_FAILED;
    }
    if (path == nullptr || data == nullptr || len == 0)
    {
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    if (!lockAcquire())
    {
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    File f = LittleFS.open(path, "a");
    if (!f)
    {
        lockRelease();
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    const size_t written = f.write(static_cast<const uint8_t*>(data), len);
    f.close();
    lockRelease();

    if (written != len)
    {
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return ErrCode::STORAGE_FULL;
    }
    return ErrCode::OK;
}

// --- Konumlu erişim (TASK-058) ---------------------------------------------

ErrCode readAt(const char* path, uint32_t offset, void* buf, size_t len)
{
    if (!g_mounted) { return ErrCode::STORAGE_FS_MOUNT_FAILED; }
    if (path == nullptr || buf == nullptr || len == 0) { return ErrCode::CFG_VALIDATION_FAILED; }
    if (!lockAcquire()) { return ErrCode::STORAGE_WRITE_FAILED; }

    File f = LittleFS.open(path, "r");
    if (!f) { lockRelease(); return ErrCode::CFG_NOT_FOUND; }

    if (!f.seek(offset))
    {
        f.close();
        lockRelease();
        return ErrCode::STORAGE_RECORD_CORRUPT;
    }

    const size_t got = f.read(static_cast<uint8_t*>(buf), len);
    f.close();
    lockRelease();

    // Kısmi okuma bozuk/eksik kayıt demektir; sessizce geçilmez.
    return (got == len) ? ErrCode::OK : ErrCode::STORAGE_RECORD_CORRUPT;
}

ErrCode writeAt(const char* path, uint32_t offset, const void* data, size_t len)
{
    if (!g_mounted) { return ErrCode::STORAGE_FS_MOUNT_FAILED; }
    if (path == nullptr || data == nullptr || len == 0) { return ErrCode::CFG_VALIDATION_FAILED; }
    if (!lockAcquire()) { return ErrCode::STORAGE_WRITE_FAILED; }

    // "r+" var olan dosyayı içeriğini KORUYARAK açar. "w" tüm dosyayı
    // siler — halka dosyada bu tüm geçmişi yok etmek olurdu.
    File f = LittleFS.open(path, LittleFS.exists(path) ? "r+" : "w+");
    if (!f)
    {
        lockRelease();
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    if (!f.seek(offset))
    {
        f.close();
        lockRelease();
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        return ErrCode::STORAGE_WRITE_FAILED;
    }

    const size_t written = f.write(static_cast<const uint8_t*>(data), len);
    f.close();
    lockRelease();

    if (written != len)
    {
        g_writeErrors.fetch_add(1, std::memory_order_relaxed);
        core::diag::log(core::LogLevel::ERROR, ErrCode::STORAGE_FULL,
                        static_cast<int32_t>(written), "konumlu yazma eksik");
        return ErrCode::STORAGE_FULL;
    }
    return ErrCode::OK;
}

ErrCode ensureSize(const char* path, uint32_t size)
{
    if (!g_mounted) { return ErrCode::STORAGE_FS_MOUNT_FAILED; }
    if (fileSize(path) >= size) { return ErrCode::OK; }

    // Son bayta 0 yazmak dosyayı o boyuta uzatır. Tamamını sıfırla
    // doldurmuyoruz: 480 KB'lık bir yazma saniyeler sürer ve halka mantığı
    // zaten geçersiz kayıtları CRC ile eliyor.
    const uint8_t zero = 0;
    return writeAt(path, (size > 0u) ? (size - 1u) : 0u, &zero, 1);
}

ErrCode removeFile(const char* path)
{
    if (!g_mounted)
    {
        return ErrCode::STORAGE_FS_MOUNT_FAILED;
    }
    if (!lockAcquire())
    {
        return ErrCode::STORAGE_WRITE_FAILED;
    }
    const bool r = LittleFS.remove(path);
    lockRelease();
    return r ? ErrCode::OK : ErrCode::CFG_NOT_FOUND;
}

uint32_t fileSize(const char* path)
{
    if (!g_mounted || !lockAcquire())
    {
        return 0;
    }
    File f = LittleFS.open(path, "r");
    const uint32_t n = f ? static_cast<uint32_t>(f.size()) : 0u;
    if (f)
    {
        f.close();
    }
    lockRelease();
    return n;
}

void resolveAsset(const char* requestPath, AssetPath& out)
{
    out.gzipped = false;
    out.exists  = false;
    out.path[0] = '\0';

    if (requestPath == nullptr || requestPath[0] == '\0')
    {
        return;
    }

    // Önce gzip'li sürüm: web varlıkları önceden sıkıştırılmış olarak
    // yüklenir (TASK-047). Mevcut sistemdeki 298 KB sıkıştırılmamış
    // Bootstrap dosyası hem flash hem bant genişliği israfıydı.
    char gz[sizeof(out.path)];
    const int n = snprintf(gz, sizeof(gz), "%s.gz", requestPath);
    if (n > 0 && static_cast<size_t>(n) < sizeof(gz) && exists(gz))
    {
        strncpy(out.path, gz, sizeof(out.path) - 1);
        out.path[sizeof(out.path) - 1] = '\0';
        out.gzipped                    = true;
        out.exists                     = true;
        return;
    }

    if (exists(requestPath))
    {
        strncpy(out.path, requestPath, sizeof(out.path) - 1);
        out.path[sizeof(out.path) - 1] = '\0';
        out.exists                     = true;
    }
}

void stats(FsStats& out)
{
    out.totalBytes  = 0;
    out.usedBytes   = 0;
    out.freeBytes   = 0;
    out.writeErrors = g_writeErrors.load(std::memory_order_relaxed);

    if (!g_mounted || !lockAcquire())
    {
        return;
    }
    out.totalBytes = static_cast<uint32_t>(LittleFS.totalBytes());
    out.usedBytes  = static_cast<uint32_t>(LittleFS.usedBytes());
    out.freeBytes  = (out.totalBytes > out.usedBytes) ? (out.totalBytes - out.usedBytes) : 0u;
    lockRelease();
}

} // namespace fs
} // namespace hal
