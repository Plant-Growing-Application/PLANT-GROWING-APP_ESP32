# Issue Log — Scope Creep Kayıtları

> Bir task sırasında kapsam dışı bir problem fark edilirse **o problem çözülmez**;
> buraya kaydedilir ve faz sonunda triyaj edilir.
>
> Amaç: "Wi-Fi bağlantısı" diye başlayan bir task'ın 3 saat sonra tüm sistemi
> değiştirmeye dönüşmesini engellemek.

## Format

```text
### ISSUE-XXX — Kısa başlık

Found during:  TASK-XXX
Severity:      P0 | P1 | P2 | P3
Status:        OPEN | TRIAGED | SCHEDULED | RESOLVED | WONTFIX

Description:
  Problemin ne olduğu.

Impact:
  Çözülmezse ne olur.

Recommended:
  Yeni TASK-XXX önerisi veya mevcut bir task'a ekleme.
```

---

## Açık Kayıtlar

### ISSUE-007 — ESPAsyncWebServer sürüm kayması derlemeyi kırıyor

Found during: TASK-001 · Severity: **P0** · Status: **RESOLVED** (TASK-002)

Description:
  `platformio.ini` içindeki `https://github.com/me-no-dev/ESPAsyncWebServer.git`
  URL'i artık **ESP32Async/ESPAsyncWebServer 3.6.0**'a yönleniyor (depo devredilmiş).
  Bu sürüm, framework'ün senkron `WebServer` kütüphanesini bağımlılık grafiğine
  sokuyor; LDF `chain` modunda `WebServer` derlenirken `WiFiServer.h` include
  yolunu bulamıyor ve derleme kırılıyor.

  Platform espressif32 @ 6.12.0 / framework 3.20017. Son başarılı derleme
  12 Haziran tarihli — o tarihten bu yana sürüm kayması olmuş.

Impact:
  **Mevcut kod da şu an derlenmiyor.** Bu, TASK-001'in yaptığı değişikliklerden
  kaynaklanmıyor; önceden var olan bir durumdur. TASK-001'in "proje derleniyor"
  kabul kriteri bu düzeltilene kadar doğrulanamaz.

