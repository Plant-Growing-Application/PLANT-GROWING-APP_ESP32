# Donanım — Pin Planı ve Flash Bölümleme

> Kaynak: TASK-002 · Derleme zamanı zorlaması: `src/core/BoardPins.h`
> Kapatılan konular: ISSUE-001, ISSUE-002, ISSUE-006

## 1. Kart

| Konu | Değer |
|---|---|
| Kart | `esp32dev` — ESP32-WROOM-32, 240 MHz, 320 KB RAM, 4 MB flash |
| Framework | Arduino (espressif32 @ 6.12.0) |
| Dosya sistemi | LittleFS |
| Seri port | 115200 baud, `esp32_exception_decoder` filtresi etkin |

## 2. ESP32 Pin Kısıtları (pazarlıksız)

Bu kısıtlar donanımdan gelir; tasarım tercihi değildir.

| Kısıt | Pinler | Sonuç |
|---|---|---|
| **ADC1 kanalları** | 32, 33, 34, 35, 36, 39 | Wi-Fi aktifken **ADC2 kullanılamaz** → tüm analog sensörler ADC1'de olmak zorunda |
| **Giriş-only, pull-up YOK** | 34, 35, 36, 39 | Çıkış sürülemez; anahtar/darbe sensörü **harici** pull-up ister |
| **Strapping** | 0, 2, 5, 12, 15 | Boot anında seviyesi okunur → röle gibi çıkışlar için uygun değil |
| **Flash'a ayrılı** | 6 – 11 | Kullanılamaz |

`BoardPins.h` bu kuralları `static_assert` ile **derleme zamanında zorlar**.
Yanlış sınıfa taşınan bir pin projeyi derletmez — hata sahada değil derleyicide çıkar.

## 3. Pin Planı

### 3.1 I2C — OLED

| İşlev | Pin | Not |
|---|---|---|
| SDA | 21 | değişmedi |
| SCL | 22 | değişmedi |

### 3.2 Analog sensörler (hepsi ADC1)

| İşlev | Pin | ADC1 kanalı | Durum |
|---|---|---|---|
| Su sıcaklığı (NTC) | 35 | CH7 | değişmedi |
| pH | **34** | CH6 | **yeni** |
| EC / besin | **36** | CH0 (VP) | **yeni** |
| Analog yedek | 39 | CH3 (VN) | ayrıldı — ileride analog seviye veya 4. sensör |

### 3.3 Dijital girişler

| İşlev | Eski | Yeni | Gerekçe |
|---|---|---|---|
| Akış sensörü (PCNT) | 34 | **4** | GPIO 34'te dahili pull-up **yok** — eski `INPUT_PULLUP` ayarı etkisizdi (ISSUE-002) |
| Su seviyesi — LOW | — | **13** | yeni şamandıra (ISSUE-000) |
| Su seviyesi — CRITICAL | — | **14** | yeni şamandıra (ISSUE-000) |
| Encoder A | 33 | **18** | ADC1_CH5 boşaltıldı (ISSUE-001) |
| Encoder B | 32 | **19** | ADC1_CH4 boşaltıldı (ISSUE-001) |
| Encoder push | 25 | 25 | değişmedi |
| Geri butonu | 27 | 27 | değişmedi |
| ~~Confirm butonu~~ | 26 | **kaldırıldı** | eski kodda `pinMode` yapılıp **hiç okunmuyordu** (ARCHITECTURE P7) |

### 3.4 Dijital çıkışlar

| İşlev | Pin | Not |
|---|---|---|
| Röle 1 — su pompası | 16 | değişmedi; strapping değil |
| Röle 2 — hava pompası | 17 | değişmedi; strapping değil |
| **Röle 3 — büyütme ışığı** | **19** | TASK-066 |
| **Röle 4 — ısıtıcı** | **26** | TASK-066 |
| **Röle 5 — besin dozaj pompası** | **18** | TASK-066 |
| Durum LED'i | 23 | değişmedi |

