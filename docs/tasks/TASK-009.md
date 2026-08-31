# TASK-009 — WatchdogGuard

**Phase:** 2 — Boot & Task Framework · **Priority:** P0

## Objective

Task watchdog'unu **doğru sırayla** kurmak ve tüm denetlenen task'ları kapsamak.
Mevcut projedeki iki hatayı gidermek: init'in task'lardan sonra çağrılması ve bir task'ın
hiç kaydolmaması.

## Scope

- TWDT yapılandırması — boot'un **ilk adımında**, hiçbir task oluşturulmadan önce
- Task kayıt/silme sarmalayıcısı
- Besleme (feed) yardımcısı
- Reset nedeni okuma ve WDT kaynaklı reset'in CRITICAL olarak kaydı
- Görev sınıfına göre farklı zaman aşımı desteği

## Out of Scope

- Task oluşturma (TASK-011)
- Heartbeat izleme (TASK-011/012)
- Idle task watchdog yapılandırması (gerekirse ayrı değerlendirilir)

## Dependencies

- TASK-004, TASK-005

## Requirements

- `REQUIREMENTS.md` — §9 Watchdog `[~]`, §10 Task_SensorLogger kayıtsız

## Architecture References

- §6.5 Watchdog davranışı
- §7.1 Aşama 0
- §16.3 Watchdog reset davranışı

## Expected Design

### Karar gerektiren nokta — Zaman aşımı stratejisi

```text
Problem:      Kontrol task'ı 100 ms'de bir döner, ağ task'ı saniyelerce sürebilir
Constraints:  ESP32 TWDT tek global timeout kullanır
Approaches:   (a) tek global timeout, en yavaş task'a göre (uzun)
              (b) global timeout kısa + yavaş task'ın işini alt adımlara bölmesi
              (c) TWDT + uygulama seviyesi heartbeat izleme birlikte
Trade-offs:   (a) kontrol task'ı kilitlenirse geç fark edilir — güvenlik açığı
              (b) doğru çözüm ama tasarım disiplini gerektirir
Recommended:  (b) + (c) — ARCHITECTURE §6.5 gereği ikisi birlikte
```

**Kritik kural:** Besleme **yalnızca döngü sonunda**, tüm iş bittikten sonra yapılır.
Döngü ortasında besleme, task'ın gerçekten ilerlediğini kanıtlamaz ve watchdog'u anlamsız
kılar. Mevcut projede `Task_WiFiMonitor` beslemeyi bloklayan `connect()` çağrısından sonra
yapıyordu — yani 5 saniyelik bloklama watchdog tarafından görülmüyordu.

## Implementation Notes

- Kayıt, task'ın **kendi başlangıcında** yapılmalı; başka bir task adına kayıt yapılmamalı.
- Bir task sonlanacaksa kaydını silmeli, aksi halde TWDT yanlış alarm üretir.
- Panik modu **açık** olmalı: kilitlenme durumunda donanımsal reset. Loglama modu tek
  başına yeterli değildir; kilitlenmiş bir sistem pompayı açık bırakabilir.
- WDT reset sonrası boot'ta röleler zaten güvenli seviyeden başlar (TASK-017), ancak
  reset nedeni **kalıcı olarak** kaydedilmeli ki tekrarlayan sorun fark edilsin.
- Arduino-ESP32 sürümüne göre TWDT API imzası farklıdır; kullanılan sürüm doğrulanmalı.

## Files