Recommended:
  TASK-002 kapsamında çözülmeli (lib_deps sadeleştirmesi zaten TASK-002 scope'unda):
  - Kütüphaneler sürüme sabitlensin (git URL yerine registry + sabit sürüm)
  - Kullanılmayan kütüphaneler (Adafruit SH110X) kaldırılsın
  - İskelet aşamasında web kütüphaneleri hiç gerekmiyor; TASK-041'de eklenebilir
  - Gerekirse `lib_ldf_mode` ayarlansın



---

## Başlangıçtan Devralınan Bilinen Konular

Bunlar `REQUIREMENTS.md` §12 ve `ARCHITECTURE.md` §20'den gelir; implementation başlamadan
önce netleşmesi gereken donanım kararlarıdır. Task'lar bunlara referans verir.

### ISSUE-000 — Su seviyesi sensörü tipi belirsiz

Found during: mimari tasarım · Severity: **P0** · Status: OPEN
Description: Güvenlik zincirinin temeli olan su seviyesi sensörünün tipi kesin değil.
Impact: TASK-026 ve TASK-030 tasarımı doğrudan etkilenir.
Recommended: İki dijital şamandıra (düşük + kritik-düşük). TASK-026 STEP 1'de kapatılmalı.

### ISSUE-001 — ADC1 pin çakışması (encoder GPIO 32/33)

Found during: mimari tasarım · Severity: **P0** · Status: **RESOLVED** (TASK-002)
Description: pH, EC ve analog seviye ADC1 (GPIO 32–39) gerektirir; Wi-Fi aktifken ADC2
kullanılamaz. Encoder şu anda ADC1 kanalları olan GPIO 32/33'ü işgal ediyor.
Impact: Analog sensörler için yeterli kanal kalmaz.
Recommended: Encoder ADC olmayan pinlere taşınsın. TASK-002 pin planında kapatılmalı.

### ISSUE-002 — GPIO 34–39'da dahili pull-up yok

Found during: mevcut kod analizi · Severity: P1 · Status: **RESOLVED** (TASK-002)
Description: Akış sensörü GPIO 34'te `INPUT_PULLUP` ile kullanılıyor; bu ayar donanımsal
olarak etkisizdir (giriş-only pinlerde dahili pull-up bulunmaz).
Impact: Darbe sayımı güvenilmez olabilir.
Recommended: Harici pull-up veya pin değişikliği. TASK-025'te doğrulanmalı.

### ISSUE-003 — Röle modülü aktif seviyesi doğrulanmadı

Found during: mimari tasarım · Severity: **P0** · Status: OPEN
Description: Aktif-düşük röle modüllerinde boot anında pompa çalışabilir.
Impact: Boot sırasında kuru çalışma riski.
Recommended: TASK-017 ilk iş olarak ölçümle doğrulasın.

### ISSUE-004 — RELAY2'nin gerçek yükü bilinmiyor

Found during: mevcut kod analizi · Severity: P2 · Status: OPEN
Description: Web arayüzünde "Oksijen Sensörü" yazıyor ama bir röle çıkışı sürülüyor.
Impact: Aktüatör isimlendirmesi ve otomasyon kuralları etkilenir.
Recommended: Donanım şeması ile doğrulanmalı. Mantıksal/fiziksel ayrım sayesinde yalnızca
yapılandırma etkilenir. TASK-028'de kapatılmalı.

### ISSUE-005 — Donanımsal RTC gerekli mi

Found during: mimari tasarım · Severity: P2 · Status: OPEN
Description: Zamanlı sulama ağ olmadan da çalışacaksa DS3231 gerekir.
Impact: TASK-040 ve TASK-056 kapsamı değişir.
Recommended: TASK-040 STEP 1'de karara bağlanmalı.

### ISSUE-006 — Partition şeması ve OTA kararı

Found during: mimari tasarım · Severity: P2 · Status: **RESOLVED** (TASK-002)
Description: SQLite kaldırılınca binary küçülür; 4 MB flash'ta çift uygulama bölümü mümkün.
Impact: TASK-002 partition tablosunu belirler; sonradan değişmesi maliyetlidir.
Recommended: TASK-002'de karara bağlanmalı.

---

## Çözüm Kaydı

| Issue | Durum | Nerede çözüldü | Nasıl |
|---|---|---|---|
| ISSUE-000 | OPEN | TASK-026 | Su seviyesi topolojisi — pin planında iki şamandıra için yer ayrıldı (13, 14), donanım kararı bekliyor |
| ISSUE-001 | **KABUL EDİLDİ** | TASK-002 → geri alındı | Encoder 33/32'de KALDI; ADC1_CH4/CH5 işgal — analog genişleme payı YOK (bkz. aşağı) |
| ISSUE-002 | **RESOLVED** | TASK-002 | Akış sensörü 34 → 4; `static_assert` ile derleme zamanında zorlanıyor |
| ISSUE-003 | OPEN | TASK-017 | Röle aktif seviyesi — ölçüm gerektirir |
| ISSUE-004 | OPEN | TASK-028 | RELAY2 gerçek yükü — mantıksal/fiziksel ayrım hazır |
| ISSUE-005 | OPEN | TASK-040 | Donanımsal RTC kararı |
| ISSUE-006 | **RESOLVED** | TASK-002 | Özel partition tablosu: 2×1.5 MB app (OTA) + 896 KB LittleFS + 64 KB coredump |
| ISSUE-007 | **RESOLVED** | TASK-002 | Registry + sabit sürüm + `lib_ldf_mode = deep+` |

### ISSUE-008 — Kodlama standardı için otomatik denetim yok

Found during: TASK-003 · Severity: P3 · Status: OPEN

Description:
  `docs/CODING_STANDARDS.md` her yasak desen için aranabilir bir iz veriyor, ancak
  denetim şu an elle (grep) yapılıyor. `clang-format` ve `clang-tidy` ortamda kurulu
  değil.

Impact:
  Kural ihlali yalnızca inceleme yapan kişi tarama komutlarını çalıştırırsa yakalanır.
  65 task boyunca insan hatası riski var.

Recommended:
  Ortama clang-format/clang-tidy kurulduğunda:
  - `.clang-format` eklensin (TASK-003 Karar 3'te ertelendi)
  - `CODING_STANDARDS.md` §2'deki grep desenleri bir betiğe dönüştürülsün
  - TASK-062 veya TASK-065 kapsamında değerlendirilebilir

### ISSUE-009 — Enum isimleri Arduino makrolarıyla çakışabiliyor

Found during: TASK-005 · Severity: P2 · Status: OPEN

Description:
  `ResetReason::EXTERNAL` derleme hatası verdi: `Arduino.h` içinde
  `#define EXTERNAL 0` bulunuyor. Preprocessor makroları `enum class`
  kapsamlamasına saygı duymaz — kısa, büyük harfli enum üye isimleri
  framework makrolarıyla sessizce çakışabilir.

  Framework taraması sonucu (cores/esp32 + sdk/esp32/include):
  çakışan makrolar `EXTERNAL` ve `DEFAULT`. Projede kullanılan diğer
  isimler (INFO, WARNING, ERROR, CRITICAL, SENSOR, NET, TIME, ...) temiz.

Impact:
  Yeni enum eklendiğinde aynı tuzağa düşülebilir. Hata mesajı yanıltıcı
  ("expected unqualified-id before numeric constant") ve çakışmanın nedenini
  göstermiyor — teşhis zaman kaybettiriyor.

  **TEKRAR (TASK-010):** `BootStage::DISPLAY` ayni tuzaga dusdu —
  `Arduino.h:51` icinde `#define DISPLAY 0x1` var. `DISPLAY_HW` olarak
  yeniden adlandirildi. Bu, kuralin gercekten gerekli oldugunun ikinci
  kanitidir; artik her yeni enum icin tarama ZORUNLU sayilmalidir.

Recommended:
  `docs/CODING_STANDARDS.md` §8'e (adlandırma) bir kural eklensin:
  "Kısa, tek kelimelik büyük harfli enum üye isimlerinden kaçının; framework
  makrolarıyla çakışabilir. Şüphede kalınırsa framework header'larında
  `#define <AD>` araması yapın."
  TASK-003'ün Files listesi dışında olduğu için bu task'ta yapılmadı.

### ISSUE-010 — Yayınlanan state tiplerinin sahipliği (karar kaydı)

Found during: TASK-006 · Severity: P1 · Status: **RESOLVED** (karar verildi)

Description:
  TASK-022 (`SensorReading`), TASK-028 (`Actuator`) ve TASK-035 (`NetworkState`)
  kendi task dosyalarında tip tanımlamayı planlıyor. Ancak bu kavramların
  tamamı `SystemState` içinde yayınlanır ve `SystemState` POD olmak zorundadır.
  İki yerde tanımlanırsa dönüşüm kodu, tutarsızlık ve ölü kod doğar (P7).

Karar (TASK-006'da sabitlendi):
  - `SystemState.h`  → YAYINLANAN state tipleri (değer, kalite, durum, kimlik)
  - Sonraki task'lar → ÇALIŞMA ve KONFİGÜRASYON tipleri; yayınlanan tipi
                       yeniden tanımlamaz, include eder.

  TASK-022 → SensorDescriptor, SensorRegistry, ISensor
  TASK-028 → ActuatorConstraints, ActuatorCommandResult
  TASK-035 → NetworkFsm iç durumu ve geçiş tablosu

Impact:
  İlgili task'lar bu kaydı okumadan tip tanımlamamalıdır.

### ISSUE-011 — ARCHITECTURE §6.5 "task sınıfına göre TWDT timeout" platformda mümkün değil

Found during: TASK-009 · Severity: P2 · Status: OPEN (doküman düzeltmesi)

Description:
  `ARCHITECTURE.md` §6.5 şöyle diyor:
  "Timeout: Kontrol task'ları için kısa (~5 s), ağ ve storage için uzun (~15 s).
   Tek bir global değer yerine görev sınıfına göre."

  Platform gerçeği: ESP-IDF 4.4 TWDT **tek global zamanlayıcıdır**.
  `esp_task_wdt_init(timeout, panic)` tüm aboneler için aynı süreyi belirler;
  task başına farklı timeout desteklenmez.

Impact:
  Mimari metni, uygulanamayan bir yetenek vaat ediyor. Okuyan bir geliştirici
  bunu aramaya çalışır.

Çözüm (TASK-009'da uygulandı):
  İki katmanlı koruma — ARCHITECTURE §6.5'in kendi (b)+(c) önerisi:
    - Donanım katı : tek global TWDT (8 sn), tam kilitlenmeye karşı
    - Uygulama katı: task başına yumuşak son tarih (heartbeat, TASK-012),
                     yavaşlamaya karşı, çok daha sıkı

Recommended:
  `ARCHITECTURE.md` §6.5'teki "Timeout" satırı bu gerçeği yansıtacak şekilde
  düzeltilsin. Mimari dokümanı bu task'ın Files listesinde olmadığı için
  değiştirilmedi.

### ISSUE-012 — `Millis operator+(Millis, Duration)` güvenli karşılaştırma eşi olmadan tuzak

Found during: TASK-012 · Severity: P1 · Status: OPEN

Description:
  `core/Time.h` (TASK-004) `Millis + Duration` toplamasını sağlıyor ve gelecekteki
  bir zaman damgası üretiyor. Ancak o damgayı **güvenle karşılaştıracak** bir
  fonksiyon YOK. `Millis`'te bilinçli olarak `<` / `>` tanımlı değil, geriye
  yalnızca `elapsed()` / `hasElapsed()` kalıyor — ikisi de GEÇMİŞ bir damga bekler.

  TASK-012 implementasyonunda tam bu tuzağa düşüldü:

      g_restartAt = now + delay;                          // gelecekteki damga
      if (hasElapsed(now, g_restartAt, Duration{0})) ...  // HATALI

  `now < g_restartAt` iken `now.v - restartAt.v` unsigned olarak sarıyor,
  devasa bir değer üretiyor ve koşul HEMEN doğru oluyor. Sonuç: gecikme hiç
  çalışmaz, cihaz anında reset olur — HTTP yanıtı gitmez, `store` kuyruğu
  boşalmaz. Tam olarak gecikmenin önlemeyi amaçladığı veri kaybı.

  Hata TASK-012'de düzeltildi (talep anını saklayıp geçen süreyi süreyle
  karşılaştıran desene geçildi). Ancak `operator+` yerinde duruyor ve artık
  hiçbir yerde KULLANILMIYOR.

Impact:
  Kullanılmayan bir operatör (P7) **ve** sonraki geliştiriciler için aktif bir
  tuzak. Aynı hata sessizce tekrarlanabilir.

Recommended:
  İki seçenekten biri, TASK-004'ün dosyasında (`core/Time.h`):
    (a) `operator+`'ı KALDIR — deadline deseni yerine "talep anı + süre"
        deseni tek yol olsun (bu batch'te fiilen kullanılan desen)
    (b) Güvenli eşini EKLE:  `constexpr bool isPast(Millis now, Millis deadline)`
        → `static_cast<int32_t>(now.v - deadline.v) >= 0` (işaretli, taşma güvenli)
        ve `operator+` yalnızca bununla birlikte kullanılsın

  Öneri: (a). Bu batch'teki tüm kullanımlar "talep anı + süre" desenine uyuyor;
  ikinci bir zaman deseni sunmak tuzak yüzeyini gereksiz büyütüyor.

  `core/Time.h` bu task'ın Files listesinde olmadığı için değiştirilmedi.

### ISSUE-013 — Planda "boot wiring" task'ı YOK (gerçek boşluk)

Found during: TASK-013 öncesi doğrulama · Severity: **P0** · Status: OPEN

Description:
  PHASE 2 (TASK-009…012) boot yürütücüsünü, task çerçevesini ve mod makinesini
  ÜRETTİ. Ancak bunları birleştiren — yani:

    · boot aşama tablosunu kuran
    · `boot::run()` çağıran
    · `tasks::createAll()` çağıran
    · beş task'ın giriş fonksiyonlarını yazan
    · `supervisor::setSafeStateHandler()` bağlayan
    · `main.cpp`'yi gerçek sisteme dönüştüren

  bir task `IMPLEMENTATION_PLAN.md`'de **bulunmuyor**.

  TASK-013 "NvsStore & Secret Store"dır. Planda hiçbir yerde "boot wiring"
  geçmiyor. En erken karşılığı TASK-060 (System Integration Bring-Up) — yani
  PHASE 14. Bu, sistemin 60 task boyunca **hiç boot etmemesi** demektir.

Impact:
  · Hiçbir çalışma zamanı doğrulaması yapılamaz (heartbeat, WDT, periyot,
    stack watermark, boot arıza senaryoları) — hepsi PHASE 14'e yığılır
  · PHASE 2 ve PHASE 3/4 çıktıları donanımda hiç denenmemiş olarak birikir
  · Bir tasarım hatası ancak 60 task sonra ortaya çıkar

Ek not — hatalı çapraz atıf:
  TASK-010, TASK-011 ve TASK-012 kayıtlarında "TASK-013 (boot wiring)" diye
  atıf yapıldı. Bu atıflar YANLIŞ ve düzeltildi (bu issue'ya yönlendiriliyor).

Recommended:
  PHASE 4'ten sonra, PHASE 5'ten önce yeni bir task açılsın:
    **TASK-022a — Boot Wiring & First Bring-Up**
  Kapsam: aşama tablosu, task giriş iskeletleri, `main.cpp` birleştirme,
  ilk donanım boot doğrulaması.
  Gerekçe: HAL (PHASE 4) tamamlandığında aşamaların çoğu gerçek içerik
  kazanmış olur; sistem o noktada ilk kez anlamlı şekilde boot edebilir.

  Karar kullanıcıya aittir; plan dosyası bu batch'in kapsamında değil.

### ISSUE-014 — Akış sensörü darbe/litre katsayısı doğrulanmadı

Found during: TASK-025 · Severity: P1 · Status: OPEN

Description:
  `flow::PULSES_PER_LITER = 450.0f` değeri **eski kodun yorumundan** alındı
  (`// YF-S401` / `pulses * 100 / 450`) ve hiç doğrulanmadı. Sahadaki sensörün
  gerçekten YF-S401 olduğu da teyit edilmedi (REQUIREMENTS §12).

Impact:
  Katsayı yanlışsa debi ölçümü orantılı olarak yanlış olur. Bu yalnızca bir
  gösterim hatası değildir: **akış doğrulaması** (TASK-031) bu değere göre
  kuru çalışma kararı verir. Katsayı 2 kat yanlışsa kuru çalışma eşiği de
  2 kat kayar ve koruma ya erken ya geç tetiklenir.

Recommended:
  Bilinen hacimde su akıtılıp darbe sayılarak ölçülsün; `PULSES_PER_LITER`
  düzeltilsin. Saha farkları `SensorConfig.scale` ile trim edilebilir.
  TASK-025 Test Plan'inde madde olarak duruyor.

### ISSUE-015 — NTC bölücü topolojisi ve parametreleri doğrulanmadı

Found during: TASK-024 · Severity: P2 · Status: OPEN

Description:
  `WaterTempSensor` şu varsayımlarla yazıldı:
    · bölücü topolojisi: NTC → VCC, R_SERIES → GND
    · R_SERIES = 10 kΩ, R_NOMINAL = 10 kΩ, BETA = 3950
  Hiçbiri ölçülmedi. Eski kodda da yalnızca BETA (3950) geçiyordu; seri
  direnç ve besleme gerilimi formüle hiç girmemişti.

Impact:
  Topoloji ters bağlıysa sıcaklık **ters yönde** değişir (ısındıkça düşer) —
  bu ilk denemede fark edilir. Parametreler yanlışsa sabit bir kayma olur;
  `SensorConfig.offset`/`scale` ile trim edilebilir ama önce doğru topoloji
  gerekir.

Recommended:
  Devre şeması ile topoloji doğrulansın, direnç değerleri ölçülsün.
  Alternatif: DS18B20'ye geçiş (ARCHITECTURE §20 açık maddesi) — kalibrasyon
  ihtiyacını ortadan kaldırır ve bir ADC1 kanalı serbest bırakır.

---

## ISSUE-016 — `Config` okuma/yazma eşzamanlılığı tanımlı değil

**Bulunduğu yer:** TASK-029 (ActuatorManager), genel
**Öncelik:** P2 · **Durum:** Açık

`ConfigService::get()` canlı bir referans döndürür. Okuyucular `app_core`
(ActuatorManager, SafetyMonitor) ve `io_sense` (kalibrasyon); yazarlar `net`
(web API) ve `store`. Hiçbir senkronizasyon tanımlı değil.

**Gerçek etki:** ESP32'de hizalı 32-bit okuma yırtılmaz, dolayısıyla tek bir
alan hiçbir zaman yarım okunmaz. Ancak bir güncelleme sırasında okunan yapı,
alanlar arasında tutarsız olabilir (örn. yeni `maxRunMs` + eski `minRunMs`).
Etki bir döngüyle sınırlıdır ve güvenlik açığı yaratmaz — `maxRunMs`'in üst
sınırı doğrulama ile garanti altındadır.

**Neden bu task'ta çözülmedi:** Kapsam dışı. Çözümü `StateStore` benzeri bir
snapshot deseni veya çift tamponlamadır ve tüm okuyucuları etkiler.

**Önerilen çözüm:** `ConfigService`'e `snapshot(Config&)` eklenip okuyucuların
döngü başında bir kopya alması. TASK-059 (config kalıcılığı) sırasında
değerlendirilmeli.

---

## ISSUE-017 — Acil durumda havalandırmanın da kesilmesi ikinci zarar üretebilir

**Bulunduğu yer:** TASK-032, `domain/models/SafetyState.h` → `masksFor()`
**Öncelik:** P2 · **Durum:** Açık — operatör kararı gerekiyor

TASK-032 kabul kriteri "mandal aktifken **tüm** aktüatörler kilitli" diyor ve
uygulama buna birebir uyuyor: `ILK_EMERGENCY_LATCHED` her aktüatörü keser.

**Endişe:** Hidroponik bir sistemde hava pompası kök bölgesine oksijen sağlar.
Günlerce duran bir havalandırma kök çürümesine yol açar. Acil durumun nedeni
su pompasıyla ilgiliyken (kuru çalışma, akış doğrulama) havalandırmayı da
kesmek, önlenen zarardan daha büyük bir zarar üretebilir — özellikle operatör
sahada değilse.

**Neden bu task'ta değiştirilmedi:** Kabul kriteri açık ve güvenlik lehine
yazılmış. Spesifikasyona aykırı bir gevşetme, güvenlik tasarımında tek
taraflı karar olur.

**Karar gerektiren soru:** Acil durum nedeni su pompasına özgü olduğunda
(`SAFETY_DRY_RUN`, `SAFETY_FLOW_VERIFY_FAILED`, `SAFETY_LEVEL_*`) hava
pompası çalışmaya devam etmeli mi?

**Not:** `masksFor()` bir `constexpr` tablodur; karar verilirse değişiklik
tek satırlıktır ve `static_assert`lar yeni kuralı derleme zamanında kilitler.

---

## ISSUE-018 — Task giriş noktaları hâlâ hiçbir yerden çağrılmıyor

**Bulunduğu yer:** `src/tasks/` — beş task entry, sıfır çağrı
**Öncelik:** **P0** · **Durum:** Açık — ISSUE-013'ün genişlemiş hâli

PHASE 8–11 sonunda beş task giriş noktası da yazıldı:

```text
appCoreTaskEntry   (TASK-033)
sensorTaskEntry    (TASK-027)
networkTaskEntry   (TASK-035/040)
uiTaskEntry        (TASK-053)
storeTaskEntry     — henuz yok (TASK-059)
```

**Hiçbiri çağrılmıyor.** `tasks::createAll()` bir `TaskDef` tablosu
bekliyor ama o tabloyu kuran kod yok; `src/main.cpp` boş kabuk.

**Sonuç:** Firmware derleniyor (%27.7 flash) ve linker ölü kodu atmıyor
çünkü entry'ler `extern` — ama **sistem hiç boot etmedi**. Bu batch'te
yazılan ağ, web ve arayüz kodunun hiçbiri **bir kez bile çalışmadı**.

**Neden bu batch'te çözülmedi:** Kapsam dışı. Boot wiring, planda **hiçbir
task'ın kapsamında değil** — ISSUE-013'te kaydedilen boşluk hâlâ açık.

**Önerilen çözüm:** IMPLEMENTATION_PLAN'e yeni bir task eklenmeli:
`main.cpp` + boot aşama tablosu + `TaskDef` tablosu. Bu yapılmadan
donanımda tek bir doğrulama yapılamaz ve M4 kapısı açılamaz.

---

## ISSUE-019 — HTTP yetki kontrolü istek başına bir `String` tahsisi yapıyor

**Bulunduğu yer:** `interfaces/web/RequestValidation.cpp:26`
**Öncelik:** P3 · **Durum:** Açık

`req->header("Authorization")` ESPAsyncWebServer API'si gereği `String`
döndürüyor; her yetkili istekte bir heap tahsisi ve serbest bırakma olur.

**Gerçek etki:** Küçük ve kısa ömürlü. Ancak uzun çalışmada heap
parçalanmasına katkı verir ve `interfaces/` katmanındaki tek tahsis noktası
budur (tarama ile doğrulandı).

**Neden çözülmedi:** Kütüphane API'sinin dayattığı bir kısıt; kaçınmak için
başlık ayrıştırmayı elle yapmak gerekir ve bu, kütüphanenin doğru çalışan
bir parçasını yeniden yazmak olur.

**Önerilen çözüm:** WS'te zaten kullanılan sorgu-parametresi yolunu HTTP'de
de birincil hâle getirmek, ya da uzun süreli testte heap parçalanması
ölçülürse elle ayrıştırma. TASK-063 (bellek doğrulama) sırasında ölçülmeli.

---

## ISSUE-020 — JSON belgeleri "önceden boyutlandırılmış" DEĞİL

**Bulunduğu yer:** `interfaces/web/StateJson.cpp` (4 üretici fonksiyon)
**Öncelik:** P2 · **Durum:** Açık

ARCHITECTURE §14.6 kuralı: *"Büyük JSON önceden boyutlandırılmış — heap
parçalanmasını önler."*

**Gerçek durum — kaynağı okuyarak doğrulandı:** ArduinoJson 7'de
`StaticJsonDocument` kaldırılmış (`compatibility.hpp:63`, `DEPRECATED`).
`JsonDocument` varsayılan olarak `DefaultAllocator` ile **yığından** tahsis
eder ve havuzlar hâlinde **büyür** (`JsonDocument.hpp:24`).

Kuralın **yarısı** sağlanıyor:
- ✅ **Çıktı** sabit tampona yazılıyor (`serializeJson(doc, out, outLen)`),
  dinamik `String` yok, çıktı sınırsız büyüyemez
- ❌ **Belgenin kendisi** yığında ve dinamik

**Neden bu turda çözülmedi:** Çözüm `Allocator` arayüzünü uygulayan statik
bir arena. Ancak `writeStateJson()` **iki task'tan birden** çağrılıyor
(`StateApi` → AsyncTCP task, `WsProtocol::tick` → `net` task). Bugün güvenli,
çünkü her çağrının kendi yığın-yerel `JsonDocument`'ı var. **Paylaşılan bir
arena bu güvenliği BOZARDI** — arena ya çağıran başına ya kilitli olmalı.

Yani düzeltme, göründüğünden büyük bir değişiklik ve bu task'ın kapsamı
değil.

**Önerilen çözüm:** TASK-063 (bellek doğrulama) sırasında uzun çalışmada
heap parçalanması ÖLÇÜLMELİ. Ölçüm sorun gösterirse: çağıran başına bir
arena allocator (iki örnek — biri AsyncTCP, biri `net`).

**Bu arada geçerli olan koruma:** çıktı tamponu sabit, `MAX_BODY_BYTES`
4096, WS mesaj sınırı 512 bayt — girdi tarafı sınırsız değil.

---

## ISSUE-021 — Kural düzenleme API'si ve arayüzü yok

**Bulunduğu yer:** `interfaces/web/api/ConfigApi.cpp`, `frontend/`
**Öncelik:** P2 · **Durum:** Açık

TASK-054–057 kural motorunu ve veri modelini tamamladı. Ancak kuralları
**oluşturmanın, düzenlemenin veya silmenin hiçbir yolu yok**:

- `GET /api/config` kuralları döndürmüyor
- `PUT /api/config/rules` uç noktası yok
- Arayüzde kural düzenleme ekranı yok

Sonuç: kural kümesi kalıcı olarak boş kalıyor ve otomasyon — mod `AUTO`
yapılsa bile — hiçbir şey yapmıyor.

**Neden bu turda yapılmadı:** Kapsam dışı. TASK-054 "Out of Scope" açıkça
"Kural düzenleme arayüzü (TASK-049)" diyor; TASK-044'ün uç nokta listesinde
de `/api/config/rules` yok. Planda bu iş **hiçbir task'a atanmamış** —
ISSUE-013 ve ISSUE-018 ile aynı türden bir plan boşluğu.

**Önerilen çözüm:** Yeni bir task: `GET/PUT /api/config/rules` +
`writeRulesJson()` + frontend'de kural listesi/düzenleyici. Doğrulama
altyapısı (`validateRules`) zaten hazır ve alan adıyla hata döndürüyor.

**Not:** M4 kapısı açılmadan bu iş anlamlı değil — kural yazılabilse bile
`AUTO` moduna geçilmemeli.

---

## ISSUE-022 — `/api/history` dosya okuması AsyncTCP bağlamında, süresi ölçülmedi

**Bulunduğu yer:** `interfaces/web/api/HistoryApi.cpp`
**Öncelik:** P2 · **Durum:** Açık — ölçüm bekliyor

ARCHITECTURE §14.6: *"Callback içinde dosya taraması / uzun döngü / bekleme
yok — AsyncTCP task'ını bloklar, tüm web donar."*

`GET /api/history` isteği karşılarken LittleFS'ten okuma yapıyor.
Sınırlandırıldı:

- en fazla 240 kayıt = 5 760 bayt
- halka en fazla iki bitişik parçaya bölündüğü için **en fazla 2 dosya açması**

Bu, ilk yazımdaki 240 dosya açmasına göre iki mertebe iyileşme (TASK-058'de
düzeltildi). Ancak **süre donanımda ölçülmedi**; LittleFS'te dosya açma
maliyeti bölüm doluluğuna ve parçalanmaya göre değişir.

**Ölçüm kriteri:** istek başına toplam süre < 20 ms olmalı. Aşarsa eşzamanlı
WebSocket telemetrisi ve diğer HTTP istekleri gecikir.

**Aşarsa çözüm:** `store` task'ı bir sayfayı önceden hazırlar (RAM'de tutar);
HTTP handler yalnızca hazır tampondan serileştirir. Altyapı hazır —
`storage::post()` bir `PREPARE_PAGE` tipi alacak şekilde genişletilir.

---

## ISSUE-023 — `store` task'ı 0 ms periyotla `vTaskDelayUntil` çağırıyordu

**Bulunduğu yer:** `tasks/TaskRunner.cpp` / `tasks/StoreTask.cpp`
**Öncelik:** **P0** · **Durum:** ✅ **ÇÖZÜLDÜ**

Bu madde ilk yazıldığında bir **belirsizlik** olarak kaydedilmişti:

> "`vTaskDelayUntil` 0 tick'lik bir periyotta davranışı DOĞRULANMADI."

İlk gerçek çalıştırmada **doğrulandı — panik atıyor**:

```text
assert failed: xTaskDelayUntil tasks.c:1474 (( xTimeIncrement > 0U ))
  TaskRunner::endCycle()   ← TaskRunner.cpp:92
  storeTaskEntry()         ← StoreTask.cpp:41
```

Boot tamamlanıyor, tüm aşamalar geçiyor, web sunucusu dinlemeye başlıyor —
sonra `store` task'ı ilk `endCycle()`'da cihazı düşürüyordu.

**Düzeltme — `TaskRunner`'a olay güdümlü kip:**

```cpp
if (_period.ms > 0u) { vTaskDelayUntil(...); }
```

**Sözleşme:** periyot 0 veren task, döngüsünde **kendisi bloklamak
zorundadır**. `store` bunu `xQueueReceive(..., 1000 ms)` ile yapıyor.

**İkinci tuzak da kapatıldı:** `storage::tick()` hazır değilken hemen
dönüyordu. Periyot 0 ile bu, %100 CPU tüketen boş bir döngü olurdu; IDLE
task aç kalır ve watchdog cihazı resetlerdi. Artık o yol da
`vTaskDelay(1000 ms)` ile blokluyor.

Marjlar: TWDT 8 sn · `STORAGE` yumuşak son tarih 5 sn · besleme aralığı
≤ 1 sn.

**Ders:** "Doğrulanmadı" diye kaydedilen bir varsayım, er ya da geç
doğrulanır — genellikle sahada.

---

## ISSUE-013 / ISSUE-018 — KAPANDI

**Durum:** ✅ **ÇÖZÜLDÜ** — TASK-060

`src/BootWiring.{h,cpp}` yazıldı: 9 aşamalık boot tablosu (ARCHITECTURE
§7.1 ile birebir) + 5 task tablosu + gerçek `main.cpp`.

Sistem artık bağlı. Bunun ortaya çıkardığı gerçek: önceki tüm boyut
ölçümleri anlamsızdı (linker uygulamanın tamamını ölü kod olarak atıyordu).
Flash %27.8 → %65.4, RAM %7.2 → %22.

---

## ISSUE-024 — Stack boyutları TAHMİN, watermark ölçülmedi

**Bulunduğu yer:** `tasks/TaskConfig.h`
**Öncelik:** P1 · **Durum:** Açık — ilk çalıştırmadan sonra kapatılmalı

Beş task'ın stack boyutu (`4096`/`5120`/`3584`) tasarım sırasında tahmin
edildi. `TaskRunner` watermark topluyor ve TASK-062'de
`GET /api/diagnostics` üzerinden görünür hâle getirildi — ama **hiç
okunmadı**.

**Risk iki yönlü:** az verilen stack taşar (çökme), fazla verilen stack
RAM israf eder. İkisi de ilk çalıştırmada ölçülebilir.

**Kapatma ölçütü:** her task için `minStack` > 512 bayt pay bırakacak
şekilde ayarlanmış olmalı.


---

## ISSUE-026 — Web sunucusu lwIP hazır olmadan başlatılıyordu (BOOT DÖNGÜSÜ)

**Bulunduğu yer:** `tasks/NetworkTask.cpp` / `interfaces/web/WebService.cpp`
**Öncelik:** **P0** · **Durum:** ✅ **ÇÖZÜLDÜ**

**Belirti:** İlk gerçek donanım denemesinde sonsuz boot döngüsü.

```text
assert failed: tcpip_api_call ... (Invalid mbox)
  AsyncServer::begin()  ← AsyncTCP.cpp:749
  AsyncWebServer::begin()
  interfaces::web::begin()      ← WebService.cpp:60
  tasks::networkTaskEntry()     ← NetworkTask.cpp:51
```

**Kök neden:** `hal::wifi::begin()` radyoyu `WIFI_MODE_NULL`'a alıyor.
Arduino kaynağı okundu (`WiFiGeneric.cpp`):

```text
WiFiGenericClass::mode(m):
    if (cm == m) return true;              ← cm=0, m=0 → ERKEN DONER
    if (!cm && m) wifiLowLevelInit(...);   ← BU HIC CALISMAZ
                    └─ tcpipInit() → esp_netif_init()
```

Yani `WIFI_MODE_NULL` ile lwIP TCP/IP thread'i **hiç başlatılmıyor**.
`net` task'ı ise ilk `fsm::tick()`'ten (radyoyu bir moda alan yer) **önce**
`web::begin()` çağırıp `AsyncServer::begin()`'e giriyordu → geçersiz mbox →
panik → reset → sonsuz döngü.

**Düzeltme:** `WebService` ikiye ayrıldı.

```text
begin()  → rotalari, WS'i ve statik servisi KAYDEDER (lwIP gerekmez)
start()  → dinlemeye baslar; YALNIZCA radyo bir modda iken cagrilir
```

`NetworkTask` döngüsünde:

```cpp
if (!web::listening() && hal::wifi::mode() != RadioMode::OFF) web::start();
```

FSM ilk tick'te radyoyu bir moda alır (credential varsa STA, yoksa kurulum
AP'si) ve o andan itibaren lwIP ayaktadır. TASK-041 Karar 5 ("sunucu AP ve
STA fark etmeksizin başlar") **korundu**.

**Ders:** Bu hata ancak gerçek donanımda ortaya çıkabilirdi. Derleme, statik
tarama ve katman denetimlerinin hiçbiri "bir kütüphane çağrısının ön koşulu
sağlanmamış" durumunu yakalayamaz.

---

## ISSUE-027 — LittleFS bölüm etiketi uyuşmazlığı

**Bulunduğu yer:** `hal/FileStore.cpp` / `partitions.csv`
**Öncelik:** P1 · **Durum:** ✅ **ÇÖZÜLDÜ**

**Belirti:**

```text
E esp_littlefs: partition "spiffs" could not be found
[CRIT][0x0302] LittleFS mount edilemedi — BICIMLENDIRILIYOR
[ERRO][0x0302] LittleFS bicimlendirilemedi
[ERRO][0x0302] detail=4 asama basarisiz     ← her boot'ta DEGRADED
```

**Kök neden:** `partitions.csv`'de bölümün **adı** `littlefs`, alt tipi
`spiffs`. Arduino `LittleFS.begin()` bölümü **ADIYLA** arar ve varsayılan
etiketi `"spiffs"`'tir (`LittleFS.h:27`). Ad eşleşmediği için mount
başarısız, biçimlendirme de başarısız (bölüm bulunamıyor).

Sonuç: dosya sistemi **her boot'ta** kullanılamıyordu → web arayüzü statik
dosyaları yok, geçmiş kaydı yok, sistem kalıcı `DEGRADED`.

**Düzeltme:** `PARTITION_LABEL = "littlefs"` açıkça veriliyor:

```cpp
LittleFS.begin(false, "/littlefs", 10, PARTITION_LABEL)
```

Bölüm adı **değiştirilmedi** — açıklayıcı adı korumak, etiketi bir yerde
vermekten daha değerli. PlatformIO `uploadfs` bölümü **alt tipe** göre
bulduğu için (`builder/main.py:207`) ad serbesttir; `partitions.csv`'ye bu
ilişkiyi anlatan bir uyarı bloğu eklendi.

---

## ISSUE-001 (yeniden açıldı ve KABUL EDİLDİ) — encoder ADC1 kanallarını işgal ediyor

**Durum:** ⚠️ **BİLİNÇLİ KABUL** — çözülmedi, bedeli kabul edildi

TASK-002'de encoder analog bütçeyi korumak için `33/32 → 18/19` taşınmıştı.
**İlk saha denemesinde** donanımın hâlâ 33/32'ye kablolu olduğu görüldü:
encoder çevirisi hiçbir olay üretmiyordu (18/19 boşta, pull-up ile sabit).

Kullanıcı kodun geri alınmasını tercih etti. `BoardPins.h` 33/32'ye döndü.

**Kabul edilen bedel:**

```text
GPIO 32 = ADC1_CH4   ← artik encoder
GPIO 33 = ADC1_CH5   ← artik encoder

Mevcut 4 analog sensor (34/35/36/39) ETKILENMIYOR.
BESINCI/ALTINCI analog sensor icin YER KALMADI.
```

Yeni bir analog sensör gerekirse iki yol var: encoder'ı taşımak veya harici
bir ADC (ADS1115 vb.) eklemek.

**Kısıt gevşetildi ama boşluk bırakılmadı.** Eski `static_assert` (encoder
ADC1'de olamaz) kaldırıldı; yerine iki yeni iddia kondu:

1. Encoder pinleri bir **analog sensör piniyle çakışamaz** — böyle bir
   çakışma sessiz bir arıza olurdu (sensör okuması encoder darbeleriyle
   bozulur).
2. Encoder ADC1'deyken **32/33 analog olarak kullanılamaz** — ileride
   biri oraya bir sensör koymaya kalkarsa derleme durur.

Yani kısıt kaybolmadı, **gerçeğe uyarlandı**.

---

## ISSUE-028 — `setApInfo()` tanımlıydı ama HİÇ ÇAĞRILMIYORDU

**Bulunduğu yer:** `interfaces/ui/UiService.cpp` · `tasks/NetworkTask.cpp`
**Öncelik:** **P0** · **Durum:** ✅ **ÇÖZÜLDÜ**

**Belirti:** Kullanıcı cihaza giremiyordu. Kurulum AP'sinin SSID'si ve
şifresi **hiçbir yerde görünmüyordu** — ne OLED'de ne seri portta.

**Kök neden:** `interfaces::ui::setApInfo()` tanımlıydı, `softap::ssid()` ve
`softap::password()` tanımlıydı — ama **üçü de hiçbir yerden
çağrılmıyordu**. `UiModel.apSsid` / `apPassword` boş string kalıyor, OLED
`NETWORK` ekranı boş satır çiziyordu.

**P7 taramamın kör noktası:** "bildirilip TANIMLANMAMIŞ fonksiyon" arıyordum
ve 0 buldum. Bu fonksiyonlar **tanımlıydı, sadece hiç çağrılmıyordu** —
farklı ve burada çok daha zararlı bir ölü kod türü. Tarama genişletildi:
46 aday çıktı, ikisi (`setApInfo`, `conn::forget`) gerçek işlevsel boşluktu.

**Düzeltme — üç katman:**

1. `NetworkTask` her döngüde AP aktifken `setApInfo()` çağırıyor
2. **HOME ekranı** kurulum modunda SSID + şifre + `192.168.4.1` gösteriyor
   (bilgiyi `NETWORK` ekranına saklamak, kullanıcıyı önce gezinmeye zorlar —
   encoder çalışmıyorsa cihaza girmenin yolu kalmaz)
3. **Seri porta** da yazılıyor: OLED yoksa/bozuksa/encoder ölüyse tek yol

**Güvenlik notu:** seri porta yazılan, cihazın KENDİ ÜRETTİĞİ kurulum
şifresidir — kullanıcının ev ağı şifresi veya arayüz parolası değil. Zaten
OLED'de gösterilmek üzere tasarlandı (TASK-038) ve seri port en az OLED
kadar fiziksel erişim gerektirir.

---

## ISSUE-029 — `NETWORK_FORGET` komutu sessizce yok sayılıyordu

**Bulunduğu yer:** `domain/AppCore.cpp`
**Öncelik:** P1 · **Durum:** ✅ **ÇÖZÜLDÜ**

`FACTORY_RESET` ile **aynı sınıf hata**: uç nokta komutu kuyruğa koyuyor,
`{"ok":true}` dönüyor, `app_core` komutu "sahipsiz" listesinde bırakıp
hiçbir şey yapmıyordu. Kullanıcı "Ağı unut" der, onay alır, ağ silinmez.

**Düzeltme:** komut `fsm::requestForget()` bayrağına çevriliyor; işi `net`
task'ı kendi bağlamında yapıyor (`conn::forget()` + AP'ye dönüş).

Bayrak yolu bilinçli: `app_core` radyoya dokunamaz (P2) ve AsyncTCP
callback'i flash yazamaz (§14.6).

---

## ISSUE-030 — OLED I2C 100 kHz'de kalmış (ekran geçişlerinde takılma)

**Bulunduğu yer:** `hal/OledPanel.cpp`
**Öncelik:** P1 · **Durum:** ✅ **ÇÖZÜLDÜ**

**Belirti:** Encoder ile ekran geçişleri akıcı değil, takılmalar oluyor.

**Kök neden:** `Wire.begin()` çağrılıyordu ama **`Wire.setClock()` hiç
çağrılmamıştı** → Arduino varsayılanı olan 100 kHz'de kalınıyordu.

```text
SSD1306 tam karesi          : 1024 bayt
100 kHz'de (~10 us/bayt)    : ~105 ms
`ui` task periyodu          :   50 ms
                              ────────
Her ekran degisimi arayuzu IKI PERIYOT boyunca blokluyordu.
```

**Düzeltme:** `Wire.setClock(400000)` — I2C fast mode. Aynı kare **~26 ms**,
yani periyodun yarısından az. 400 kHz SSD1306 modüllerinin standart hızıdır.

**Not:** Bu, kod incelemesiyle yakalanabilecek bir eksiklikti ama
yakalanmadı — "eksik olan çağrı" aramak, "yanlış olan çağrı" aramaktan
zordur. Sahada tek bir cümlelik geri bildirimle ortaya çıktı.

---

## ISSUE-031 — Encoder ISR'ında sıçrama (bounce) reddi yoktu

**Bulunduğu yer:** `hal/InputDevices.cpp`
**Öncelik:** P1 · **Durum:** ✅ **ÇÖZÜLDÜ**

Quadrature tablosu geçersiz geçişleri eliyordu, ancak mekanik kontak
sıçraması tabloda **geçerli görünen** geçişler üretir (`00→01→00→01…`).
Tablo bunları `+1, -1, +1` olarak sayar; sonuç titrek gezinme ve kaçan
detentlerdir.

**Düzeltme:** ISR'a zaman kapısı eklendi.

```text
ENCODER_GLITCH_US = 600
Sicrama tipik olarak 100-300 us  → ELENIR
Elle cevirmede detentler >10 ms  → ETKILENMEZ
```

---

## ISSUE-032 — OLED'de uzun değerler ekrandan taşıyordu (IP okunamıyor)

**Bulunduğu yer:** `interfaces/ui/screens/Screens.cpp`
**Öncelik:** P1 · **Durum:** ✅ **ÇÖZÜLDÜ**

**Belirti:** "Küçük ekranın ağ sayfasında IP doğru değil ya da ekrana
sığmıyor."

**Kök neden:** `row()` değeri sabit `COL_VALUE = 62`'den başlatıyordu.

```text
"192.168.1.100" = 13 karakter × 6 px = 78 px
62 + 78 = 140 px   >   128 px ekran   → SON 2 HANE KESILIYORDU
```

IP değerinin kendisi doğruydu; **çizim taşıyordu**. Kullanıcının "doğru
değil" demesi bu yüzden — kesilmiş bir IP yanlış görünür.

**Düzeltme:** değerler artık **sağa yaslanıyor** (`x = 128 - textWidth`).
Değer etiketin üzerine binecek kadar uzunsa etiketin hemen sağından başlar,
böylece etiket her zaman okunur kalır.

`hal::oled::textWidth()` bu düzeltmeye kadar **hiç çağrılmıyordu** —
"tanımlı ama çağrılmayan" listesindeydi ve tam da bu iş için yazılmıştı.

**Aynı sınıf ikinci hata:** durum çubuğunda mod yazısı sabit `x=64`'ten,
hata rozeti sabit `x=114`'ten başlıyordu. `"CALISIYOR"` (54 px) 118'e kadar
uzanıp rozetin üzerine biniyordu. Rozet artık sağa yaslanıp yerini önce
ayırıyor, mod yazısı kalan alana sığdırılıyor.

**Doğrulama:** 9 tipik etiket/değer çifti hesaplandı — en uzunu
`255.255.255.255` dahil **hepsi 128 px'e tam oturuyor**.
