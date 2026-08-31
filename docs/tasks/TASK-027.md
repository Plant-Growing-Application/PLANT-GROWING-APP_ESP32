# TASK-027 — SensorService & io_sense Task

**Phase:** 5 — Sensor System · **Priority:** P0

## Objective

Tüm sensörleri tek bir servis altında periyodik olarak örneklemek ve sonucu `StateStore`'a
**tek yazar** olarak yayınlamak. Sensör okuma ile UI çiziminin birbirine karışmasını
(mevcut projenin en belirgin katman ihlali) kalıcı olarak engellemek.

## Scope

- `SensorService`: kayıt tablosundaki sensörleri periyoduna göre örnekleme
- Hattan geçirip `StateStore.publishSensors()` çağrısı
- Sensör bazlı örnekleme periyodu yönetimi
- `io_sense` task döngüsü, heartbeat ve watchdog entegrasyonu
- Örnekleme süresi bütçe kontrolü

## Out of Scope

- Sensör implementasyonları (TASK-024, 025, 026)
- Ekran gösterimi — **kesinlikle bu task'ta değil**
- Otomasyon kararları

## Dependencies

- TASK-024, TASK-025, TASK-026, TASK-011

## Requirements

- `REQUIREMENTS.md` — §3.7, §10 (Task_SensorLogger), Kritik Problem 2

## Architecture References

- §2.5 SensorService (yasaklar listesi)
- §3.1 Sensör → Web akışı · §6.1 io_sense task satırı
- §9.4 Okuma/işleme/gösterim ayrımı

## Expected Design

### Kesin yasaklar (§2.5)

`SensorService` **hiçbir koşulda**:

- Ekrana çizmez
- Ağa bağlanmaz
- Aktüatör tetiklemez
- Karar vermez

Mevcut `Sensor::SensorValues()` fonksiyonu üçünü de ihlal ediyordu (okuma + OLED çizimi
aynı yerde). Bu task o desenin karşıtıdır.

### Karar gerektiren nokta — Örnekleme zamanlaması

```text
Problem:      Sensörlerin periyotları farklı (seviye 500 ms, sıcaklık 1 s, pH/EC 2 s)
Constraints:  io_sense task'ı 250 ms'de bir çalışıyor;
              tüm sensörleri aynı döngüde okumak süre sıçraması yaratır;
              güvenlik sensörü asla atlanmamalı
Approaches:   (a) her döngüde hepsini oku
              (b) her sensöre bir sonraki okuma zamanı, sırayla
              (c) döngü başına en fazla N sensör (yük dağıtma)
Trade-offs:   (a) gereksiz yük ve prob yıpranması (pH/EC);
              (c) güvenlik sensörünün gecikmesine yol açabilir
Recommended:  (b) + güvenlik sensörüne öncelik ve garanti
```

## Implementation Notes

- Yayınlama **tek çağrıda** yapılmalı: tüm sensörler işlendikten sonra bir kez
  `publishSensors()`. Her sensör için ayrı yayınlama, versiyon sayacını gereksiz artırır ve
  tutarsız ara görüntüler üretir.
- Döngü süresi ölçülmeli; 250 ms periyotta örnekleme + işleme bütçeyi aşmamalı.
- Bir sensörün hata vermesi diğerlerinin okunmasını engellememeli.
- Task, config yüklenmeden örneklemeye başlamamalı (kalibrasyon gerekli) — boot
  senkronizasyonu EventGroup ile (§5).
- Sensör okuma sırasında ADC'ye başka task erişmemeli; `io_sense` ADC'nin tek sahibidir (§6.1).
- Watchdog beslemesi döngünün **en sonunda**.

## Files

- `src/services/SensorService.h` / `.cpp` (yeni)
- `src/tasks/SensorTask.cpp` (yeni)

## Acceptance Criteria

- [ ] Tüm kayıtlı sensörler periyoduna göre örnekleniyor
- [ ] Güvenlik sensörü hiçbir döngüde atlanmıyor
- [ ] Yayınlama tek çağrıda, tüm sensörler işlendikten sonra
- [ ] Servis ekrana çizmiyor, ağa dokunmuyor, aktüatör tetiklemiyor
- [ ] Bir sensörün hatası diğerlerini etkilemiyor
- [ ] Döngü süresi ölçüldü ve 250 ms bütçesine sığıyor
- [ ] Config yüklenmeden örnekleme başlamıyor
- [ ] Heartbeat ve watchdog doğru sırada

## Test Plan

- [ ] Tüm sensörler `StateStore`'da doğru periyotlarla güncelleniyor
- [ ] Bir sensör kasıtlı arızalandırıldığında diğerleri çalışmaya devam ediyor
- [ ] Döngü süresi en kötü durumda (tüm sensörler aynı anda) ölçüldü
- [ ] Güvenlik sensörünün gerçek örnekleme aralığı ölçüldü ve hedefte
- [ ] Uzun süreli çalışmada heartbeat kesintisiz
- [ ] Stack watermark ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.5, §9.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — döngü süresi bütçede mi
- [ ] Shared state güvenli mi? — **tek yazar kuralı**
- [ ] Memory problemi var mı? — stack watermark
- [ ] Error handling var mı? — tek sensör hatası izole mi
- [ ] ESP32 resource kullanımı uygun mu? — ADC tek sahipli mi
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`SensorValues()`'un OLED çizimi asla taşınmamalı**

## Definition of Done

Ortak DoD + döngü süresi ve güvenlik sensörü örnekleme aralığı ölçüldü + katman ihlali
olmadığı incelemeyle onaylandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Örnekleme zamanlaması: sıra tabanlı + güvenlik garantisi

