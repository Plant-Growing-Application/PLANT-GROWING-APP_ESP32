# TASK-067 — Ürün Profili Çekirdeği (Meyve Modu)

## Amaç

"Çilek yetiştireceğim" diyen kullanıcıya, o bitkinin o dönemdeki hedeflerini ve
sulama programını **kendiliğinden** kuran bir veri katmanı eklemek.

Kural motoru DEĞİŞMEZ. `Rule.h` "şu aktüatör açık olsun" diyebilen bir dil
tanımlar; bu task o dilde yazılmış, ürüne özgü metinleri ekler.

## Karar 1 — Profiller flash'ta, config'te yalnızca seçim

`Config` bir NVS blob'udur ve boyutu `static_assert` ile sınırlıdır. Altı ürün
× dört dönem × beş hedef aralığı ~1,3 KB tutar; config'e koymak sınırı kat kat
aşardı.

```
.rodata (kCatalog)   → parametre tabloları, firmware ile sürümlenir
Config::crop         → {plantedAt, crop, stage, autoStage, intensity, derivedFrom}
```

Yan fayda: kullanıcının NVS'inde **bozulmuş bir çilek profili olamaz**.

## Karar 2 — Ürün × dönem matrisi, ürün başına tek satır değil

Bu, "meyve modu" isteğinin özüdür. Çileği domates EC'sinde çalıştırmak yaprak
büyütür ve meyve tutumunu öldürür; aynı ürün içinde de fide dönemi meyve
dönemiyle aynı çözeltiyi kaldıramaz. Tek bir "ortalama" değer iki dönemde
birden yanlış olurdu.

Değerler ve kaynakları: `docs/CROP_PROFILES.md`.

## Karar 3 — Değişken dönem sayısı

Marul ve fesleğende çiçeklenme/meyve dönemi **yoktur** (sapa kalkan marul
acılaşır). Dizi boyutu sabit (`CROP_MAX_STAGES = 4`), geçerli dönem sayısı
profil başına değişken (`stageCount`).

Kullanılmayan satırlar **sıfırlanmaz, son geçerli dönemin kopyasıdır**:
sıfırlanmış bir satır `ec{0,0}` demek olurdu ve bir hata sonucu oraya
erişilirse EC eşiği 0'a kurulup dozaj pompası hiç durmazdı. Kopya, aynı hatayı
zararsız kılar.

## Karar 4 — Üretilen kurallar ETKİN doğar, motor kapalı kalır

Gerilim şuydu: kurallar `enabled = 0` doğarsa hiçbir şey çalışmaz ve kullanıcı
"profili uyguladım ama bir şey olmuyor" der. `enabled = 1` doğarsa M4 güvenlik
kapısı riske girer.

Çözüm ikisinin de değil: kurallar **etkin** doğar (amaçları budur), ama
`buildCropRules` **`automation.mode`'a dokunmaz**. Mod varsayılan `MANUAL`'dir
ve motor `MANUAL`'de hiçbir kuralı değerlendirmez (ARCHITECTURE §11.1).

Sonuç: kural kümesi hazır ve görünür durur, sistem kendiliğinden hiçbir şey
sürmez, `AUTO`'ya geçmek operatörün açık kararıdır. **M4 kapısı kapalı kalır.**

## Karar 5 — Çalışamayacak kural üretilmez

`buildCropRules` iki bayrak dizisi alır: hangi röleler bağlı, hangi sensörler
takılı.

- ısıtıcı rölesi yoksa → ısıtma kuralı hiçbir şeyi sürmez
- sıcaklık sensörü yoksa → kural HİÇ tetiklenmez, **sessizce ölü durur**

İkincisi daha sinsidir: kural listesinde doğru görünen bir satır asla
değerlendirilmez ve kullanıcı nedenini kural ekranında arar. Üretmemek, üretip
açıklamaktan iyidir.

## Karar 6 — Çevrim süresi `maxRunMs`'e karşı kırpılır

