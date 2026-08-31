#pragma once

// NVS anahtar-değer deposu — TASK-013
//
// Mevcut sistemin ham `EEPROM.h` kullanımının yerini alır. O yaklaşımın üç
// problemi vardı (REQUIREMENTS §7, ARCHITECTURE §15.2):
//   · tek alan değişse bile tüm blok yeniden yazılıyordu → flash aşınması
//   · yazma sırasında güç kesilirse TÜM config kayboluyordu → tek nokta hatası
//   · yapı değişince eski kayıt tamamen geçersiz oluyordu → migration imkânsız
//
// NVS bunları anahtar bazında atomik yazma, aşınma dengeleme ve alan bazında
// evrimle çözer.
//
// NEDEN ARDUINO `Preferences` DEĞİL: `Preferences` hata YUTAR — `putString()`
// başarısızlıkta 0 döner ama nedenini söylemez (dolu mu, bozuk mu, anahtar mı
// uzun). `STORAGE_FULL` ile `STORAGE_WRITE_FAILED` farklı davranış gerektirir;
// ham `nvs_*` API'si `esp_err_t` döndürür ve `ErrCode`'a eşlenebilir.
//
// KATMAN SINIRI: bu bir L1 sürücüsüdür. Yazma SENKRONDUR ve flash hızında
// çalışır. "Çağıran beklemesin" kuralı üst katmanın sorumluluğudur — `store`
// task'ı (TASK-059) bu sürücüyü kendi bağlamından çağıracaktır. Sürücüye
// kuyruk koymak iş kuralı eklemek olurdu (D6).

#include <stddef.h>
#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {

/// NVS anahtar adı üst sınırı. `NVS_KEY_NAME_MAX_SIZE = 16` (sonlandırıcı dahil)
/// olduğu için 15 kullanılabilir karakter vardır.
constexpr size_t NVS_KEY_MAX_LEN = 15;

/// Konfigürasyon namespace'i.
constexpr const char* NS_CONFIG = "cfg";

/// Sırlar namespace'i — bilinçli olarak AYRI (bkz. SecretStore.h).
constexpr const char* NS_SECRET = "sec";

/// Sistem/güvenlik kalıcı kayıtları — `NS_CONFIG`'ten AYRI (TASK-032).
///
/// Fabrika ayarlarına dönüş `NS_CONFIG`'i siler. Acil durum mandalı orada
/// dursaydı, bir yapılandırma sıfırlaması bir GÜVENLİK MANDALINI sessizce
/// temizlerdi — üstelik "koşullar düzeldi mi" kontrolünü atlayarak.
constexpr const char* NS_SYSTEM = "sys";

namespace nvsstore {

/// NVS bölümünü başlatır.
///
/// Bölüm dolu veya farklı sürümle yazılmışsa yeniden biçimlendirilir —
/// aksi halde sistem hiç çalışamaz. Ancak bu **TÜM AYARLARI SİLER** ve
/// bu yüzden **sessizce yapılmaz**: CRITICAL loglanır ve
/// `wasReformatted()` ile sorgulanabilir olur (ARCHITECTURE §16.4 —
/// sessiz varsayılana dönüş yasak; kullanıcı ayarlarının neden gittiğini
/// bilmelidir).
///
/// @return ErrCode::OK veya STORAGE_NVS_INIT_FAILED
core::ErrCode begin();

/// Bu boot'ta NVS bölümü yeniden biçimlendirildi mi?
/// `true` ise kullanıcının ayarları silinmiş demektir; boot raporuna ve
/// arayüze yansıtılmalıdır.
bool wasReformatted();

// --- Okuma / yazma ----------------------------------------------------------
//
// Tüm yazmalar `nvs_commit()` ile tamamlanır. Dönüş değerleri kontrol
// edilmeden bırakılmaz (CODING_STANDARDS Y11).

core::ErrCode setU32(const char* ns, const char* key, uint32_t value);
core::ErrCode getU32(const char* ns, const char* key, uint32_t& out);

core::ErrCode setString(const char* ns, const char* key, const char* value);

/// @param bufLen giriş: tampon boyutu · çıkış: yazılan uzunluk (sonlandırıcı dahil)
core::ErrCode getString(const char* ns, const char* key, char* buf, size_t& bufLen);

core::ErrCode setBlob(const char* ns, const char* key, const void* data, size_t len);

/// @param len giriş: tampon boyutu · çıkış: okunan bayt sayısı
core::ErrCode getBlob(const char* ns, const char* key, void* out, size_t& len);

/// Anahtarın var olup olmadığını, **değerini okumadan** bildirir.
/// Sır maskelemesinin temeli budur (bkz. SecretStore).
bool exists(const char* ns, const char* key);

core::ErrCode eraseKey(const char* ns, const char* key);

/// Bir namespace'in tamamını siler. Factory reset altyapısı (TASK-015).
core::ErrCode eraseNamespace(const char* ns);

/// Yazma hatalarının toplam sayısı — teşhis için.
uint32_t writeErrorCount();

} // namespace nvsstore
} // namespace hal
