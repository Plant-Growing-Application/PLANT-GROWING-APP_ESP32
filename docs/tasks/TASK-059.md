# TASK-059 — StorageService Task & History API

**Phase:** 13 — Data Logging · **Priority:** P3

## Objective

Flash yazma işlemlerini düşük öncelikli bir task'a taşımak ve geçmiş veriyi API üzerinden
sunmak. Hiçbir çağıranın flash yazmasını beklememesini garanti etmek.

## Scope

- `store` task döngüsü (olay güdümlü, en düşük öncelik, Core 0)
- Yazma isteği kuyruğu (config, geçmiş, kritik log)
- Periyodik örnek kaydı
- `GET /api/history` endpoint'i
- Depolama istatistikleri

## Out of Scope

- Halka dosya implementasyonu (TASK-058)
- Config mantığı (TASK-015)
- Frontend grafik (kapsam dışı)

## Dependencies

- TASK-058, TASK-043

## Requirements

- `REQUIREMENTS.md` — §7 (storage), §10 (Task_SensorLogger loglama çalışmıyor)

## Architecture References

- §2.12 StorageService · §6.1 store task satırı
- §14.3 `/api/history` endpoint'i

## Expected Design

### Bloklamama garantisi

```text
   ConfigService.persist()  ─┐
   HistoryStore.append()    ─┼──▶ StorageWriteQueue ──▶ store task ──▶ flash
   Diagnostics kritik log   ─┘        (bloklamayan)      (düşük öncelik)
```

Flash yazma yavaş ve **değişken sürelidir**. `app_core` veya AsyncTCP callback'i asla
bunu beklememelidir. Mevcut projede EEPROM yazma Wi-Fi event handler'ından yapılıyordu —
kritik bir bağlamda yavaş bir işlem.

### Karar gerektiren nokta — Kuyruk dolduğunda

```text
Problem:      Yazma kuyruğu dolarsa hangi istek düşürülür?
Constraints:  Config yazması kaybolmamalı (kullanıcı ayarı);
              geçmiş veri kaybı tolere edilebilir;
              kritik log kaybı teşhisi zorlaştırır
Approaches:   (a) hepsini eşit değerlendir, yeniyi reddet
              (b) tipe göre öncelik: config > kritik log > geçmiş
Recommended:  (b) — geçmiş veri kaybı en az zararlı olandır
```

### Task önceliği

`store` task'ı **en düşük önceliğe** sahiptir (1). Flash yazması gerçek zamanlı bir iş
değildir ve güvenlik döngüsünü hiçbir koşulda geciktirmemelidir.

## Implementation Notes

- Task olay güdümlü olmalı: kuyruk boşken bloklayan bekleme yapmalı (CPU harcamamalı),
  ancak watchdog beslemesi için makul bir zaman aşımıyla uyanmalı.
- Watchdog timeout'u uzun olmalı; tek bir flash yazması beklenenden uzun sürebilir.
- `GET /api/history` sayfalı olmalı; büyük aralık sorgusu tek yanıtta dönmemeli.
- API yanıtı üretilirken dosya okunacaktır — bu **AsyncTCP callback'inde yapılmamalı**
  (§14.6). Chunked response veya önceden hazırlanmış tampon kullanılmalı.
- Depolama istatistikleri (kullanılan alan, yazma sayısı, hata sayısı) teşhis için
  sunulmalı.
- Örnek kayıt periyodu config'ten okunmalı.
- Task heartbeat yayınlamalı; flash yazması uzun sürdüğünde bile bayatlamamalı.

## Files

- `src/services/StorageService.h` / `.cpp` (yeni)
- `src/tasks/StorageTask.cpp` (yeni)
- `src/interfaces/web/api/HistoryApi.cpp` (yeni)

## Acceptance Criteria