```text
Problem:      Sensor periyotlari farkli (seviye 500 ms, akis/sicaklik 1 sn,
              pH/EC 2 sn). io_sense 250 ms'de bir caliyor.
Approaches:   (a) her donguda hepsini oku → gereksiz yuk; pH/EC problari
                  DC beslemede polarize olur, prob omru kisalir
              (b) her sensore "bir sonraki okuma zamani", sirayla
              (c) dongu basina en fazla N sensor (yuk dagitma)
Selected:     (b) + GUVENLIK GARANTISI
Kural:        `isSafetyCritical` isaretli sensorler (seviye, akis) periyodu
              geldiginde ASLA atlanmaz — yuk dagitma onlari erteleyemez.
Gerekce:      (c) tek basina secilseydi, yogun bir dongude guvenlik sensoru
              siraya girip gecikirdi. Guvenlik gecikmesi kabul edilemez.
```

## Karar 2 — Yayınlama TEK çağrıda, tüm sensörler işlendikten sonra

```text
Her sensor icin ayri `publishSensors()` cagirmak:
  · versiyon sayacini gereksiz artirir → web bosuna WS trafigi uretir
  · TUTARSIZ ARA GORUNTU yaratir: okuyucu seviyeyi yeni, akisi eski
    goruntuyle alabilir → guvenlik karari karisik veriyle verilir
Selected: tum sensorler islendikten sonra TEK `publishSensors()`.
```

## Karar 3 — Kesin yasaklar (ARCHITECTURE §2.5)

`SensorService` **hiçbir koşulda**: ekrana çizmez, ağa bağlanmaz, aktüatör
tetiklemez, karar vermez.

Mevcut sistemdeki `Sensor::SensorValues()` üçünü birden ihlal ediyordu —
sensör okuma ve OLED çizimi aynı fonksiyondaydı (REQUIREMENTS §6.3). Bu task
o desenin doğrudan karşıtıdır ve **tarama ile denetlenecektir**.

## Karar 4 — Bir sensörün hatası diğerlerini etkilemez

Her sensör kendi `try`siz hata yolunu döndürür (`RawSample.hardwareFault`).
Döngü hiçbir zaman erken çıkmaz; bir sensörün arızası kalan sensörlerin
okunmasını engellemez.

## Karar 5 — `SampleContext` sırası: sıcaklık EC'den ÖNCE

EC sıcaklık telafisi için o döngüdeki sıcaklık değerini ister. Bu yüzden
kayıt tablosunda su sıcaklığı EC'den önce gelir ve servis, bağlamı
sırayla doldurur. Sıcaklık o döngüde okunmadıysa **bir önceki geçerli**
değer kullanılır ve yaşı kontrol edilir.

## Karar 6 — Config yüklenmeden örnekleme yok

Kalibrasyon config'ten gelir. Config yüklenmeden örneklemeye başlamak,
`scale = 0` (memset'lenmiş) bir yapıyla her ölçümü sıfırlamak demektir.
Task boot Aşama 8'de oluşturulduğu için config zaten yüklüdür (Aşama 3);
servis yine de `begin()` içinde kontrol eder.

---

# STEP 3 — REVIEW RECORD

- [x] Tüm kayıtlı sensörler **kendi periyoduna göre** örnekleniyor
- [x] **Güvenlik sensörü hiçbir döngüde atlanmıyor** — yük dağıtma (döngü
      başına N sensör) bilinçli olarak KULLANILMADI
- [x] Yayınlama **tek çağrıda**, tüm sensörler işlendikten sonra
- [x] Servis ekrana çizmiyor, ağa dokunmuyor, aktüatör tetiklemiyor —
      **tarama ile doğrulandı, 0 ihlal**
- [x] Bir sensörün hatası diğerlerini etkilemiyor (döngü erken çıkmıyor)
- [x] Config yüklenmeden örnekleme başlamıyor
- [x] Heartbeat ve watchdog `TaskRunner` tarafından doğru sırada
- [x] ADC1 ve PCNT tek sahipli (`io_sense`)
- [ ] **Döngü süresi ölçümü, gerçek örnekleme aralıkları, stack watermark —
      donanım gerekiyor**

## Bulduğum hata: periyot mantığı çalışmıyordu

İlk yazımda:

```text
if (g_nextSampleAt[i].v != 0 && !hasElapsed(now, g_nextSampleAt[i], Duration{0}))
    continue;
```

`hasElapsed(..., Duration{0})` = `elapsed(...) >= 0` → **her zaman true**
(unsigned). `!true` = false → **hiçbir sensör atlanmıyordu**. Periyotlar
sessizce yok sayılıyor, her sensör her 250 ms'de okunuyordu.

Somut sonucu: pH/EC probları 8 kat fazla örneklenir; bu problar DC beslemede
polarize olur ve elektrot ömrü kısalır.

Kök neden **ISSUE-012'nin tam aynısı**: "bir sonraki okuma zamanı" zihinsel
modeli. Doğru desen SON okuma anını saklayıp geçen süreyi periyotla
karşılaştırmaktır. Düzeltildi ve kod içinde ISSUE-012'ye atıfla belgelendi.

Bu, ISSUE-012'de önerdiğim `operator+` kaldırma önerisinin neden önemli
olduğunun ikinci kanıtı.

## Statik denetimler

```text
§2.5 yasaklari (oled/wifi/relay/Serial)  : 0 ihlal
services/ → domain|interfaces|tasks      : 0
heap (new/malloc)                        : 0
matematiksel domain korumasi             : 9 nokta
```

**TASK-027: TAMAMLANDI** (donanım ölçümleri bekliyor).