> **ESP32-WROVER uyarısı:** GPIO 16/17 PSRAM'li modüllerde kullanılamaz.
> Bu proje WROOM-32 (PSRAM'siz) varsayar. Modül değişirse röleler taşınmalıdır.

Beş rölenin de pini `BoardPins.h` içinde `static_assert` ile denetlenir:
strapping/giriş-only olamaz, birbirleriyle ve I2C / encoder / sensör
pinleriyle çakışamaz. Eşlenmemiş mantıksal aktüatör **kalmadı** — arayüzde
açılıp hiçbir şey yapmayan düğme bırakmamak için (ARCHITECTURE P7).

### 3.5 I2C ortam sensörleri — TASK-066

Üç cihaz **tek hattı** paylaşır (GPIO 21/22); adresler çakışmaz ve **yeni pin
gerekmez**. Bu, ADC1 bütçesi tükendiği için (ISSUE-001) zorunlu bir tercihti:
analog bir ortam sensörü fiziksel olarak takılamazdı.

| Cihaz | Adres | Ölçtüğü | Sürücü |
|---|---|---|---|
| SSD1306 | 0x3C | (ekran) | `hal/OledPanel` |
| **AHT20** | **0x38** | ortam sıcaklığı + bağıl nem | `hal/Aht20` |
| **BH1750** | **0x23** | aydınlık (lüks) | `hal/Bh1750` |

Hattın kurulumu `hal/I2cBus` içindedir; `Wire.begin()` çağrısı projede
**yalnızca orada** bulunur. Üç modül de kendi `begin()`'inde onu çağırır ve
hangisinin önce başladığı önemsizdir.

**AHT20 iki fazlı okunur** (tetikle → sonraki turda oku): ölçüm ~80 ms sürer ve
`io_sense` task'ının 250 ms'lik periyodunda beklemek güvenlik sensörlerinin
örneklemesini geciktirirdi (ARCHITECTURE P3). Her okumada CRC8 doğrulanır —
bozuk bir aktarım 22 °C yerine 120 °C okutup doğrudan ısıtıcı eşiğine
girebilirdi.

> **Not:** `SensorId::HUMIDITY` TASK-006'dan beri tanımlıydı, arayüzde kartı
> vardı ve geçmiş verisinde yeri ayrılmıştı ama onu okuyan **hiçbir kod
> yoktu**. AHT20 bu hayalet sensörü gerçek yapar.

### 3.6 Boşta kalan pinler

**Kalmadı.** `18`, `19`, `26` TASK-066 ile rölelere verildi; `32`/`33` encoder'da
(ISSUE-001), `5` strapping. Altıncı bir röle (örneğin pH dozajı) ya bir GPIO
genişletici (PCF8574) ya da encoder'ın taşınmasını gerektirir.

## 4. Gereken Kablolama Değişikliği

Mevcut donanımda **yalnızca 3 kablo** taşınır:

```text
  Encoder A     GPIO 33  →  GPIO 18
  Encoder B     GPIO 32  →  GPIO 19
  Akış sensörü  GPIO 34  →  GPIO 4
```

Yeni eklenecekler: pH (34), EC (36), 2 adet şamandıra (13, 14).

### 4.1 TASK-066 ile eklenen donanım

| Parça | Bağlantı | Tahmini rol |
|---|---|---|
| AHT20 sıcaklık/nem modülü | I2C — SDA 21, SCL 22, 3V3, GND | ortam ölçümü |
| BH1750 ışık modülü | I2C — SDA 21, SCL 22, 3V3, GND | ışığın gerçekten yandığını doğrulama |
| 3'lü (veya 5'li) röle modülü | IN3→19, IN4→26, IN5→18 | ışık, ısıtıcı, dozaj |
| Büyütme ışığı armatürü | röle 3 üzerinden şebeke | fotoperiyot |
| Daldırma ısıtıcısı + termostat | röle 4 üzerinden şebeke | çözelti sıcaklığı |
| Peristaltik dozaj pompası | röle 5 | besin dozajı |

> **ISITICI GÜVENLİĞİ.** Daldırma ısıtıcısı susuz çalışırsa yanar ve hazneyi
> eritir. Bu yüzden yazılımda ısıtıcı, **su pompasıyla aynı seviye
> kilitlerine** bağlanmıştır (`SafetyState.h: masksFor`): seviye yetersizse
> veya seviye sensörü okunamıyorsa ısıtıcı çalışamaz. Buna ek olarak
> **kendi termostatı olan** bir ısıtıcı seçilmesi önerilir — yazılım kilidi
> tek savunma hattı olmamalıdır.

> **DOZAJ POMPASI.** Varsayılan `cooldownMs` 10 dakikadır ve bu bir konfor
> ayarı değil **aşırı gübreleme korumasıdır**: dozajdan sonra EC'nin yükselmesi
> haznenin karışmasını gerektirir; bekleme olmasa kural "EC hâlâ düşük" görüp
> arka arkaya dozaj yapar ve çözeltiyi bitkiyi yakacak seviyeye çıkarırdı.
> `maxRunMs` üst sınırı da 5 dakikayla sınırlıdır (`ACTUATOR_MAX_RUN_DOSING`).

