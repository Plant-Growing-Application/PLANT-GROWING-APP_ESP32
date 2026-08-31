# TASK-021 — Input Devices (Encoder & Buttons)

**Phase:** 4 — Hardware Abstraction · **Priority:** P2

## Objective

Rotary encoder ve butonlardan **ham olay** üretmek. Dekodlama ve yorumlama işini ISR'den
task bağlamına taşımak.

## Scope

- Quadrature encoder dekodlama ve detent normalizasyonu
- Buton debounce, kısa/uzun basış ayrımı
- ISR → olay kuyruğu köprüsü
- Detent adım oranının yapılandırılabilir olması

## Out of Scope

- Navigasyon mantığı ve menü davranışı (TASK-051)
- Komut üretimi (TASK-053)
- Ekran çizimi

## Dependencies

- TASK-004, TASK-008

## Requirements

- `REQUIREMENTS.md` — §6.1 (encoder, butonlar), §12 (detent oranı belirsizliği)

## Architecture References

- §2.15 RotaryEncoder, ButtonInput · §13.4 Girdi işleme

## Expected Design

### Karar gerektiren nokta 1 — Encoder dekodlama yeri

```text
Problem:      Quadrature dekodlama nerede yapılacak?
Constraints:  ISR kısa olmalı; kaçırılan geçiş yön hatası üretir;
              ui task'ı 50 ms'de bir çalışıyor
Approaches:   (a) ISR'de tam dekodlama + sayaç  (mevcut projenin yöntemi)
              (b) ISR'de yalnızca pin durumu → kuyruk, dekodlama task'ta
              (c) donanımsal PCNT'nin quadrature modu
Trade-offs:   (a) ISR uzun; (b) kuyruk taşması hızlı çevirmede olay kaybettirir;
              (c) donanım destekli, CPU'suz — ancak PCNT birimi akış sensörüyle paylaşılır
Recommended:  Geliştirici seçer; (c) mümkünse tercih edilmeli, PCNT birim bütçesi
              kontrol edilerek
```

### Karar gerektiren nokta 2 — Detent normalizasyonu

Mevcut kodda `stepsPerDetent = 1.5` (`double`) değeri `int` sayaçla karşılaştırılıyor —
tanımsız davranışa yakın bir desen. Yeni tasarımda:

- Değer **tamsayı** olmalı (tipik encoder'larda detent başına 4 geçiş)
- Yapılandırılabilir olmalı, koda gömülmemeli
- Sahada gerçek encoder ile doğrulanmalı

## Implementation Notes

- ISR fonksiyonları `IRAM_ATTR` olmalı; içlerinde log, dinamik ayırma veya bloklama yasak.
- Buton kuyruğu encoder kuyruğundan ayrı ya da tek bir girdi olay kuyruğu olabilir; komut
  kuyruğuyla (TASK-008) **karıştırılmamalı** — farklı soyutlama seviyeleri.
- Uzun basış süresi yapılandırılabilir olmalı; acil durdurma için uzun basış kullanılacaksa
  (§13.3) yanlışlıkla tetiklenmeyecek kadar uzun seçilmeli.
- Mevcut kodda `PIN_CONFIRM_BUTTON` tanımlanmış ama hiç okunmuyordu. Yeni tasarımda ya
  kullanılmalı ya da tanımdan çıkarılmalı (P7).
- Encoder pinleri ADC olmayan pinlere taşınmış olmalı (TASK-002, ISSUE-001).
- Debounce süresi ölçümle belirlenmeli; encoder kalitesine göre değişir.

## Files

- `src/hal/RotaryEncoder.h` / `.cpp` (yeni)
- `src/hal/ButtonInput.h` / `.cpp` (yeni)
- `src/hal/InputEvents.h` (yeni — olay tipleri)

## Acceptance Criteria

- [ ] Encoder yön ve adım tespiti güvenilir
- [ ] Detent oranı tamsayı ve yapılandırılabilir
- [ ] Buton debounce çalışıyor; kısa/uzun basış ayrımı var
- [ ] ISR'ler kısa; log/alloc/bloklama içermiyor
- [ ] Olaylar kuyruk üzerinden task bağlamına taşınıyor
- [ ] Girdi kuyruğu komut kuyruğundan ayrı
- [ ] Kullanılmayan buton pini ya kullanılıyor ya kaldırıldı
- [ ] Encoder pinleri ADC olmayan pinlerde

## Test Plan

- [ ] Yavaş çevirmede her detent tam bir adım üretiyor
- [ ] Hızlı çevirmede yön hatası ve adım kaybı yok
- [ ] Yön değiştirmede sıçrama yok
- [ ] Buton zıplaması (bounce) tek olay üretiyor
- [ ] Uzun basış eşiği doğru tetikleniyor, kısa basışla karışmıyor
- [ ] Kuyruk taşması senaryosunda çökme yok
- [ ] ISR süresi ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§13.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — ISR içinde
- [ ] Shared state güvenli mi? — ISR ile task arası veri paylaşımı
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — kuyruk taşması
- [ ] ESP32 resource kullanımı uygun mu? — `IRAM_ATTR`, PCNT birim bütçesi
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`1.5` detent oranı ve ISR'de tam dekodlama sorgulanmalı**

## Definition of Done

Ortak DoD + gerçek encoder ile hızlı/yavaş çevirme testleri geçti + detent oranı sahada
doğrulandı.

---

# STEP 1+3 — DESIGN & REVIEW RECORD

## Karar 1 — Encoder dekodlama: ISR + geçiş TABLOSU

```text
Approaches:  (a) ISR'de tam dekodlama (mevcut sistem: uzun if/else zinciri)
             (b) ISR ham pin durumu → kuyruk, dekodlama task'ta
             (c) PCNT quadrature modu
Selected:    (a) — ANCAK gecis TABLOSU ile.
Gerekce:     · Tablo sabit sureli ve ~10 komut — ISR'de olmasi gereken budur.
               Mevcut sistemin if/else zinciri uzun ve dallanmaliydi.
             · (b) hizli cevirmede kuyruk tasmasi riski tasir
             · (c) PCNT birimi akis sensorune ayrildi (TASK-019); encoder
               guvenlik-kritik degil, ikinci birimi burada harcamak
               gereksiz karmasiklik olurdu
```

## Karar 2 — Detent oranı TAMSAYI ve yapılandırılabilir

Mevcut sistemde `stepsPerDetent = 1.5` (`double`) bir `int` sayaçla
karşılaştırılıyordu — tanımsız davranışa yakın bir desen (REQUIREMENTS §12).
Burada `uint8_t`, varsayılan 4 (tipik encoder), `begin()` parametresi.
**Sahada doğrulanması gerekiyor.**

## Karar 3 — Butonlar ISR'de değil, `poll()` içinde örnekleniyor

Debounce ve kısa/uzun basış ayrımı **zaman** gerektirir; bunu ISR'de yapmak
CODING_STANDARDS §6'yı ihlal ederdi. Encoder ISR'de (hızlı, zamansız),
butonlar task bağlamında (yavaş, zamanlı) — doğru ayrım.

## Karar 4 — Kullanılmayan buton pini kaldırıldı

Mevcut sistemde `PIN_CONFIRM_BUTTON` (GPIO 26) `pinMode` yapılıyor ama **hiç
okunmuyordu** (REQUIREMENTS §6.1). Yeni tasarımda iki buton var: encoder push
(seç) ve geri. Confirm gereksizdi ve pin serbest bırakıldı (P7, TASK-002).

## Review

- [x] Encoder yön/adım tespiti geçiş tablosuyla; geçersiz geçişler yok sayılıyor
- [x] Detent oranı **tamsayı** ve yapılandırılabilir
- [x] Buton debounce + kısa/uzun basış ayrımı
- [x] **ISR'de log/alloc/bloklama YOK** — tarama ile doğrulandı
- [x] Olaylar kuyruk üzerinden task bağlamına taşınıyor
- [x] Girdi kuyruğu **komut kuyruğundan ayrı** (ARCHITECTURE §5)
- [x] Kuyruk taşması sayılıyor (`droppedEvents`) — sessiz kayıp yok
- [x] Encoder pinleri ADC olmayan pinlerde (GPIO 18/19)
- [x] Derleme SUCCESS, 0 uyarı
- [ ] **Hızlı/yavaş çevirme, yön hatası, zıplama, ISR süresi, detent oranının
      sahada doğrulanması — donanım gerekiyor**

**TASK-021: TAMAMLANDI** (donanım testleri bekliyor).

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

> **Protokol notu:** Geriye dönük kayıt (bkz. TASK-018).

## Karar 1 — ISR YALNIZCA OLAY ÜRETİR

Debounce yorumlama, detent normalizasyonu ve **tüm karar mantığı** task
bağlamındadır (CODING_STANDARDS §6). ISR'de karar vermek hem jitter üretir
hem test edilemez.

## Karar 2 — GİRDİ OLAYLARI KOMUT KUYRUĞUNA KARIŞMAZ (§5)

```text
Girdi olayi = ham donanim olayi  ("encoder bir detent dondu")
Komut       = niyet bildirimi     ("su pompasini ac")

`ui` task'i girdiyi alir, YORUMLAR ve gerekiyorsa komut uretir.
```

İkisini tek kuyrukta taşımak, ekran gezinmesinin komut kuyruğunu doldurması
demek olurdu.

## Karar 3 — DETENT ORANI TAMSAYIDIR

```text
Eski sistem: `stepsPerDetent = 1.5` (double) bir `int` sayacla
             karsilastiriliyordu — tanimsiz davranisa yakin bir desen
             (REQUIREMENTS §12).
Yeni:        `uint8_t stepsPerDetent`, varsayilan 4 (tam quadrature).
```

TASK-053 sonrası encoder oranı ayarlandı (`refactor(ui): adjust encoder
detent steps calculation` commit'i).

## İnceleme

- [x] ISR yalnızca kuyruğa koyuyor
- [x] Girdi kuyruğu komut kuyruğundan ayrı
- [x] Detent oranı tamsayı
- [x] Kuyruk taşması sayılıyor (`droppedEvents()`)
- [x] Uzun basış ve debounce sabitleri tanımlı
- [ ] **Gerçek encoder davranışı ölçülmedi**