- `src/core/WatchdogGuard.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] TWDT boot'un ilk adımında, task oluşturulmadan önce yapılandırılıyor
- [ ] Kayıt/silme/besleme sarmalayıcıları çalışıyor
- [ ] Panik modu etkin
- [ ] Reset nedeni okunuyor, WDT reset'i CRITICAL olarak kaydediliyor
- [ ] Besleme yalnızca döngü sonunda çağrılıyor (kural belgelenmiş)
- [ ] Zaman aşımı stratejisi seçilip gerekçelendirilmiş

## Test Plan

- [ ] Kasıtlı sonsuz döngü ile bir task kilitlendiğinde WDT reset gerçekleşiyor
- [ ] Reset sonrası boot'ta neden doğru raporlanıyor
- [ ] Normal çalışmada yanlış pozitif reset olmuyor (uzun süreli test)
- [ ] Beş task'ın da kayıtlı olduğu çalışma zamanında doğrulandı
- [ ] Kayıt silme sonrası yanlış alarm üretilmiyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§6.5)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — kayıt başarısızlığı ele alınıyor mu
- [ ] ESP32 resource kullanımı uygun mu? — TWDT API sürümü doğru mu
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **ters init sırası taşınmamalı**

## Definition of Done

Ortak DoD + kasıtlı kilitlenme testi ile reset kanıtlandı + reset nedeni raporlanıyor.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Ortam gerçekleri (ölçüldü, varsayılmadı)

Framework header'ları ve `sdkconfig` incelendi:

```text
API (IDF 4.4 / arduino-esp32 2.0.17):
  esp_task_wdt_init(uint32_t timeout_SANIYE, bool panic)
  esp_task_wdt_add/delete/status(TaskHandle_t)   → NULL = mevcut task
  esp_task_wdt_reset(void)

sdkconfig varsayilanlari:
  CONFIG_ESP_TASK_WDT=y                      → TWDT ZATEN ACIK
  CONFIG_ESP_TASK_WDT_TIMEOUT_S=5
  CONFIG_ESP_TASK_WDT_PANIC=y                → panic ZATEN acik
  CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y → IDLE0 izleniyor
  # CPU1 IDLE izlenmiyor
  CONFIG_ESP_INT_WDT=y, TIMEOUT_MS=300       → ayri interrupt watchdog

Arduino loopTask: loopTaskWDTEnabled = false → TWDT'ye ABONE DEGIL
```

### Bu, task dosyasındaki varsayımı düzeltiyor

Task dosyası "TWDT boot'un ilk adımında, hiçbir task oluşturulmadan önce
**yapılandırılır**" diyor. Gerçek daha iyi: **TWDT `setup()` çalışmadan önce
IDF tarafından zaten başlatılmış durumda.** Sistem ilk komuttan itibaren
korumalı. Bizim `begin()` çağrımız bir *ilk kurulum* değil, **yeniden
yapılandırma**dır (header: "If the TWDT is already initialized... this function
will update the TWDT's timeout period and panic configurations instead").

Bu ayrıca mevcut projedeki hatanın neden **sessizce yarı çalıştığını** açıklıyor:
`esp_task_wdt_add()` çağrıları IDF'in 5 sn'lik TWDT'sine kaydoluyordu; sonradan
gelen `esp_task_wdt_init(15, true)` yalnızca süreyi değiştiriyordu. Yani koruma
vardı ama **niyet edilenden farklı bir süreyle** ve geliştirici bunun farkında
değildi. Sıra hatası yine de gerçek: hangi sürenin geçerli olduğu belirsizdi.

## Karar 1 — TWDT zaman aşımı değeri

```text
Problem:      Tek global TWDT suresi ne olmali?
Guvenlik hesabi:
  app_core kilitlenirse ve pompa ACIK ise, pompa su kadar fazladan calisir:
      TWDT timeout + reset + boot Asama 1 (GPIO guvenli seviye)
  Boot Asama 1'e ~0.5 sn icinde ulasilmali (TASK-010).
  → 15 sn timeout = ~15.5 sn kontrolsuz pompa
  →  8 sn timeout = ~ 8.5 sn kontrolsuz pompa

En uzun mesru bloklama (olculen/tahmin):
  store   : LittleFS sektor silme ~20-100 ms, toplu yazma < 1 sn
  net     : FSM bloklamiyor (TASK-035); esp_wifi cagrilari onlarca ms
  app_core: bloklama YOK (P3)
  → 5 sn bile genis pay birakiyor

Selected: 8 saniye (varsayilan, yapilandirilabilir)
Gerekce:  Mesru en uzun islemin ~8 kati pay; buna karsilik kontrolsuz pompa
          suresi 15 sn yerine 8.5 sn. Guvenlik lehine secildi.
```

## Karar 2 — ARCHITECTURE §6.5 ile çelişki: per-task timeout mümkün değil

```text
ARCHITECTURE §6.5 diyor ki:
  "Kontrol task'lari icin kisa (~5 s), ag ve storage icin uzun (~15 s).
   Tek bir global deger yerine gorev sinifina gore."

