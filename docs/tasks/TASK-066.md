# TASK-066 — Donanım Genişletme: İklim Röleleri ve I2C Ortam Sensörleri

## Amaç

Ürün profilinin üreteceği kuralların gidecek bir yeri olsun diye fiziksel
çıkışları ve ölçümleri tamamlamak:

- **çıkış:** büyütme ışığı, ısıtıcı, besin dozaj pompası
- **girdi:** ortam sıcaklığı, bağıl nem, aydınlık düzeyi

## Karar 1 — `AUX_1`/`AUX_2` kaldırıldı, yerine gerçek kimlikler geldi

Eski model iki "yardımcı" mantıksal aktüatör taşıyordu ve `RelayOutput` ikisini
de `PIN_UNMAPPED` olarak reddediyordu. Yani arayüzde açılıp **hiçbir şey
yapmayan** iki düğme vardı.

`MAX_ACTUATORS` 4 → 5 ve kimlikler anlamlandırıldı:

```
WATER_PUMP(16) · AIR_PUMP(17) · GROW_LIGHT(19) · HEATER(26) · NUTRIENT_PUMP(18)
```

Beşi de fiziksel pin taşır; eşlenmemiş slot kalmadı (ARCHITECTURE P7).

**Reddedilen:** `MAX_ACTUATORS = 6` bırakıp bir slotu yedek tutmak. Kartta
altıncı bir güvenli çıkış YOK; slot açmak tek başına işe yaramaz, yalnızca
16 bayt config ve bir ölü arayüz kartı üretirdi.

## Karar 2 — Pin bütçesi tükendi, bu belgelenerek kapatıldı

Kalan tek güvenli çıkış kümesi `{18, 19, 26}` idi ve üçü de kullanıldı.
`BoardPins.h`, bu üç pinin birbirleriyle ve I2C / encoder / dijital sensör
pinleriyle çakışmadığını `static_assert` ile zorlar; `relayPinsDistinct()`
on karşılaştırmayı tek ifadede yapar (C++11: `constexpr` gövdesinde döngü yok).

İki rölenin aynı pine verilmesi **sessiz ve tehlikeli** bir hatadır: "ışığı aç"
komutu ısıtıcıyı da çalıştırır ve `readback()` her ikisi için de "açık"
döndüğü için uyuşmazlık tespiti bile devreye girmez.

## Karar 3 — Ortam sensörleri I2C, çünkü ADC1 bütçesi sıfır

ISSUE-001 ile encoder GPIO 32/33'ü (ADC1_CH4/CH5) işgal etti; 34/35/36/39
zaten sensörlerde. **Analog bir ortam sensörü fiziksel olarak takılamazdı.**

AHT20 (0x38) ve BH1750 (0x23), OLED'in (0x3C) zaten kullandığı hatta oturur;
yeni pin gerekmez, adresler çakışmaz.

## Karar 4 — `hal::i2cbus` — hattın tek sahibi

`Wire.begin()` eskiden `OledPanel::begin()` içindeydi. Üç cihaz olunca bu
sürdürülemez: OLED takılı değilse hat kurulmadan sensörler okunmaya
çalışılırdı. Hat kurulumu ayrı bir modüle taşındı; çağrı **idempotenttir** ve
her kullanıcı kendi `begin()`'inde çağırır. Boot aşama sırasına bağımlılık yok
(ARCHITECTURE P4).

Eş zamanlılık: hattı `ui` (OLED) ve `io_sense` (sensörler) task'ları paylaşır.
Arduino `TwoWire` kendi mutex'ini taşır (`Wire.cpp`, `lock`) — işlemler
serileştirilir, yarış yoktur. Bekleme karşı tarafın işlem uzunluğuyla
sınırlıdır; bu yüzden I2C işlemleri kısa tutulur.

## Karar 5 — AHT20 iki fazlı okunur, BEKLEMEZ

Ölçüm ~80 ms sürer. `delay(80)`, `io_sense`'in 250 ms'lik periyodunun üçte
birini yerdi ve güvenlik sensörlerinin (seviye, akış) örneklemesini
geciktirirdi — kuru çalışma tespitini yavaşlatan bir ortam sensörü kabul
edilemez (ARCHITECTURE P3).

Durum makinesi: bir çağrı tetikler, sonraki çağrı okur ve hemen yeniden
tetikler. İlk ölçüm `begin()` içinde tetiklenir; boot ile ilk örnekleme turu
arasında geçen süre dönüşüm süresinden çok uzun olduğu için ilk `sample()`
hazır bir değer bulur ve sensör "arızalı" görünmez.

`service()` aynı turdaki ikinci çağrıyı yok sayar: sıcaklık ve nem
sarmalayıcıları TEK çipi paylaşır ve ikisi de aynı `now` ile gelir; korumasız
kalsaydı her tur iki kez tetikleme yapılır ve ölçüm hiç tamamlanmazdı.

## Karar 6 — CRC doğrulanır

AHT20 her okumaya CRC8 (polinom 0x31) ekler. Kontrol edilmeyen bir CRC, var
olmayan bir korumadır: bozuk bir aktarım 22 °C yerine 120 °C okutabilir ve bu
değer doğrudan ısıtıcı kuralının eşiğine girer.

## Karar 7 — Isıtıcı seviye kilitlerine bağlandı

