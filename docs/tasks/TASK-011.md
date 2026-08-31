# TASK-011 — Task Framework & Heartbeat

**Phase:** 2 — Boot & Task Framework · **Priority:** P0

## Objective

Beş task'ın tutarlı bir iskeletle oluşturulmasını sağlamak: çekirdeğe sabitleme, periyodik
döngü, watchdog kaydı ve heartbeat yayını her task'ta aynı şekilde yapılmalı.

## Scope

- Task tanım tablosu: ad, giriş fonksiyonu, periyot, öncelik, stack, çekirdek
- Çekirdeğe sabitlenmiş task oluşturma (`xTaskCreatePinnedToCore`)
- Periyodik döngü yardımcısı (sabit periyot, kayma birikmeyen)
- Her döngüde: iş → heartbeat → watchdog besleme sırası
- Stack watermark ölçüm desteği

## Out of Scope

- Task'ların iş mantığı (kendi task'larında)
- Heartbeat izleme ve mod kararı (TASK-012)

## Dependencies

- TASK-009

## Requirements

- `REQUIREMENTS.md` — §10 (task'lar çekirdeğe sabitlenmemiş, `loop()` boş döngü)

## Architecture References

- §6.1 Task tablosu · §6.2 Çekirdek dağılımı · §6.3 Öncelik gerekçesi
- §6.4 Task'a dönüşmeyen işler · §6.5 Watchdog

## Expected Design

### Karar gerektiren nokta — Periyodik zamanlama

```text
Problem:      Sabit periyotlu döngü nasıl kurulacak?
Constraints:  app_core 100 ms'de bir güvenlik değerlendirmesi yapmalı;
              iş süresi değişkenken periyot kaymamalı
Approaches:   (a) vTaskDelay(period) — iş süresi periyoda eklenir, kayma birikir
              (b) vTaskDelayUntil — mutlak uyanma zamanı, kayma birikmez
              (c) yazılım timer
Trade-offs:   (a) mevcut projenin yaklaşımı; 100 ms hedefi 130 ms'e kayabilir
              (c) timer callback bağlamında güvenlik mantığı çalıştırmak uygun değil
Recommended:  (b) — periyot garantisi güvenlik zamanlaması için gereklidir
```

**Çekirdek dağılımı** (§6.2): Core 0 → `net`, `store` (+ Wi-Fi/AsyncTCP yığını).
Core 1 → `app_core`, `io_sense`, `ui`. Gerekçe: Wi-Fi yığını öngörülemeyen süreler
harcar; güvenlik ve aktüatör kontrolü bundan yalıtılmalıdır.

**Öncelik**: `app_core` en yüksek (4). Mevcut projede en yüksek öncelik loglama
task'ındaydı — bu ters çevrilmiştir.

## Implementation Notes

- Stack boyutları başlangıç tahminidir; TASK-062'de watermark ölçümüyle düzeltilecektir.
  Başlangıçta cömert, sonra ölçüme göre kısılmalı.
- Arduino `loop()` **kullanılmayacak**. Kullanılmayacaksa boş bırakılmamalı; sürekli
  çalışan boş döngü CPU harcar. Ya silinmeli ya da uygun bir bekleme içermeli.
- Her task döngüsü şu sırayı izlemeli: **iş → heartbeat → watchdog besleme**.
  Besleme her zaman en sonda.
- Task oluşturma başarısızlığı ele alınmalı: `xTaskCreatePinnedToCore` başarısız olursa
  bu CRITICAL bir hatadır ve sistem SAFE moda geçmelidir.
- Task'lar boot'ta gerekli önkoşulları beklemeli (EventGroup, §5) — örneğin config
  yüklenmeden sensör örneklemesi başlamamalı.

## Files

- `src/tasks/TaskConfig.h` (yeni — tablo)
- `src/tasks/TaskRunner.h` / `.cpp` (yeni — periyodik döngü yardımcısı)
- `src/main.cpp` (güncelleme)

## Acceptance Criteria

- [ ] Beş task çekirdeğe sabitlenmiş olarak oluşturuluyor
- [ ] Periyodik zamanlama kayma biriktirmiyor
- [ ] Her task WDT'ye kendi başlangıcında kaydoluyor
- [ ] Besleme yalnızca döngü sonunda
- [ ] Heartbeat her döngüde yayınlanıyor
- [ ] `loop()` CPU harcamıyor
- [ ] Task oluşturma başarısızlığı CRITICAL olarak ele alınıyor
- [ ] Stack watermark ölçülebiliyor

## Test Plan

- [ ] Beş task'ın da doğru çekirdekte çalıştığı çalışma zamanında doğrulandı
- [ ] `app_core` periyodu ölçüldü; hedeften sapma kabul edilebilir sınırda
- [ ] Yapay yük altında periyot kayması biriktirmiyor
- [ ] Stack watermark ilk ölçümü alındı ve kaydedildi
- [ ] Task oluşturma kasıtlı başarısız kılındığında sistem SAFE moda geçiyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§6)
- [ ] Gereksiz abstraction var mı? — task sayısı 5'te kaldı mı (§6.4)
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — stack boyutları
- [ ] Error handling var mı? — task oluşturma hatası
- [ ] ESP32 resource kullanımı uygun mu? — çekirdek dağılımı doğru mu
- [ ] Task sorumluluğu doğru mu? — öncelik sırası doğru mu
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — eski 4 task yapısı ve öncelikleri taşınmamalı