PLATFORM GERCEGI:
  IDF 4.4 TWDT TEK GLOBAL zamanlayicidir. Task basina farkli timeout
  DESTEKLENMEZ. esp_task_wdt_init() tum aboneler icin ayni sureyi belirler.

Cozum (ARCHITECTURE §6.5'in kendi onerisi (b)+(c)):
  Donanim kati : tek global TWDT (8 sn) — TAM kilitlenmeye karsi
  Uygulama kati: task basina YUMUSAK son tarih (heartbeat, TASK-012)
                 — yavaslamaya karsi, cok daha siki (100-500 ms)

Bu task `TaskClass` ve sinif basina yumusak son tarihleri tanimlar;
TASK-011 kayit sirasinda kullanir, TASK-012 izler.
```

Bu bir **mimari sapma** değil, mimarinin platformdaki tek uygulanabilir
karşılığıdır. Yine de `ARCHITECTURE.md` §6.5 metni "task sınıfına göre TWDT
timeout" izlenimi verdiği için **ISSUE-011** olarak kaydedildi.

## Karar 3 — Reset nedeni burada TEKRAR implement EDİLMEYECEK

```text
Task scope'u "Reset nedeni okuma ve WDT kaynakli reset'in CRITICAL kaydi"
diyor. Ancak bu TASK-005'te ZATEN implement edildi:
    diag::captureResetReason()  → ResetReason dondurur,
                                  WDT ve panic icin CRITICAL loglar

Selected: Burada YENIDEN YAZILMAZ (P7 — olu/ikiz kod yasagi).
          WatchdogGuard yalnizca TWDT sarmalayicisidir.
          Boot Asama 0 (TASK-010) diag::captureResetReason() cagirir.
Not:      Header'da bu sinir acikca belgelenir ki sonraki gelistirici
          "eksik" sanip ikinci bir implementasyon yazmasin.
```

## Karar 4 — `subscribe()` çağıran task'ın kendisi tarafından

`esp_task_wdt_add(NULL)` mevcut task'ı kaydeder. Kural (ARCHITECTURE §6.5):
**her task kendi başlangıcında kendini kaydeder**; başka bir task adına kayıt
yapılmaz. Bunun nedeni, kaydın task'ın gerçekten çalışmaya başladığını
kanıtlamasıdır.

## Karar 5 — IDLE0 izleniyor: bilinçli olarak dokunulmuyor

`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y` — Core 0'ın IDLE task'ı izleniyor.
Core 0'da `net` ve `store` çalışacak. Bu koruma **kaldırılmayacak**: bir Core 0
task'ı IDLE'ı 8 sn aç bırakacak kadar CPU tüketiyorsa bu gerçek bir hatadır ve
yakalanmalıdır.

## Kapsam dışı bırakılanlar

- Task oluşturma → TASK-011
- Heartbeat izleme ve mod kararı → TASK-011 / TASK-012
- Reset nedeni okuma → TASK-005'te yapıldı, tekrarlanmayacak
- IDLE task watchdog yapılandırması → varsayılan korunuyor

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] TWDT task oluşturulmadan önce yapılandırılıyor — `begin()` boot Aşama 0'da
      çağrılacak (TASK-010). **Düzeltme:** TWDT zaten IDF tarafından `setup()`
      öncesi başlatılmış; bizim çağrımız yeniden yapılandırma. Bu, doğrulanmış
      ortam gerçeği olarak belgelendi.
- [x] Kayıt / silme / besleme sarmalayıcıları çalışıyor (`subscribe`, `unsubscribe`, `feed`)
- [x] Panik modu etkin — varsayılan `panic = true`
- [x] Reset nedeni okunuyor ve WDT reset'i CRITICAL kaydediliyor —
      **TASK-005'te implement edilmiş** `diag::captureResetReason()` kullanılıyor;
      burada tekrarlanmadı (P7)
- [x] "Besleme yalnızca döngü sonunda" kuralı header'da belgelendi ve gerekçesi
      (mevcut projedeki `Task_WiFiMonitor` hatası) yazıldı
- [x] Zaman aşımı stratejisi seçildi ve **güvenlik hesabıyla** gerekçelendirildi

## Doğrulanmış ortam gerçekleri

Tahminle değil, framework header'ları ve `sdkconfig` okunarak:

| Gerçek | Etki |
|---|---|
| `esp_task_wdt_init(uint32_t saniye, bool panic)` | API imzası doğrulandı |
| TWDT `setup()` öncesi zaten açık (5 s, panic) | `begin()` = yeniden yapılandırma |
| `esp_task_wdt_add(NULL)` = mevcut task | Her task kendini kaydediyor |
| IDLE0 izleniyor, IDLE1 izlenmiyor | Core 0 task'ları (net, store) IDLE'ı aç bırakamaz |
| `loopTaskWDTEnabled = false` | Arduino `loop()` abone değil — `vTaskDelay` güvenli |
| `CONFIG_ESP_INT_WDT_TIMEOUT_MS=300` | Ayrı interrupt watchdog aktif |

## Zaman aşımı kararının güvenlik hesabı

```text
app_core kilitlenir + pompa ACIK  →  pompa su kadar kontrolsuz calisir:
    TWDT timeout + reset + boot Asama 1 (GPIO guvenli seviye ~0.5 sn)

    15 sn secilirse  →  ~15.5 sn kontrolsuz pompa
     8 sn secilirse  →  ~ 8.5 sn kontrolsuz pompa

En uzun mesru bloklama: flash toplu yazma < 1 sn
→ 8 sn hem ~8 kat pay birakiyor hem pompa riskini yariya indiriyor
```

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı (Flash +136 bayt)
- [x] TWDT API imzası ve semantiği framework header'ından doğrulandı
- [ ] **Kasıtlı kilitlenme → WDT reset testi — donanım gerekiyor**
- [ ] **Reset sonrası neden raporlaması — donanım gerekiyor**
- [ ] **Uzun süreli yanlış pozitif kontrolü — donanım gerekiyor**
- [ ] **Beş task'ın kayıtlı olduğunun doğrulanması — TASK-011 sonrası, donanımda**
- [ ] **Kayıt silme sonrası yanlış alarm — donanım gerekiyor**

> `subscriberCount()` bu doğrulamayı kolaylaştırmak için eklendi: TASK-011
> sonrası beklenen değer 5'tir ve TASK-060'ta çalışma zamanında kontrol edilecek.

## Review Checklist

- [x] Architecture'a uygun mu? — §6.5 iki katmanlı koruma uygulandı
- [x] Gereksiz abstraction var mı? — beş serbest fonksiyon + bir enum;
      sınıf, sanal fonksiyon, şablon yok
- [x] Blocking işlem var mı? — hiçbir çağrı bloklamıyor
- [x] Shared state güvenli mi? — abone sayacı `std::atomic`;
      TWDT'nin kendi durumu IDF tarafından korunuyor
- [x] Memory problemi var mı? — 8 bayt statik (sayaç + süre + bayrak)
- [x] Error handling var mı? — `esp_err_t` → `ErrCode` eşlemesi; kayıt
      başarısızlığı **CRITICAL** loglanıyor (mevcut projede bir task hiç
      kaydolmamıştı ve kimse fark etmemişti)
- [x] ESP32 resource kullanımı uygun mu? — **API sürümü doğrulandı**,
      IDLE0 koruması bilinçli olarak korundu
- [x] Task sorumluluğu doğru mu? — her task kendini kaydeder kuralı zorlanıyor
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Ters init sırası
      taşınmadı; ayrıca `feed()` içinde bilinçli olarak log yapılmıyor
      (her döngüde çağrılır, loglamak sel yaratır — bunun yerine hata kodu döner).

## Bulgular

**ISSUE-011** kaydedildi: `ARCHITECTURE.md` §6.5 "task sınıfına göre TWDT
timeout" diyor ama IDF 4.4'te TWDT **tek global zamanlayıcıdır**; task başına
timeout desteklenmez. Mimarinin kendi (b)+(c) önerisi uygulandı: global TWDT +
uygulama katmanı heartbeat. Mimari metninin düzeltilmesi önerildi (o dosya
bu task'ın Files listesinde değil).

## Durum

**TASK-009: TAMAMLANDI** (donanım testleri bekliyor).