## 5. Doğrulanması Gereken Donanım Maddeleri

Bunlar **ölçümle** kapatılacaktır; tahmin kabul edilmez.

- [ ] **Röle modülü aktif seviyesi** (ISSUE-003) — aktif-düşük modülde boot anında
      pompa çalışabilir. TASK-017'de osiloskop/multimetre ile ölçülecek.
- [ ] **Su seviyesi topolojisi** (ISSUE-000) — iki dijital şamandıra öneriliyor.
      Kablolama, **kopuk kablo "su yok" okunacak** şekilde yapılmalıdır
      (normalde kapalı kontak) — böylece kablo kopması fail-safe tarafa düşer.
- [ ] **Su sıcaklığı sensör tipi** — NTC mi DS18B20 mı (TASK-024). DS18B20
      seçilirse GPIO 35 bir ADC1 kanalı olarak serbest kalır.
- [ ] **Akış sensörü modeli** — YF-S401 varsayımı ve darbe/litre katsayısı (TASK-025).
- [ ] **Üç yeni rölenin aktif seviyesi** (TASK-066) — 19/26/18. `RELAY_ACTIVE_LOW`
      tüm röleler için TEK bir derleme zamanı sabitidir; farklı polaritede bir
      ikinci modül kullanılırsa bu varsayım kırılır.
- [ ] **AHT20 / BH1750 I2C adresleri** — modüllerin varsayılanı 0x38 ve 0x23'tür,
      ama BH1750'nin ADDR pini VCC'ye çekiliyse adres 0x5C olur. `i2cdetect`
      benzeri bir taramayla teyit edilmeli.
- [ ] **I2C hat yükü** — üç cihaz + uzun kablo 400 kHz'de pull-up direnci
      gerektirebilir (tipik 4.7 kΩ). OLED hata sayacı (`i2cErrorCount`) artıyorsa
      ilk bakılacak yer burasıdır.

## 6. Flash Bölümleme

Dosya: `partitions.csv` · Karar: ISSUE-006 kapandı

| Bölüm | Tip | Offset | Boyut | Amaç |
|---|---|---|---|---|
| `nvs` | data/nvs | 0x9000 | 20 KB | konfigürasyon + sırlar |
| `otadata` | data/ota | 0xE000 | 8 KB | OTA durum kaydı |
| `app0` | app/ota_0 | 0x10000 | **1.5 MB** | uygulama |
| `app1` | app/ota_1 | 0x190000 | **1.5 MB** | OTA hedefi |
| `littlefs` | data/spiffs | 0x310000 | **896 KB** | web varlıkları + geçmiş halka dosyası |
| `coredump` | data/coredump | 0x3F0000 | 64 KB | çökme dökümü |

### Gerekçe

- **OTA etkinleştirildi.** SQLite kaldırılınca (ARCHITECTURE §15.2) binary belirgin
  şekilde küçüldü ve çift uygulama bölümü 4 MB'a sığdı. Sera cihazında fiziksel
  erişim gerektirmeyen güncelleme ciddi kazançtır.
- **1.5 MB uygulama payı:** eski binary (SQLite dahil) 961 152 bayttı. İskelet
  build 266 973 bayt. Tam sistem için rahat pay var.
- **896 KB LittleFS:** gzip'li web varlıkları ~100 KB mertebesinde beklenir;
  kalan alan geçmiş veri halka dosyasına (TASK-058) ayrılır.
- **coredump bölümü:** `REQUIREMENTS.md` §9'da eksik işaretlenen "ESP32 crash
  recovery" için altyapı. Çökme sonrası neden analiz edilebilir olur.

## 7. Kütüphane Sürüm Sabitlemesi

`platformio.ini` içinde git URL'i **kullanılmaz**. Gerekçe (ISSUE-007):
`me-no-dev/ESPAsyncWebServer.git` deposu devredildi, URL sessizce farklı bir
projeye (ESP32Async 3.6.0) yönlendi ve derlemeyi kırdı. Kütüphaneler artık
registry üzerinden ve sürüme sabitlenerek çekilir.