## Definition of Done

Ortak DoD + çekirdek ve periyot ölçümleri kayıtlı + ilk stack watermark verisi alındı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Ortam gerçeği (ölçüldü)

```text
ESP-IDF FreeRTOS, vanilla FreeRTOS'tan FARKLI:

  xTaskCreatePinnedToCore(..., usStackDepth, ...)
      "The size of the task stack specified as the number of BYTES.
       Note that this differs from vanilla FreeRTOS."

  uxTaskGetStackHighWaterMark()
      "the minimum free stack space ... IN BYTES NOT WORDS,
       unlike vanilla FreeRTOS"
```

Bu doğrulama önemli: birim word sanılsaydı stack'ler **4 kat küçük** oluşturulur
ve sahada rastgele taşma yaşanırdı. ARCHITECTURE §6.1'deki değerler (4 KB, 3 KB,
5 KB, 3.5 KB, 4 KB) doğrudan bayt olarak kullanılıyor.

## Karar 1 — Heartbeat deposu nerede yaşamalı? (katman kısıtı)

```text
Problem:      TaskRunner (tasks/) heartbeat YAZAR.
              SystemSupervisor (domain/, TASK-012) heartbeat OKUR.
              Ama domain/ yalnizca core/'a bagimli olabilir (D5) —
              domain/'in tasks/'i include etmesi katman ihlalidir.
Approaches:   (a) depo tasks/ icinde  → domain → tasks bagimliligi, IHLAL
              (b) depo SystemState icinde → 5 task yazar, TEK YAZAR kurali (P1) IHLAL
              (c) depo core/ icinde, kendi senkronizasyonuyla
Selected:     (c) — `core/TaskRegistry` olusturuldu.
Senkronizasyon: her task YALNIZCA kendi slotuna yazar → cekisme yok.
              `std::atomic<uint32_t>` slot basina; kilitsiz, ~nanosaniye maliyet.
              Okuyucu (app_core) tum slotlari okur — atomikler icin ideal desen.
```

**Files listesinden sapma:** Task dosyası `TaskConfig.h`, `TaskRunner.h/.cpp` ve
`main.cpp` listeliyor. `core/TaskRegistry.h/.cpp` **eklendi**; alternatifi bir
katman ihlaliydi. Sapma bilinçli ve gerekçeli.

## Karar 2 — EventGroup gerekmiyor (P7)

```text
Task notu: "Task'lar boot'ta gerekli onkosullari beklemeli (EventGroup, §5)"

INCELEME SONUCU: gerekmiyor.
  ARCHITECTURE §7.1 boot sirasi:
     Asama 3 config → 4 FS → 5 OLED → 6 sensor HW → 7 Wi-Fi → 8 TASK OLUSTURMA

  Task'lar EN SON asamada olusturuluyor. Calismaya basladiklarinda tum
  onkosullar ZATEN saglanmis durumda. Bir EventGroup eklemek, hicbir zaman
  beklemeyecek bir bekleme mekanizmasi olurdu → olu kod (P7).

Selected: EventGroup EKLENMIYOR.
Kosul:    Bu karar boot sirasinin korunmasina baglidir. Task'lar ileride daha
          erken olusturulmak istenirse EventGroup gerekli hale gelir; bu bagimlilik
          TaskConfig.h icinde belgelendi.
```

## Karar 3 — Periyodik zamanlama

```text
Selected: vTaskDelayUntil (mutlak uyanma zamani)
Reddedilen: vTaskDelay(period) — mevcut projenin yaklasimi.
            Is suresi periyoda EKLENIR: 100 ms hedef, 30 ms is → 130 ms gercek
            periyot. Kayma birikir ve guvenlik dongusunun zamanlamasi bozulur.

Ilk uyanma zamani task basinda alinir; her donguden sonra period kadar ilerletilir.
Bir dongu periyodu asarsa vTaskDelayUntil hemen doner (kayma telafi edilmez,
ama biriktirilmez de) ve asim SAYILIR — TASK-012 bunu yavaslamaya cevirir.
```