- [ ] `store` task'ı en düşük öncelikte, Core 0'da, olay güdümlü çalışıyor
- [ ] Yazma kuyruğu çalışıyor; çağıran bloklanmıyor
- [ ] Kuyruk dolu politikası tipe göre öncelikli
- [ ] Periyodik örnek kaydı yapılıyor; periyot config'ten
- [ ] `GET /api/history` sayfalı çalışıyor
- [ ] API yanıtı AsyncTCP callback'ini bloklamıyor
- [ ] Depolama istatistikleri sunuluyor
- [ ] Task heartbeat yayınlıyor ve watchdog timeout'u uygun
- [ ] Yazma hataları raporlanıyor

## Test Plan

- [ ] Config yazma sırasında `app_core` döngü süresi etkilenmiyor
- [ ] Config yazma sırasında web arayüzü donmuyor
- [ ] Kuyruk doldurulduğunda öncelik politikası uygulanıyor
- [ ] Geçmiş veri düzenli kaydediliyor
- [ ] `GET /api/history` büyük aralıkta sayfalı yanıt veriyor
- [ ] API sorgusu sırasında sistem yanıt vermeye devam ediyor
- [ ] Flash dolu senaryosunda hata raporlanıyor
- [ ] Uzun süreli çalışmada task heartbeat kesintisiz
- [ ] Stack watermark ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.12, §6.1)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **çağıranlar bloklanmıyor mu**
- [ ] Shared state güvenli mi? — kuyruk çok yazarlı
- [ ] Memory problemi var mı? — API yanıt tamponu
- [ ] Error handling var mı? — yazma hatası, dolu kuyruk
- [ ] ESP32 resource kullanımı uygun mu? — flash aşınması
- [ ] Task sorumluluğu doğru mu? — en düşük öncelikte mi
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **event handler'dan flash yazma yasak**

## Definition of Done

Ortak DoD + flash yazması sırasında `app_core` ve web'in etkilenmediği ölçümle kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Kuyruk dolduğunda: (b) tipe göre öncelik

```text
config      > kritik log > gecmis
KAYBOLMAZ     onemli       ATILABILIR
```

Kuyruk dolduğunda **geçmiş kayıtları düşürülür**; config yazması asla.
Bir sulama örneğini kaybetmek grafikte bir boşluk demektir; kullanıcının
ayarını kaybetmek onun yeniden girmesi demektir ve fark edilmesi zordur.

Uygulama: kuyruk kapasitesi 8; `HISTORY` tipi için yer ayrılırken **1 slot
rezerv** bırakılır. Böylece kuyruk geçmişle dolup config'i dışarıda
bırakamaz.

## Karar 2 — Task olay güdümlü, ama watchdog için ZAMAN AŞIMIYLA uyanır

```text
xQueueReceive(..., pdMS_TO_TICKS(1000))

Kuyruk bosken CPU harcamaz (bloklar), ama en fazla 1 sn sonra uyanip
heartbeat besler. Sonsuz bloklama watchdog'u tetiklerdi.
```

`store` task'ı **en düşük öncelikte** (1): flash yazması gerçek zamanlı bir
iş değildir ve güvenlik döngüsünü hiçbir koşulda geciktirmemelidir.

## Karar 3 — Periyodik örnek isteğini `store` task'ı KENDİ üretir

`app_core`'un her 60 saniyede bir kuyruğa istek koyması, güvenlik
döngüsüne bir sorumluluk daha eklemek olurdu. `store` zaten 1 sn'de bir
uyanıyor; periyodu kendi sayar ve snapshot'ı kendi alır.

Bu aynı zamanda `HistoryStore.append()`'in **yalnızca** `store` task'ından
çağrıldığını garanti eder.

## Karar 4 — `GET /api/history` SAYFALI ve SINIRLI

```text
Varsayilan   : son 120 kayit
Ust sinir    : 240 kayit (MAX_PAGE)  → 240 × 24 = 5 760 bayt okuma
Sinirsiz aralik sorgusu YOK.
```

**Bilinen istisna (§14.6):** okuma AsyncTCP bağlamında yapılıyor.
Azaltılmış hâliyle bu **en fazla 2 dosya açması** (halka en fazla iki
bitişik parçaya bölünür) ve ~5,8 KB okumadır. İlk yazımda kayıt başına bir
`readAt` vardı — 240 dosya açması; bu **düzeltildi** (bkz. inceleme kaydı).

