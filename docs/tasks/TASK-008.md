# TASK-008 — Command Model & CommandQueue

**Phase:** 1 — Core Infrastructure · **Priority:** P0

## Objective

Arayüzlerden (web, OLED) gelen niyet bildirimlerini domain katmanına güvenli şekilde
taşımak. AsyncTCP callback'i ile röle GPIO'su arasına **task sınırı** koymak.

## Scope

- `Command` veri modeli: tip, hedef, parametre, kaynak (WEB/UI/SYSTEM), istek kimliği
- Komut sonucu modeli (`ACCEPTED`, `REJECTED_SAFETY`, `DEFERRED_*`, `REJECTED_MODE`, `NO_CHANGE`)
- FreeRTOS kuyruğu sarmalayıcısı: `post()` (bloklamayan), `receive()`
- Kuyruk dolu politikası

## Out of Scope

- Komutların yürütülmesi (TASK-033 `app_core`)
- Web/UI tarafından komut üretimi (TASK-045, TASK-051)
- Yetkilendirme (TASK-042)

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — §5.4 (elle JSON parse), Kritik Problem 5

## Architecture References

- §2.2 CommandQueue modülü
- §3.3 Komut akış diyagramı
- §5 Neden queue
- §10.4 Komut sonucu sözleşmesi

## Expected Design

### Karar gerektiren nokta — Kuyruk dolduğunda davranış

```text
Problem:      Yavaş bir app_core veya komut seli kuyruğu doldurabilir
Constraints:  post() çağıranı bloklamamalı (AsyncTCP callback'inden çağrılıyor);
              güvenlik komutu (acil durdurma) kaybolmamalı
Approaches:   (a) yeni komutu reddet, çağırana false dön
              (b) en eskiyi düşür, yeniyi al
              (c) öncelikli kuyruk / ayrı acil durum kanalı
Trade-offs:   (b) kullanıcının ilk komutunu sessizce yutar — kabul edilemez
              (c) karmaşıklık ekler ama acil durdurmayı garantiler
Recommended:  (a) normal komutlar için + acil durdurma için ayrı garantili yol
              (bayrak veya ayrılmış slot)
```

- Komut yapısı **sabit boyutlu POD** olmalı; kuyruk statik ayrılmalı.
- `post()` **asla bloklamamalı** — zaman aşımı sıfır olmalı.
- `reqId` alanı web tarafındaki ack eşleştirmesi için zorunludur (§14.2).
- Komut tipleri kapalı bir enum olmalı; serbest metin komut yok.
- Acil durdurma komutunun kuyruk doluyken bile ulaşacağı yol tasarlanmalı.

## Implementation Notes

- Kuyruk boyutu ölçülü seçilmeli: çok küçük → komut kaybı, çok büyük → RAM israfı ve
  eskimiş komutların gecikmeli uygulanması. 8–16 aralığı başlangıç için makul.
- Komut **eskime kontrolü**: kuyrukta uzun süre bekleyen komut uygulanmadan önce
  zaman damgasına bakılmalı. 30 sn önce verilmiş bir "pompayı aç" komutu artık geçerli
  olmayabilir.
- Kaynak alanı (`source`) tahkim için gereklidir (§10.3): MANUAL komut ile AUTO kararı
  farklı işlenir.
- ISR'den `post()` çağrılacaksa ISR-güvenli varyant gerekir; encoder olayları için
  bu ayrı bir kuyruktur (TASK-021), buraya karıştırılmamalı.

## Files