## Karar 4 — Döngü sırası `TaskRunner` tarafından ZORLANIR

```text
ARCHITECTURE §6.5 kurali:  is → heartbeat → watchdog besleme

Bu kural yoruma birakilmaz; TaskRunner'in yapisi zorlar:

    runner.begin();            // WDT kaydi + registry kaydi
    for (;;) {
        ...is...
        runner.endCycle();     // heartbeat → WDT besle → vTaskDelayUntil
    }

Besleme dongunun EN SONUNDA. Mevcut projede `Task_WiFiMonitor` beslemeyi
5 saniye bloklayan `connect()` cagrisindan SONRA yapiyordu — bloklama
watchdog tarafindan hic gorulmedi.
```

## Karar 5 — Stack watermark her döngüde ölçülmez

`uxTaskGetStackHighWaterMark()` stack'i tarar; maliyeti stack boyutuyla
orantılıdır. `ui` task'ı 20 Hz çalışıyor — her döngüde ölçmek israf olur.
**64 döngüde bir** ölçülür; watermark yavaş değişen bir büyüklüktür.

## Karar 6 — `main.cpp` bu task'ta DEĞİŞTİRİLMİYOR

Files listesi `main.cpp (güncelleme)` diyor. Ancak task'ları gerçekten
oluşturmak boot Aşama 8'in işidir ve **boot wiring boot wiring kapsamında (ISSUE-013)dır**.
Bu task `tasks::createAll()` fonksiyonunu sağlar; onu çağırmak TASK-013'e aittir.

`main.cpp`'yi yalnızca listede geçtiği için değiştirmek, kapsam dışı iş yapmak
olurdu. Değiştirilmedi.

## Task tablosu (ARCHITECTURE §6.1)

| Task | Periyot | Öncelik | Stack (bayt) | Çekirdek | Sınıf |
|---|---|---|---|---|---|
| `app_core` | 100 ms | 4 | 4096 | 1 | CONTROL |
| `io_sense` | 250 ms | 3 | 3072 | 1 | SENSING |
| `net` | 100 ms | 2 | 5120 | 0 | NETWORK |
| `ui` | 50 ms | 2 | 3584 | 1 | UI |
| `store` | 100 ms | 1 | 4096 | 0 | STORAGE |

`app_core` en yüksek öncelikte: güvenlik kararları gecikmemelidir. Mevcut
projede en yüksek öncelik **loglama task'ındaydı** — ters çevrildi.

Çekirdek dağılımı: Wi-Fi/lwIP yığını Core 0'da öngörülemeyen süreler harcar;
güvenlik ve kontrol Core 1'e yalıtıldı (ARCHITECTURE §6.2).

## Kapsam dışı bırakılanlar

- Task'ların iş mantığı → kendi task'ları
- Heartbeat İZLEME ve mod kararı → TASK-012 (burada yalnızca depo + yazma)
- Boot wiring, `createAll()` çağrısı → TASK-013

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Beş task çekirdeğe sabitlenmiş oluşturuluyor — `xTaskCreatePinnedToCore`;
      sabitlenmemiş `xTaskCreate` kullanımı **yok** (tarama ile doğrulandı)
- [x] Periyodik zamanlama kayma biriktirmiyor — `vTaskDelayUntil` (mutlak uyanma);
      `vTaskDelay(period)` kullanılmıyor
- [x] Her task WDT'ye **kendi başlangıcında** kaydoluyor (`TaskRunner::begin()`)
- [x] Besleme yalnızca döngü sonunda — `endCycle()` sırası tarama ile doğrulandı:
      `beat` → `updateStack` → **`feed`** → `vTaskDelayUntil`