`masksFor()` eskiden su pompası dışındaki her şeye yalnızca acil-durum + süre
kilidi veriyordu. Daldırma ısıtıcısı susuz çalışırsa **yanar ve hazneyi
eritir** — bu, kuru çalışan bir pompadan daha tehlikelidir.

Isıtıcı artık `ILK_LEVEL_INSUFFICIENT` ve `ILK_LEVEL_SENSOR_FAULT`
kilitlerinden de etkilenir. `ILK_DRY_RUN` verilmedi: akış doğrulama pompanın
çalışmasını ölçer, ısıtıcının değil.

Işık ve dozaj pompası seviyeden etkilenmez — ışık suya değmez, dozaj pompası
birkaç mL basar ve haznenin dolmasını beklemek gübrelemeyi geciktirir.

## Karar 8 — `maxRunMs` üst sınırı role göre ayrıldı

Tek global sınır (2 saat) artık yetmiyor:

| Aktüatör | Üst sınır | Gerekçe |
|---|---|---|
| pompalar | 2 sa | mevcut koruma |
| büyütme ışığı | 20 sa | fotoperiyot 14–18 saat |
| ısıtıcı | 6 sa | soğuk hazne saatler sürebilir, sonsuza kadar değil |
| dozaj pompası | **5 dk** | takılı kalan pompa bidonu boşaltır |

Sınırı global olarak 18 saate çıkarmak pompa korumasını yok ederdi; 2 saatte
bırakmak ışığı her 2 saatte bir söndürürdü ve kullanıcı nedenini kural
ekranında ararken bulamazdı. `limits::maxRunLimitFor()` sınırı indekse göre
seçer; enum hizası `static_assert` ile kilitlidir.

## Karar 9 — Varsayılanlar da role göre

Tek tip varsayılan (`maxRunMs = 5 dk`) sessiz bir arıza üretirdi: 16 saatlik
ışık penceresi kuralına rağmen ışık her 5 dakikada zorla kapanırdı.

Işık 18 sa · ısıtıcı 4 sa · dozaj 60 sn `maxRunMs` ile doğar. Dozaj pompasının
`cooldownMs`'i **10 dakikadır ve aşırı gübreleme korumasıdır**.

Üçü de `enabled = 0` doğar: kullanıcı o röleyi kablolamış olmak zorunda
değildir ve olmayan bir donanımı sürmeye çalışmak arayüzde "açtım ama bir şey
olmadı" olarak görünürdü.

## Karar 10 — Işık sensörünün geçerli aralığı

Genel varsayılan `{-1000, +1000}` ışık için sessiz bir arızadır: güneşli bir
serada 20 000 lüks olağandır ve her okuma `OUT_OF_RANGE` damgalanıp
kullanılamaz hâle gelirdi. Aralıklar veri sayfası sınırlarından alındı:
nem `{0,100}`, ortam sıcaklığı `{-40,85}`, ışık `{0,120000}`.

## Dokunulan dosyalar

```
src/core/BoardPins.h            + 3 role pini, cakisma static_assert'leri
src/core/SystemState.h          MAX_ACTUATORS 4→5, ActuatorId/SensorId genisledi
src/core/Config.h               maxRunLimitFor(), rol basina sinirlar
src/core/ConfigValidation.cpp   rol basina varsayilan + sinir, sensor araliklari
src/domain/models/SafetyState.h isitici seviye kilitlerine baglandi
src/hal/I2cBus.{h,cpp}          YENI — hattin tek sahibi
src/hal/Aht20.{h,cpp}           YENI — iki fazli, CRC'li
src/hal/Bh1750.{h,cpp}          YENI — surekli mod
src/hal/OledPanel.cpp           Wire.begin() → i2cbus::begin()
src/hal/RelayOutput.cpp         5 pinlik tablo + hiza static_assert'leri
src/services/sensors/EnvSensors.{h,cpp}  YENI — ISensor sarmalayicilari
src/services/sensors/SensorRegistry.h    5 → 8 sensor
src/services/sensors/ISensor.h  SensorUnit::LUX
src/services/SensorService.cpp  3 yeni ornek
src/interfaces/web/StateJson.cpp        yeni adlar
src/interfaces/web/WsProtocol.cpp       elle yazilmis ad zinciri → tek sozluk
src/interfaces/ui/ViewModelBuilder.cpp  OLED etiketleri
docs/HARDWARE.md                pin plani, I2C, malzeme listesi
```

## Yan bulgu — düzeltildi

`WsProtocol.cpp` komut hedefini elle yazılmış bir `strcmp` zinciriyle
çözüyordu; `StateJson.h` ise "kablo üzerindeki sözlük TEK YERDE" diyordu. İki
doğruluk kaynağı vardı ve yeni aktüatör eklenince biri güncellenip diğeri
unutulsaydı hata ancak sahada "buton hiçbir şey yapmıyor" olarak görünürdü.
Zincir `actuatorIdFromName()` çağrısıyla değiştirildi.

## Definition of Done

- [x] `pio run` temiz (`-Wall -Wextra`, 0 uyarı)
- [x] Pin çakışmaları derleme zamanında zorlanıyor
- [x] Röle tablosu ↔ enum hizası `static_assert` ile kilitli
- [x] Isıtıcı seviye kilidi `static_assert` ile kilitli
- [ ] **Donanımda doğrulanmadı** — üç yeni rölenin polaritesi, I2C adresleri ve
      hat yükü ölçülmedi (bkz. `docs/HARDWARE.md §5`)