- `src/core/Command.h` (yeni)
- `src/core/CommandQueue.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Komut modeli sabit boyutlu POD
- [ ] Komut sonucu enum'u §10.4 ile birebir uyumlu
- [ ] `post()` bloklamıyor; kuyruk doluyken açık sonuç döndürüyor
- [ ] Acil durdurma için garantili yol mevcut
- [ ] `reqId` alanı var ve ack eşleştirmesi için yeterli
- [ ] Kuyruk statik ayrılmış, dinamik bellek yok
- [ ] Komut eskime kontrolü için zaman damgası mevcut

## Test Plan

- [ ] Kuyruk doldurulduğunda `post()` bloklamadan sonuç dönüyor
- [ ] Çoklu yazar (2 task + callback) altında komut bozulması yok
- [ ] Acil durdurma komutu kuyruk doluyken de ulaşıyor
- [ ] Komut sırası korunuyor (FIFO)
- [ ] Eskimiş komut tespiti çalışıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.2, §5)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **`post()` kesinlikle bloklamamalı**
- [ ] Shared state güvenli mi? — kuyruk çok yazarlı
- [ ] Memory problemi var mı? — statik ayırma, kuyruk boyutu
- [ ] Error handling var mı? — dolu kuyruk, eskimiş komut
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — WS handler'ın doğrudan GPIO sürmesi taşınmamalı

## Definition of Done

Ortak DoD + çoklu yazar testi geçti + acil durdurma yolu donanımda doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Acil durdurmanın garantili yolu

```text
Problem:      Kuyruk doluyken acil durdurma komutu kaybolabilir.
Constraints:  post() bloklamamali (AsyncTCP callback'inden cagriliyor);
              acil durdurma ASLA kaybolmamali (ARCHITECTURE §12.3);
              normal komutlarin sessizce yutulmasi da kabul edilemez
Approaches:   (a) kuyrukta ayrilmis slot     → kuyruk mantigi karmasiklasir
              (b) ayri oncelikli kuyruk      → ikinci kuyruk, ikinci tuketim yolu
              (c) atomik bayrak + neden      → kuyruktan tamamen bagimsiz
Selected:     (c) — `postEmergencyStop()` atomik bir bayrak set eder.
              `app_core` her dongude ONCE bayragi kontrol eder, SONRA kuyrugu
              bosaltir. Kuyruk tamamen dolu olsa bile acil durdurma ulasir.
Gerekçe:      Kilitsiz, bloklamayan, ISR'den bile guvenli. Guvenlik yolunun
              kuyruk doluluguna bagimli olmamasi tasarim geregi.
```

Normal komutlar için politika: **kuyruk doluysa YENİ komut reddedilir**,
en eski düşürülmez. Gerekçe: en eskiyi düşürmek kullanıcının ilk komutunu
sessizce yutar — kabul edilemez. Çağıran `WEB_BUSY` alır ve kullanıcıya
"meşgul" bildirilir (ARCHITECTURE §2.2).

## Karar 2 — Komut sonucu enum'unun sahibi (ISSUE-010 uzantısı)

```text
Cakisma:  TASK-028 de "Komut sonucu enum'u (§10.4)" tanimlamayi planliyor.
Karar:    `CommandResult` BURADA tanimlanir (Command.h).
Gerekçe:  Sonuc, komut protokolunun yanit tipidir; kuyrugun tuketici sozlesmesi
          ve WebSocket ack'i (TASK-045) buna bagimli. TASK-028 include eder,
          yeniden tanimlamaz.
```

`ARCHITECTURE.md` §10.4'teki 6 sonuca iki tane ekleniyor ve gerekçesi:

| Sonuç | Kaynak |
|---|---|
| `ACCEPTED`, `REJECTED_SAFETY`, `DEFERRED_MIN_RUNTIME`, `DEFERRED_COOLDOWN`, `REJECTED_MODE`, `NO_CHANGE` | ARCHITECTURE §10.4 |
| `BUSY` | Kuyruk dolu — ARCHITECTURE §2.2 ve TASK-045 "BUSY ack" |
| `REJECTED_INVALID` | Şema doğrulaması başarısız — ARCHITECTURE §14.5 |

## Karar 3 — Komut eskimesi

```text
Problem:      Kuyrukta bekleyen komut uygulanmadan once gecerliligini yitirebilir.
Ornek:        30 sn once verilmis "pompayi ac" komutu artik istenmiyor olabilir;
              bu arada guvenlik durumu degismis olabilir.
Selected:     Her komut `issuedAt` (monotonik) tasir. Tuketici (TASK-033) yasi
              kontrol eder ve esigi asani DUSURUR — sessizce degil, loglayarak.
Not:          Esik degeri ve uygulama TASK-033'e ait; burada yalnizca alan ve
              `isStale()` yardimcisi saglanir.
```

## Karar 4 — Girdi olayları bu kuyruğa KARIŞMAZ

Encoder/buton olayları ayrı bir kuyrukta taşınır (TASK-021). Gerekçe: farklı
soyutlama seviyeleri. Girdi olayı ham donanım olayıdır; komut ise **niyet
bildirimidir**. `ui` task'ı girdi olayını alır, yorumlar ve gerekiyorsa komut
üretir. İkisini tek kuyrukta birleştirmek katman ihlali olur.

Bu nedenle `post()` **ISR-güvenli varyant sunmaz**: komutlar her zaman task
bağlamından gelir. (Acil durdurma bayrağı bunun istisnasıdır ve atomiktir.)

## Karar 5 — Komut tipi kümesi (P7 disiplini)

Komut tipleri **spekülatif olarak üretilmez**. Yalnızca `ARCHITECTURE.md`
§14.3 (API sözleşmesi), §10.3 (tahkim) ve §8.2 (ağ) içinde açıkça belgelenmiş
eylemler tanımlanır:

```text
§14.3 → aktuator komutu, emergency-stop, emergency-clear, restart,
        factory-reset, network scan, config guncelleme
§10.3 → otomasyon modu degistirme
§8.2  → credential unutma, "simdi tekrar dene"
```

## Bellek bütçesi

| Yapı | Hesap |
|---|---|
| `Command` | 16 bayt (hedef) |
| Kuyruk kapasitesi | 16 komut → 256 bayt |

Kapasite gerekçesi: çok küçük olursa komut kaybı, çok büyük olursa RAM israfı
ve eskimiş komut birikmesi. 16, web + OLED eşzamanlı kullanımda rahat pay bırakır.

## Kapsam dışı bırakılanlar

- Komutların yürütülmesi ve tahkim → TASK-033, TASK-029
- Web/UI'dan komut üretimi → TASK-045, TASK-053
- Yetkilendirme → TASK-042
- Eskime eşiği ve düşürme politikası → TASK-033

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Komut modeli sabit boyutlu POD — **16 bayt**, `is_trivially_copyable` doğrulandı
- [x] Komut sonucu enum'u §10.4 ile uyumlu (6 sonuç) + gerekçeli 2 ek (`BUSY`, `REJECTED_INVALID`)
- [x] `post()` bloklamıyor — `xQueueSend(..., 0)`, `portMAX_DELAY` kullanımı yok;
      kuyruk doluyken açık `BUSY` sonucu döndürüyor
- [x] **Acil durdurma için garantili yol mevcut** — kuyruktan bağımsız atomik bayrak
- [x] `reqId` alanı var (uint32, ack eşleştirmesi için yeterli)
- [x] Kuyruk **statik ayrılmış** (`xQueueCreateStatic`) — dinamik bellek yok
- [x] Komut eskime kontrolü — `issuedAt` (monotonik) + `isStale()` yardımcısı

## Ölçümler

| Yapı | Boyut |
|---|---|
| `Command` | 16 bayt |
| Kuyruk depolama | 16 × 16 = **256 bayt** (statik) |

Flash artışı: 267 301 → 267 437 bayt (**+136 bayt**).

## Statik denetimler

```text
xQueueSend    timeout = 0   → bloklamiyor
xQueueReceive timeout = 0   → bloklamiyor
portMAX_DELAY               → kullanilmiyor
new / malloc / xQueueCreate → yok (xQueueCreateStatic)
```

## Acil durdurma yolunun tasarımı

```text
postEmergencyStop():  neden + kaynak yazilir  →  EN SON bayrak set edilir
                      (memory_order_release)
takeEmergencyStop():  bayrak atomik exchange  →  neden okunur
                      (memory_order_acquire)
```

Sıralama bilinçli: okuyucu bayrağı gördüğünde neden alanları **kesinlikle
hazırdır**. Kilit alınmaz, bloklama yoktur, kuyruk doluluğundan bağımsızdır.

## Test Plan

- [x] Kuyruk dolu davranışı kod düzeyinde doğrulandı: `xQueueSend` timeout 0,
      dönüş `BUSY`, sayaç artıyor, en eski komut düşürülmüyor
- [x] Derleme SUCCESS, 0 uyarı
- [ ] **Kuyruk doldurma çalışma zamanı testi — donanım gerekiyor**
- [ ] **Çoklu yazar (2 task + callback) stres testi — donanım gerekiyor**
- [ ] **Acil durdurmanın kuyruk doluyken ulaştığı testi — donanım gerekiyor**
- [ ] **FIFO sırası doğrulaması — donanım gerekiyor**
- [ ] **Eskimiş komut tespiti — donanım gerekiyor** (TASK-033 tüketiciyi yazınca)

> Çalışma zamanı testleri fiziksel kart gerektiriyor. TASK-033 (tüketici) ve
> TASK-060 (entegrasyon) kapsamında donanımda kapatılacak.

## Review Checklist

- [x] Architecture'a uygun mu? — §2.2 modül sözleşmesi, §3.3 komut akışı,
      §10.4 sonuç sözleşmesi uygulandı
- [x] Gereksiz abstraction var mı? — şablon yok, sanal fonksiyon yok;
      FreeRTOS kuyruğunun ince sarmalayıcısı
- [x] **Blocking işlem var mı? — HAYIR.** İki kuyruk çağrısı da timeout 0;
      acil durdurma yolu tamamen kilitsiz
- [x] Shared state güvenli mi? — FreeRTOS kuyruğu kendi içinde thread-safe;
      acil durum bayrağı `std::atomic` ile release/acquire sıralamalı
- [x] Memory problemi var mı? — 256 B statik kuyruk + birkaç atomik; heap yok
- [x] Error handling var mı? — dolu kuyruk `BUSY` döndürüyor ve sayılıyor;
      başlatılmamış kuyruk `REJECTED_INVALID`; sessiz yutma yok
- [x] ESP32 resource kullanımı uygun mu? — statik kuyruk, atomik işlemler
- [x] Task sorumluluğu doğru mu? — çok yazar → tek okuyucu (`app_core`);
      girdi olayları bilinçli olarak **ayrı kuyrukta** (TASK-021)
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Mevcut sistemde
      WebSocket handler'ı AsyncTCP bağlamından doğrudan `digitalWrite()`
      çağırıyordu; bu task tam olarak o yolu kapatmak için var. Elle string
      parse (`indexOf("\"id\":")`) yerine tipli, kapalı bir komut enum'u kullanıldı.

## Bulgular

**ISSUE-010 genişletildi:** TASK-028 de "komut sonucu enum'u" tanımlamayı
planlıyordu. `CommandResult` burada tanımlandı çünkü komut protokolünün yanıt
tipidir ve hem kuyruk sözleşmesi hem WebSocket ack'i (TASK-045) ona bağımlı.
TASK-028 include edecek, yeniden tanımlamayacak.

## Durum

**TASK-008: TAMAMLANDI** (çalışma zamanı testleri donanım bekliyor).