- [x] Heartbeat her döngüde yayınlanıyor
- [x] `loop()` CPU harcamıyor (TASK-001'de `vTaskDelay` ile çözüldü)
- [x] Task oluşturma başarısızlığı CRITICAL + `SYS_TASK_CREATE_FAILED` döndürüyor →
      boot yürütücüsü zorunlu aşama başarısızlığı olarak SAFE moda çeviriyor
- [x] Stack watermark ölçülebiliyor (`TaskRegistry::minFreeStackBytes`)

## Doğrulanmış ortam gerçeği — stack birimi

```text
xTaskCreatePinnedToCore usStackDepth  → BAYT ("differs from vanilla FreeRTOS")
uxTaskGetStackHighWaterMark()         → BAYT ("in bytes not words")
```

Bu doğrulanmasaydı ve birim word sanılsaydı stack'ler **4 kat küçük** oluşur,
sahada rastgele taşma yaşanırdı. ARCHITECTURE §6.1 değerleri doğrudan bayt
olarak kullanıldı.

## API kullanılabilirlik doğrulaması

`TaskRunner` ve `createAll()` sahte bir task ve iki satırlık tabloyla derlendi:
kullanım deseni (`begin()` → döngü → `endCycle()`) ve tablo kurulumu tip
düzeyinde tutarlı. Harness derlemeden sonra kaldırıldı.

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı (Flash +152 bayt)
- [x] `vTaskDelayUntil` kullanımı doğrulandı
- [x] Yalnızca `xTaskCreatePinnedToCore` kullanılıyor
- [x] Döngü sırası (iş → heartbeat → besleme → bekleme) tarama ile doğrulandı
- [ ] **Beş task'ın doğru çekirdekte çalıştığı — donanım gerekiyor**
- [ ] **`app_core` periyot ölçümü — donanım gerekiyor**
- [ ] **Yük altında kayma biriktirmediği — donanım gerekiyor**
- [ ] **İlk stack watermark verisi — donanım gerekiyor**
- [ ] **Task oluşturma kasıtlı başarısızlığında SAFE mod — donanım gerekiyor**

> Task'lar henüz oluşturulmuyor: `createAll()` çağrısı boot Aşama 8'e aittir ve
> **boot wiring kapsamındadır (ISSUE-013)**. Çalışma zamanı doğrulamaları o task'tan sonra,
> TASK-060/TASK-062'de yapılacak. `TaskRegistry` bu ölçümleri şimdiden
> topladığı için o task'lar bu dosyalara yeniden dokunmayacak.

## Review Checklist

- [x] Architecture'a uygun mu? — §6.1 tablo, §6.2 çekirdek, §6.3 öncelik,
      §6.5 döngü sırası birebir uygulandı
- [x] **Gereksiz abstraction var mı? — task sayısı 5'te kaldı** (§6.4).
      Ayrıca EventGroup **bilinçli olarak eklenmedi**: boot sırası zaten
      önkoşulları garanti ediyor, eklenirse hiç beklemeyecek bir bekleme
      mekanizması olurdu (P7).
- [x] Blocking işlem var mı? — `vTaskDelayUntil` bir yield'dır, bloklama değil;
      `TaskRunner` içinde `delay()` veya bekleme döngüsü yok
- [x] Shared state güvenli mi? — heartbeat slot başına `std::atomic`;
      her task yalnızca kendi slotuna yazar → çekişme yok, kilit yok
- [x] Memory problemi var mı? — `TaskRegistry` ~70 bayt statik;
      stack boyutları ARCHITECTURE değerleri (toplam 19.8 KB), TASK-062'de
      ölçüme göre düzeltilecek
- [x] Error handling var mı? — TWDT kayıt hatası CRITICAL; task oluşturma
      hatası CRITICAL + hata kodu; `feed()` başarısızlığı **yalnızca ilk kez**
      loglanıyor (her döngüde çağrıldığı için log seli önlendi)
- [x] ESP32 resource kullanımı uygun mu? — **stack birimi doğrulandı**,
      watermark seyrek örnekleniyor (64 döngüde bir; her döngüde ölçmek israf)
- [x] Task sorumluluğu doğru mu? — `app_core` en yüksek öncelikte (4);
      mevcut projede en yüksek öncelik **loglama task'ındaydı**, ters çevrildi
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Eski 4 task yapısı,
      öncelik dağılımı ve `vTaskDelay` deseni taşınmadı. `Task_WifiLed` ayrı
      task olarak **açılmadı** (§6.4 gereği `ui` döngüsünde çözülecek).

## Files listesinden bilinçli sapmalar

| Dosya | Karar | Gerekçe |
|---|---|---|
| `core/TaskRegistry.h/.cpp` | **eklendi** | Heartbeat'i `tasks/` içine koymak `domain/ → tasks/` bağımlılığı yaratırdı (D5 ihlali); `SystemState`'e koymak tek yazar kuralını (P1) bozardı |
| `src/main.cpp` | **değiştirilmedi** | `createAll()` çağrısı boot Aşama 8'in işi ve **boot wiring kapsamında (ISSUE-013)**. Yalnızca listede geçtiği için değiştirmek kapsam dışı iş olurdu |

## Bulgular

Yeni eklenen tüm enum ve sabit isimleri (ISSUE-009 dersi gereği) framework
makrolarına karşı tarandı — bu turda çakışma yok.

## Durum

**TASK-011: TAMAMLANDI** (çalışma zamanı doğrulaması boot wiring sonrası).