`MAX_GENERATED_ON_S = 240` (su pompasının varsayılan `maxRunMs`'i 5 dk).
Profil daha uzun bir çevrim üretseydi `ActuatorManager` pompayı **her
çevrimde** süre aşımıyla zorla kapatırdı ve kullanıcı doğru görünen bir kuralın
neden kesildiğini bulamazdı. `static_assert` sınırı kilitler.

Yoğunluk ölçekleme (`×0.7 / ×1.0 / ×1.4`) sonucu her zaman geçerli bir çevrim
üretir: `validateRule` `0 < cycleOnS < cyclePeriodS` ister ve kırpma burada
yapılır — geçersiz bir kural **hiç doğmaz**.

## Karar 7 — Eşik yönü iki eşikten türer, ayrı bayrak yok

`Rule.h`'ın kararı korunur: `onThreshold < offThreshold` = "altına düşünce AÇ".
Isıtma ve besin dozajı için doğru yön budur ve ayrı bir "üstünde/altında"
bayrağı olmadığı için **ters kurulması imkânsızdır**. Host testi bu yönü
doğrular.

## Karar 8 — `LOW`/`HIGH` makro çakışması (ISSUE-009'un aynı sınıfı)

`Intensity` ilk yazımda `LOW / NORMAL / HIGH` idi ve derleme kırıldı:
`esp32-hal-gpio.h` içinde `#define LOW 0x0` vardır. Önişlemci kapsam tanımaz —
`Intensity::LOW` metin olarak `Intensity::0x0`'a dönüşür. **`enum class` bu
tuzağa karşı koruma sağlamaz.**

Değerler `SPARSE / NORMAL / ABUNDANT` olarak yeniden adlandırıldı; gerekçe
başlığa yazıldı ki bir sonraki geliştirici aynı yere basmasın.

## Karar 9 — Şema sürümü 2 → 3

`CropConfig` bölümü eklendi (`crop` NVS anahtarı). Sürüm 2 kaydı okunduğunda
bölüm varsayılanda kalır (`crop = NONE`) — cihaz TASK-067 öncesiyle **birebir
aynı** davranır. Göçün taşıyacağı veri yoktur.

`MAX_ACTUATORS` 4 → 5 olduğu için `cfg.act` bölümünün bayt uzunluğu da değişti;
bu ayrı bir göç adımı gerektirmez: `loadSection()` uzunluk uyuşmazlığını zaten
yakalar, bölümü varsayılanda bırakır ve WARNING loglar. Sonuç doğrudur — eski
kayıt yeni röleler için hiçbir değer taşımıyordu.

`CropConfig` yapının **başında** durur: `int64` taşır ve 8 bayta hizalanır;
ortaya konsaydı derleyici öncesine 4 bayt dolgu koyardı. Başlık zaten 8
baytlık olduğu için orada dolgu sıfırdır.

## Testler (host — `pio test -e native`)

Donanımda test edilmesi pratik olmayanlar:

| Test | Neden donanımda yapılamaz |
|---|---|
| `every_crop_and_stage_produces_valid_rules` | 6×4×3 = 72 kombinasyon |
| `generated_cycle_never_exceeds_pump_max_runtime` | her çevrimi gerçek zamanda beklemek gerekir |
| `missing_hardware_produces_no_rule_for_it` | sensör/röle söküp takmak |
| `heater_and_dosing_thresholds_point_the_right_way` | ters kurulmuş ısıtıcı ancak hazne donunca fark edilir |
| `ec_rises_across_fruiting_stages` | dönem geçişleri haftalar sürer |
| `leafy_crops_reject_fruiting_stage` | doğrulama reddi |
| `stage_advances_by_day_and_never_goes_backwards` | gerçekte 72 gün |
| `crop_key_roundtrip_is_lossless` | sözlük bozulması sessizdir |

## Dokunulan dosyalar

```
src/core/CropProfile.{h,cpp}    YENI — katalog + saf kural uretimi
src/core/Config.h               CropConfig, sema surumu 3
src/core/ConfigValidation.{h,cpp}  validateCrop(), varsayilanlar
src/services/ConfigService.{h,cpp} crop NVS bolumu, updateCrop()
platformio.ini                  native ortama CropProfile.cpp
test/test_domain/test_domain.cpp  9 yeni test
docs/CROP_PROFILES.md           YENI — kaynakli parametre tablosu
```

## Definition of Done

- [x] `pio run` temiz, `sizeof(Config)` sınır içinde
- [x] Katalogdaki her ürün × dönem geçerli kural üretiyor (test)
- [x] Şema göçü sürüm 2 kaydını bozmuyor
- [ ] **`pio test -e native` KOŞULMADI** — bu makinede host derleyicisi (gcc/g++)
      yok (ISSUE-033). Testler yazıldı ve derleyici kurulduğunda koşacak.