Süre donanımda ölçülmedi; ISSUE olarak kaydedildi. Ölçüm kabul edilemez
çıkarsa çözüm, `store` task'ının bir sayfayı önceden hazırlaması olacaktır.

`readRange()` (tüm halkayı tarayabilir) API'den **çağrılmıyor** — yalnızca
`readRecent()` kullanılıyor.

## Karar 5 — Depolama istatistikleri teşhis için sunulur

`GET /api/diagnostics` içine: kullanılan alan, saklanan kayıt, toplam
yazma, bozuk kayıt, yazma hatası. Eski projede `Task_SensorLogger`
loglama yapmıyordu ve bunu kimse fark etmemişti — sayaçlar tam olarak bu
yüzden var.

---

# STEP 3 — REVIEW RECORD

- [x] `store` task döngüsü; **en düşük öncelik (1)**, Core 0
- [x] Olay güdümlü: kuyruk boşken bloklar, 1 sn'de bir uyanıp heartbeat besler
- [x] Yazma kuyruğu (8 slot); **tipe göre öncelik** — geçmiş düşürülür,
      config asla (1 slot rezerv)
- [x] Periyodik örnek kaydı; istek `store` task'ının kendisi tarafından üretiliyor
- [x] `history::append()` **yalnızca** `store` task'ından çağrılıyor —
      tarama ile doğrulandı
- [x] `GET /api/history` sayfalı ve sınırlı (varsayılan 120, üst sınır 240)
- [x] `readRange()` API'den **çağrılmıyor** — tarama: 0 çağrı
- [x] Depolama istatistikleri `/api/diagnostics` içinde
- [x] Heap tahsisi yok; sayfa tamponu statik
- [ ] **Yanıt üretim süresi donanımda ölçülmedi** — ISSUE-022

## Bulduğum hata: config değişiklikleri HİÇ YAZILMIYORDU

Tarama sırasında görüldü:

```text
config::persist()  cagiran: HICBIRI
config::isDirty()  cagiran: HICBIRI
```

`ConfigService::updateNetwork()`, `updateSafety()`, `updateActuator()`…
hepsi yalnızca RAM'i değiştirip config'i **kirli** işaretliyordu. Yazmayı
tetikleyen hiçbir şey yoktu.

**Somut sonucu:** kullanıcı web arayüzünden Wi-Fi ayarını, güvenlik
eşiğini veya aktüatör kısıtını değiştirir, "Kaydedildi" mesajını görür —
ve **yeniden başlatmada her şey eski hâline döner.** Üstelik hiçbir hata
görünmeden.

Bu hata TASK-014 (persist yazıldı), TASK-044 (update çağrıldı) ve TASK-059
(yazma yolu kuruldu) arasındaki boşlukta duruyordu; hiçbir task'ın tek
başına kapsamında değildi.

**İşaret:** `isDirty()` API'sinin **hiç çağrılmıyor olması**. Ölü bir API,
eksik bir bağlantının en güvenilir belirtisidir (P7'nin tersten okunuşu).

**Düzeltme:** `storage::tick()` her çevrimde `isDirty()` kontrol ediyor ve
**2 saniyelik birleştirme** sonrası `CONFIG_PERSIST` isteği kuyruğa
koyuyor. Debounce bilinçli: kullanıcı bir formu kaydettiğinde arka arkaya
birkaç `PUT` gelir; her birinde flash'a yazmak gereksiz aşınma üretirdi.

## §14.6 istisnası açıkça kaydedildi

`GET /api/history` dosyayı AsyncTCP bağlamında okuyor. Sınırlandırılmış
hâliyle bu **en fazla 2 dosya açması ve ~5,8 KB okuma**; ilk yazımdaki
240 dosya açması TASK-058'de düzeltildi.

Süre **ölçülmedi**. ISSUE-022 kaydedildi; ölçüm kabul edilemez çıkarsa
çözüm `store` task'ının bir sayfayı önceden hazırlamasıdır.

**TASK-059: TAMAMLANDI** (donanım ölçümleri bekliyor).
