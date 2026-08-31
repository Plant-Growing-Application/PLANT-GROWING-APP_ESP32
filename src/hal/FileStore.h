#pragma once

// LittleFS dosya sistemi sürücüsü — TASK-016
//
// MOUNT HATASI BİR DURUMDUR, İSTİSNA DEĞİL.
//
// Mevcut sistemde LittleFS mount hatası `setup()`'tan erken `return` ile
// sonuçlanıyor ve HİÇBİR TASK OLUŞMUYORDU (REQUIREMENTS Kritik Problem 4).
// Burada `isMounted()` sorgulanır: web statik dosya servis edemez ama
// otomasyon, güvenlik ve sensörler TAM ÇALIŞIR (ARCHITECTURE §16.3).
//
// EŞZAMANLILIK: LittleFS'in iş parçacığı güvenliği garantili değildir.
// `store` task'ı (geçmiş veri) ile AsyncTCP callback'i (web varlıkları)
// gerçekten eşzamanlı erişir; sürücü içinde mutex vardır.

#include <stddef.h>
#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {

/// Bir dosya yolunun çözümlenmiş hâli.
struct AssetPath
{
    /// Servis edilecek gerçek yol (`.gz` uzantılı olabilir).
    char path[64];
    /// true → `Content-Encoding: gzip` başlığı gerekir.
    bool gzipped;
    bool exists;
};

struct FsStats
{
    uint32_t totalBytes;
    uint32_t usedBytes;
    uint32_t freeBytes;
    uint32_t writeErrors;
};

namespace fs {

/// Dosya sistemini bağlar.
///
/// Önce **biçimlendirmeden** denenir: geçici bir mount hatasında veri
/// kurtarılabilir. Başarısız olursa biçimlendirilir — ancak SESSİZCE DEĞİL:
/// CRITICAL loglanır ve `wasFormatted()` ile sorgulanabilir (§16.4).
///
/// @return ErrCode::OK veya STORAGE_FS_MOUNT_FAILED
core::ErrCode begin();

/// Bağlı mı? Bağlı değilse üst katman kısıtlı çalışır, sistem durmaz.
bool isMounted();

/// Bu boot'ta dosya sistemi biçimlendirildi mi? `true` ise web varlıkları
/// ve geçmiş veri silinmiş demektir.
bool wasFormatted();

// --- Dosya işlemleri --------------------------------------------------------

bool exists(const char* path);

/// @param len giriş: tampon boyutu · çıkış: okunan bayt sayısı
core::ErrCode readFile(const char* path, void* buf, size_t& len);

core::ErrCode writeFile(const char* path, const void* data, size_t len);

/// Var olan dosyanın sonuna ekler; yoksa oluşturur.
core::ErrCode appendFile(const char* path, const void* data, size_t len);

core::ErrCode removeFile(const char* path);

// --- Konumlu erişim (TASK-058 halka dosyası) --------------------------------
//
// Halka dosya, sabit boyutlu kayıtlara RASTGELE erişim gerektirir. Tüm
// dosyayı okuyup yazmak 480 KB'lık bir dosyada hem imkânsız (RAM) hem
// yıkıcıdır (her kayıt için tam dosya yeniden yazımı = flash aşınması).
//
// D6 notu: konumlu okuma/yazma bir SÜRÜCÜ yeteneğidir, iş kuralı değil.
// Halka mantığı (sarma, dizin bulma, CRC) `services/` içindedir.

/// Belirtilen konumdan okur.
core::ErrCode readAt(const char* path, uint32_t offset, void* buf, size_t len);

/// Belirtilen konuma yazar. Dosya yoksa oluşturulur; kısaysa uzatılır.
core::ErrCode writeAt(const char* path, uint32_t offset, const void* data, size_t len);

/// Dosyanın en az `size` bayt olmasını sağlar. Zaten büyükse dokunmaz.
///
/// Halka dosya için ÖNCEDEN AYIRMA yapılmaz — dosya kullanıldıkça büyür.
/// 480 KB'lık bir dosyayı ilk boot'ta sıfırlarla doldurmak saniyeler sürer
/// ve hiçbir şey kazandırmaz.
core::ErrCode ensureSize(const char* path, uint32_t size);

/// Dosya boyutu; yoksa 0.
uint32_t fileSize(const char* path);

// --- Varlık çözümleme -------------------------------------------------------

/// İstenen yolu servis edilebilir bir varlığa çevirir.
///
/// `/style.css` istendiğinde `/style.css.gz` varsa onu seçer ve `gzipped`
/// bayrağını set eder. Web katmanı bu ayrıntıyı bilmez; yalnızca başlığı ekler.
void resolveAsset(const char* requestPath, AssetPath& out);

// --- Teşhis ------------------------------------------------------------------

void stats(FsStats& out);

} // namespace fs
} // namespace hal