`lib_ldf_mode = deep+` gereklidir: ESPAsyncWebServer 3.x, framework'ün senkron
`WebServer` kütüphanesini bağımlılık grafiğine sokar ve `chain` modunda include
yolu yayılmadığı için derleme kırılır.

---

## 8. ZORUNLU Donanım Gereksinimi — Röle Boot Koruması (ISSUE-003)

> **Bu madde yazılımla karşılanamaz. Donanım değişikliği gerektirir.**

### Problem

ESP32 GPIO'ları reset'ten sonra ve bootloader boyunca (**yüzlerce ms**) yüksek
empedanstadır. Yazılım en erken `setup()` başında müdahale edebilir.

Bu pencerede röle giriş hattı boşta kalır. **Aktif-düşük** bir röle modülünde
boşta/LOW giriş, rölenin çekili olması — yani **pompanın çalışması** demektir.
Hazne boşsa bu, her açılışta birkaç yüz milisaniye kuru çalışmadır.

### Gerekli çözüm

| Röle modülü tipi | Gereken |
|---|---|
| Aktif-düşük (yaygın, optokuplörlü) | Röle giriş hattına **harici pull-up** (10 kΩ, VCC'ye) |
| Aktif-yüksek | Röle giriş hattına **harici pull-down** (10 kΩ, GND'ye) |

Direnç, ESP32 pininin sürebileceği kadar zayıf ama hattı tutacak kadar güçlü
olmalıdır; 10 kΩ tipik değerdir.

### Yazılım tarafında yapılanlar

- Boot **Aşama 1**'de (her şeyden önce) röleler güvenli seviyeye alınır
- Glitch'siz sıra: önce çıkış yazmacına güvenli seviye, sonra `pinMode(OUTPUT)`
- Polarite **derleme zamanı sabiti** (`BoardPins.h: RELAY_ACTIVE_LOW`) —
  config'te olsaydı Aşama 1'de bilinmezdi

### Doğrulanması gerekenler — HENÜZ YAPILMADI

- [ ] Röle modülünün aktif seviyesi multimetre ile **ölçülecek**
- [ ] `BoardPins.h: RELAY_ACTIVE_LOW` ölçüme göre düzeltilecek
      (şu an `true` — **doğrulanmamış varsayım**)
- [ ] Güç verme anından itibaren röle çıkışı osiloskopla izlenecek;
      hiçbir anda aktif olmamalı
- [ ] Aynı ölçüm yazılımsal reset ve WDT reset için tekrarlanacak

---

## 9. ZORUNLU Kablolama — Su Seviyesi Şamandıraları (ISSUE-000)

> Güvenlik zincirinin temeli. Yanlış kablolama korumayı **sessizce** devre dışı bırakır.

### Topoloji: iki bağımsız şamandıra

| İşlev | Pin | Konum |
|---|---|---|
| `LEVEL_FLOAT_LOW` | GPIO 13 | üstte — düşük seviye eşiği |
| `LEVEL_FLOAT_CRIT` | GPIO 14 | altta — kritik seviye eşiği |

Tek sensör yerine iki bağımsız sensör kullanılır: **tek nokta hatası** ortadan
kalkar ve aralarında tutarlılık kontrolü yapılabilir.

### Kontak yönü — KRİTİK

Şamandıra kontağı **normalde kapalı (NC)** bağlanmalı ve dahili pull-up'lı
girişe çekilmelidir:

```text
   Su VAR      → kontak KAPALI → giriş LOW
   Su YOK      → kontak AÇIK   → giriş HIGH
   KABLO KOPUK →                giriş HIGH → "su yok" okunur  ✓ FAIL-SAFE
```

**Ters bağlanırsa** kopuk kablo "su var" okunur ve koruma sessizce ölür.
Bu, sistemin en tehlikeli sessiz arızasıdır.

### Tutarlılık kuralı

Üst şamandıra suda yüzerken alttakinin kuru olması **fiziksel olarak
imkânsızdır**. Bu kombinasyon görülürse yazılım her iki sensörü de arızalı
sayar ve pompayı kilitler.

### Doğrulanması gerekenler

- [ ] Şamandıra kontak yönü (NC) ölçümle doğrulanacak
- [ ] Kablo kopukken "su yok" okunduğu test edilecek
- [ ] İki şamandıranın fiziksel yükseklik farkı, pompa emiş seviyesinin
      üstünde olacak şekilde ayarlanacak
- [ ] ISSUE-000 kullanıcı onayı: iki şamandıra topolojisi kabul edildi mi
