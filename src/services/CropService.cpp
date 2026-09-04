#include "CropService.h"

#include "ConfigService.h"
#include "TimeService.h"
#include "core/ConfigValidation.h"
#include "core/Diagnostics.h"

namespace services {
namespace crop {
namespace {

using core::ConfigError;
using core::CropId;
using core::CropProfile;
using core::ErrCode;
using core::GrowthStage;
using core::Intensity;

/// Bir günün saniyesi — gün sayımı için.
constexpr int64_t SECONDS_PER_DAY = 86400;

/// Dönem ilerlemesi bu sıklıkta kontrol edilir.
///
/// Dönem GÜN mertebesinde değişir; her 100 ms'lik ağ turunda tarih hesabı
/// yapmak yalnızca CPU yakar. Bir saat, en kötü durumda dönem geçişini bir
/// saat geciktirir — 21 günlük bir dönemde bu ölçülemez bir farktır.
constexpr core::Duration STAGE_CHECK_PERIOD = core::hours(1);

core::Millis g_lastCheck  = core::Millis{0};
bool         g_everChecked = false;

/// Config'ten "hangi röleler bağlı" ve "hangi sensörler takılı" bayraklarını
/// çıkarır. `buildCropRules` çalışamayacak kural üretmesin diye gerekli.
void collectHardware(bool (&acts)[core::MAX_ACTUATORS], bool (&sens)[core::MAX_SENSORS])
{
    const core::Config& cfg = config::get();
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        acts[i] = cfg.actuators[i].enabled != 0u;
    }
    for (uint8_t i = 0; i < core::MAX_SENSORS; ++i)
    {
        sens[i] = cfg.sensors[i].enabled != 0u;
    }
}

/// Mevcut kümede kaç ETKİN kural var? Önizlemede "üzerine yazılacak" sayısı.
uint8_t activeRuleCount()
{
    const core::RuleSet& rs = config::get().rules;
    uint8_t n = 0;
    for (uint8_t i = 0; i < rs.count && i < core::MAX_RULES; ++i)
    {
        if (rs.rules[i].kind != core::RuleKind::INACTIVE)
        {
            ++n;
        }
    }
    return n;
}

/// Seçimi bir `CropConfig`'e doldurur (mevcut değerleri temel alarak).
core::CropConfig makeSelection(CropId id, GrowthStage stage, Intensity intensity,
                               int64_t plantedAtEpoch, bool autoStage)
{
    core::CropConfig cc = config::get().crop;
    cc.crop            = id;
    cc.stage           = stage;
    cc.intensity       = intensity;
    cc.plantedAtEpoch  = plantedAtEpoch;
    cc.autoStage       = autoStage ? 1u : 0u;
    cc.derivedFrom     = id;  // `CUSTOM`'a düşerse hangi profilden geldiği kalsın
    return cc;
}

/// Ortak plan hesabı — `preview()` ve `apply()` AYNI yolu kullanır.
///
/// İki ayrı hesap yazılsaydı, önizlemede "uygulanabilir" görünen bir seçim
/// uygulama anında reddedilebilirdi.
ErrCode computePlan(CropId id, GrowthStage stage, Intensity intensity, CropPlan& out)
{
    out = CropPlan{};
    out.stage = stage;
    out.error = core::configOk();

    const CropProfile* p = core::cropById(id);
    if (p == nullptr)
    {
        out.error = ConfigError{ErrCode::CFG_VALIDATION_FAILED, "crop.crop"};
        return out.error.code;
    }

    if (!core::stageValidFor(*p, stage))
    {
        out.error = ConfigError{ErrCode::CFG_VALIDATION_FAILED, "crop.stage"};
        return out.error.code;
    }

    bool acts[core::MAX_ACTUATORS];
    bool sens[core::MAX_SENSORS];
    collectHardware(acts, sens);

    out.ruleCount     = core::buildCropRules(*p, stage, intensity, acts, sens, out.rules);
    out.replacedCount = activeRuleCount();

    // Yazılacakmış gibi TAM doğrulama. Önizleme "uygulanabilir" dediyse
    // `apply()` de başarılı olmalıdır — aksi hâlde onay ekranı yanıltıcıdır.
    out.error = core::cfgvalid::validateRules(out.rules, config::get().sensors);
    return out.error.code;
}

} // namespace

ErrCode preview(CropId id, GrowthStage stage, Intensity intensity, CropPlan& out)
{
    return computePlan(id, stage, intensity, out);
}

ErrCode apply(CropId id, GrowthStage stage, Intensity intensity, int64_t plantedAtEpoch,
              bool autoStage, CropPlan& out)
{
    // --- 1) Seçimi ÖNCE doğrula ---
    //
    // Geçersizse hiçbir şey yazılmaz. Kurallar önce yazılsaydı, geçersiz bir
    // seçimde config'te "kurallar çilek ama ürün seçili değil" kalırdı.
    const core::CropConfig cc = makeSelection(id, stage, intensity, plantedAtEpoch, autoStage);
    const ConfigError      ce = core::cfgvalid::validateCrop(cc);
    if (!ce.ok())
    {
        out       = CropPlan{};
        out.stage = stage;
        out.error = ce;
        return ce.code;
    }

    // --- 2) Kuralları üret ve doğrula ---
    const ErrCode planRc = computePlan(id, stage, intensity, out);
    if (planRc != ErrCode::OK)
    {
        return planRc;
    }

    // --- 3) Kural kümesini yaz ---
    const ConfigError re = config::updateRules(out.rules);
    if (!re.ok())
    {
        out.error = re;
        return re.code;
    }

    // --- 4) Seçimi yaz ---
    //
    // 1. adımda aynı değer zaten doğrulandı; burada başarısız OLAMAZ. Yine de
    // dönüş değeri yok sayılmıyor: sessizce yutulan bir hata, kuralların
    // yazıldığı ama seçimin yazılmadığı bir duruma yol açardı.
    const ConfigError se = config::updateCrop(cc);
    if (!se.ok())
    {
        out.error = se;
        return se.code;
    }

    core::diag::log(core::LogLevel::INFO, ErrCode::OK,
                    static_cast<int32_t>(out.ruleCount), "urun profili uygulandi");

    out.error = core::configOk();
    return ErrCode::OK;
}

void markCustomized()
{
    core::CropConfig cc = config::get().crop;

    // Ürün seçilmemişse söylenecek bir şey yok; zaten CUSTOM ise tekrar
    // yazmak boş yere flash aşındırır.
    if (cc.crop == CropId::NONE || cc.crop == CropId::CUSTOM)
    {
        return;
    }

    cc.derivedFrom = cc.crop;
    cc.crop        = CropId::CUSTOM;

    // Elle düzenlenmiş kurallar artık dönem tablosuna karşılık gelmiyor;
    // otomatik ilerleme onları sessizce geri yazarsa kullanıcının
    // düzenlemesi kaybolurdu.
    cc.autoStage = 0;

    (void)config::updateCrop(cc);

    core::diag::log(core::LogLevel::INFO, ErrCode::OK,
                    static_cast<int32_t>(static_cast<uint8_t>(cc.derivedFrom)),
                    "kurallar elle degistirildi — profil OZEL'e dusuruldu");
}

uint32_t daysSincePlanting()
{
    const core::CropConfig& cc = config::get().crop;

    if (cc.plantedAtEpoch <= 0 || !timesvc::valid())
    {
        return 0;
    }

    const core::EpochSeconds nowEpoch = timesvc::epoch();
    if (nowEpoch.s <= cc.plantedAtEpoch)
    {
        // Dikim tarihi gelecekte: kullanıcı yanlış tarih girmiş olabilir veya
        // saat geriye alınmıştır. Negatif gün üretmek yerine 0 döneriz —
        // dönem başlangıçta kalır, geri gitmez.
        return 0;
    }

    return static_cast<uint32_t>((nowEpoch.s - cc.plantedAtEpoch) / SECONDS_PER_DAY);
}

bool autoStageActive()
{
    const core::CropConfig& cc = config::get().crop;
    return cc.autoStage != 0u && cc.crop != CropId::NONE && cc.crop != CropId::CUSTOM &&
           cc.plantedAtEpoch > 0 && timesvc::valid();
}

core::GrowthStage currentStage()
{
    return config::get().crop.stage;
}

void tick(core::Millis now)
{
    if (g_everChecked && !core::hasElapsed(now, g_lastCheck, STAGE_CHECK_PERIOD))
    {
        return;
    }
    g_lastCheck   = now;
    g_everChecked = true;

    if (!autoStageActive())
    {
        return;
    }

    const core::CropConfig& cc = config::get().crop;
    const CropProfile*      p  = core::cropById(cc.crop);
    if (p == nullptr)
    {
        return;
    }

    const GrowthStage want = core::stageForDay(*p, daysSincePlanting());
    if (want == cc.stage)
    {
        return;
    }

    // DÖNEM GERİ ALINMAZ. `stageForDay` monotoniktir ama saat geriye
    // alınırsa (SNTP düzeltmesi) daha küçük bir dönem hesaplanabilir. Geri
    // gitmek EC hedefini yarıya indirir ve meyve dönemindeki bitkiyi aç
    // bırakır; ileri gitmemek ise yalnızca gecikme yaratır.
    if (static_cast<uint8_t>(want) < static_cast<uint8_t>(cc.stage))
    {
        return;
    }

    CropPlan plan{};
    const ErrCode rc =
        apply(cc.crop, want, cc.intensity, cc.plantedAtEpoch, cc.autoStage != 0u, plan);

    if (rc == ErrCode::OK)
    {
        core::diag::log(core::LogLevel::INFO, ErrCode::OK,
                        static_cast<int32_t>(static_cast<uint8_t>(want)),
                        "gelisim donemi ilerledi — kurallar guncellendi");
    }
    else
    {
        // Sessiz başarısızlık YOK: dönem ilerlemedi ve nedeni kayda geçti.
        core::diag::log(core::LogLevel::WARNING, rc,
                        static_cast<int32_t>(static_cast<uint8_t>(want)),
                        "donem ilerletilemedi");
    }
}

} // namespace crop
} // namespace services
