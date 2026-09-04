#pragma once

// Ürün profili uygulama servisi — TASK-068
//
// ── GÖREV BÖLÜMÜ ────────────────────────────────────────────────────────────
//   core/CropProfile  → katalog + SAF kural üretimi (donanımsız, host-test)
//   bu servis         → seçimi doğrula, uygula, kalıcılaştır, dönemi ilerlet
//   interfaces/web    → HTTP sözleşmesi
//
// Kural üretiminin `core/` içinde kalması bilinçlidir: "çilek meyve döneminde
// hangi kuralları üretir" sorusu PC'de test edilebilir olmalıdır.
//
// ── ÖNİZLEME / UYGULAMA AYRIMI ──────────────────────────────────────────────
// `preview()` hiçbir şey yazmaz. Kullanıcı çileği seçtiğinde arayüz önce
// "şunlar değişecek" diyebilsin diye vardır: mevcut kuralların ÜZERİNE
// yazılacağı bir işlemi onaysız yapmak, projenin kendi ilkesine aykırıdır
// (ConfigService: "sessiz varsayılana dönüş yok").
//
// ── M4 KAPISI KAPALI KALIR ──────────────────────────────────────────────────
// `apply()` `automation.mode`'a DOKUNMAZ. Üretilen kurallar etkindir ama motor
// yalnızca `AUTO` modda kural değerlendirir; mod varsayılan olarak `MANUAL`'dir
// ve oraya geçmek operatörün açık kararıdır. Yani profil uygulamak, cihazın
// kendiliğinden sulamaya başlaması DEMEK DEĞİLDİR.

#include <stdint.h>

#include "core/Config.h"
#include "core/ConfigValidation.h"  // ConfigError — `CropPlan` alan adıyla hata taşır
#include "core/CropProfile.h"
#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace services {
namespace crop {

/// Bir profil uygulandığında ne olacağının özeti — önizleme ve yanıt için.
struct CropPlan
{
    core::RuleSet     rules;        ///< üretilen kural kümesi
    uint8_t           ruleCount;    ///< üretilen kural sayısı
    uint8_t           replacedCount;///< ÜZERİNE YAZILACAK mevcut kural sayısı
    core::GrowthStage stage;        ///< uygulanacak dönem
    core::ConfigError error;        ///< `ok()` değilse plan uygulanamaz
};

/// Bir seçimin üreteceği planı hesaplar. **Hiçbir şey yazmaz.**
///
/// Üretilen küme, yazılacakmış gibi tam olarak doğrulanır: kullanıcı onay
/// ekranında "uygulanabilir" yazısını gördüyse `apply()` de başarılı olmalıdır.
///
/// @param id        ürün (katalogda olmalı)
/// @param stage     dönem (üründe geçerli olmalı)
/// @param intensity sulama yoğunluğu
/// @param out       hesaplanan plan
core::ErrCode preview(core::CropId id, core::GrowthStage stage, core::Intensity intensity,
                      CropPlan& out);

/// Seçimi uygular: kural kümesini ve ürün seçimini yazar.
///
/// SIRA ÖNEMLİ — önce doğrulama, sonra iki yazma:
/// ```
///   1. seçimi doğrula   → geçersizse HİÇBİR ŞEY yazılmaz
///   2. kuralları üret + doğrula + yaz
///   3. ürün seçimini yaz  (1'de zaten doğrulandı, burada başarısız olamaz)
/// ```
/// Ters sırada yazılsaydı, geçersiz bir kural kümesinde config'te "çilek
/// seçili ama çilek kuralları yok" gibi tutarsız bir durum kalırdı.
///
/// @param plantedAtEpoch dikim tarihi (0 = bilinmiyor)
/// @param autoStage      1 = dönem gün sayısına göre kendiliğinden ilerlesin
core::ErrCode apply(core::CropId id, core::GrowthStage stage, core::Intensity intensity,
                    int64_t plantedAtEpoch, bool autoStage, CropPlan& out);

/// Kurallar profilin dışında değiştirildi — seçimi `CUSTOM`'a düşürür.
///
/// `PUT /api/config/rules` bunu çağırır. Çağırmasaydı arayüz "Çilek profili
/// aktif" demeye devam eder ve YALAN SÖYLERDİ: ekranda çilek yazarken
/// kurallar artık çilek profilinin ürettiği kurallar olmazdı.
///
/// Zaten `CUSTOM` veya `NONE` ise işlemsizdir.
void markCustomized();

/// Otomatik dönem ilerlemesi. `net` task döngüsünden çağrılır.
///
/// Web API ile AYNI bağlamda koşar: config yazma yolu tektir ve `app_core`
/// yalnızca okur (mevcut `PUT /api/config/rules` deseninin aynısı).
///
/// ── ZAMAN GEÇERSİZSE HİÇBİR ŞEY YAPMAZ ──────────────────────────────────────
/// Donanımsal RTC yoktur (ISSUE-005). Güç kesintisi sonrası ağ da yoksa duvar
/// saati geçersizdir; gün saymak meyve dönemindeki çileği fideye döndürür ve
/// EC hedefini yarıya indirirdi. Dönem DONAR ve olduğu yerde kalır.
void tick(core::Millis now);

/// Şu an geçerli dönem (config'ten okunur).
core::GrowthStage currentStage();

/// Dikimden bu yana geçen gün. Zaman geçersiz veya dikim tarihi yoksa 0.
uint32_t daysSincePlanting();

/// Otomatik ilerleme şu an çalışabiliyor mu? (Zaman geçerli ve açık mı.)
///
/// Arayüz "dönem ilerlemesi duraklatıldı — saat geçersiz" diyebilsin diye.
bool autoStageActive();

} // namespace crop
} // namespace services
