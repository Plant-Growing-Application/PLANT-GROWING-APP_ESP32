# TASK-004 — Core Types, Result & Error Model

**Phase:** 1 — Core Infrastructure · **Priority:** P0

## Objective

Tüm katmanların paylaşacağı temel veri tiplerini ve hata dönüş modelini tanımlamak.
Bu task'ın çıktısı, projedeki hemen her dosyanın bağımlı olacağı taban tiptir; bu yüzden
küçük, sabit ve dikkatli tasarlanmalıdır.

## Scope

- `Result` / durum dönüş tipi (istisna kullanılmadan hata taşıma)
- Hata kodu taksonomisi: `{subsystem, code}` çifti (ARCHITECTURE §16.2)
- Log seviyesi enum'u (`INFO`, `WARNING`, `ERROR`, `CRITICAL`)
- Zaman tipleri: monotonik `uptimeMs`, duvar saati `epoch`, ikisinin ayrımı
- Temel yardımcı tipler: sabit boyutlu string tamponu, aralık (min/max) tipi

## Out of Scope

- Diagnostics/log implementasyonu (TASK-005)
- SystemState alanları (TASK-006)
- Herhangi bir alt sistemin kendi enum'ları (kendi task'larında)

## Dependencies

- TASK-001

## Requirements

- `REQUIREMENTS.md` — §9 (hata yönetimi eksikliği), Kritik Problem 6

## Architecture References

- §16.1 Hata seviyeleri
- §16.2 Hata kodu yapısı
- §0 P7

## Expected Design

### Karar gerektiren nokta — Hata taşıma modeli

```text
Problem:      Hata bilgisi katmanlar arasında nasıl taşınacak?
Constraints:  C++ istisnaları ESP32'de kapalı/maliyetli; heap kullanımı istenmiyor;
              hata kodu makine tarafından karşılaştırılabilir olmalı
Approaches:   (a) bool dönüş + ayrı getLastError()   — thread-safe değil
              (b) enum hata kodu dönüşü              — değer + hata birlikte taşınamaz
              (c) Result<T> benzeri değer/hata birliği — küçük, POD, kopyalanabilir
              (d) çıkış parametresi + bool
Trade-offs:   (c) en açık sözleşmeyi verir; (b) en ucuzdur
Recommended:  Değer döndürmeyen işlemler için (b), değer döndürenler için (c)
```

**Kritik kural:** Hata kodu **makine tarafından** karşılaştırılır; serbest metin yalnızca
insan içindir. Karar mantığı asla metne bakmaz.

## Implementation Notes

- Tüm tipler POD olmalı: `memcpy` ile kopyalanabilir, dinamik ayırma içermez.
  Bunun nedeni TASK-007'deki snapshot deseninin bunu gerektirmesidir.
- `uptimeMs` (monotonik) ile `epoch` (duvar saati) **kesinlikle karıştırılmamalı**.
  Zaman aşımı ve süre ölçümleri her zaman monotonik zamanla yapılır; NTP senkronizasyonu
  duvar saatini geriye alabilir ve süre hesaplarını bozar.
- `millis()` taşması (~49.7 gün) süre karşılaştırmalarında doğru ele alınmalı
  (fark alma, doğrudan karşılaştırma değil). Sistem aylarca çalışacaktır.
- Sabit boyutlu string tipi seçilmeli; `String` sınıfı sıcak yolda ve state içinde
  kullanılmamalı (heap parçalanması).
- Alt sistem kimlikleri (SENSOR, NET, SAFETY, STORAGE, UI, WEB, SYS) burada tanımlanmalı.

## Files

- `src/core/Types.h` (yeni)
- `src/core/Result.h` (yeni)
- `src/core/ErrorCodes.h` (yeni)
- `src/core/Time.h` (yeni)

## Acceptance Criteria

- [ ] Hata dönüş modeli seçildi ve gerekçesi yazıldı
- [ ] Hata kodu taksonomisi `{subsystem, code}` olarak tanımlı ve genişletilebilir
- [ ] Log seviyeleri tanımlı
- [ ] Monotonik/duvar saati ayrımı tip düzeyinde açık
- [ ] Tüm tipler POD; dinamik ayırma yok
- [ ] `millis()` taşması güvenli süre karşılaştırması için yardımcı mevcut
- [ ] Kullanılmayan tip veya alan yok (P7)

## Test Plan

- [ ] Host tarafında derleme testi (donanım gerektirmeyen)
- [ ] Süre karşılaştırma yardımcısı taşma sınırında test edildi (`UINT32_MAX` civarı)
- [ ] `Result` tipinin boyutu ölçüldü ve makul (state içinde çoğaltılacak)
- [ ] POD olma özelliği derleme zamanı kontrolüyle doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§16)
- [ ] Gereksiz abstraction var mı? — taban tipler minimal mi
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? (POD/kopyalanabilirlik)
- [ ] Memory problemi var mı? — tip boyutları ölçüldü mü
- [ ] Error handling var mı? — modelin kendisi
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı?

## Definition of Done

Ortak DoD + tipler host tarafında derleniyor + boyutları ölçülüp kaydedilmiş.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Ortam kısıtı (ölçüldü)

`-std=gnu++11` — arduino-esp32 2.0.17. Sonuçları:

- `constexpr` fonksiyonlar **tek `return` ifadesi** olmak zorunda
- `if constexpr`, değişken şablonu, `std::optional` **yok**
- `static_assert` mesaj zorunlu
- `<type_traits>` mevcut (GCC 8.4) → POD doğrulaması derleme zamanında yapılabilir

## Karar 1 — Hata taşıma modeli

```text
Problem:      Hata bilgisi katmanlar arasında nasıl taşınacak?
Constraints:  İstisna yok (CODING_STANDARDS §4); heap yok; POD kalmalı
              (StateStore snapshot memcpy gerektiriyor — TASK-007)
Approaches:   (a) bool + getLastError()   → thread-safe değil, 5 task var
              (b) enum hata kodu dönüşü   → değer + hata birlikte taşınamaz
              (c) Result<T> değer/hata birliği
              (d) çıkış parametresi + bool → çağrı yeri okunaksız
Selected:     Değer döndürmeyen işlem  → (b) `ErrCode` doğrudan dönülür
              Değer döndüren işlem     → (c) `Result<T>`
Gerekçe:      Her iki durumda da sözleşme çağrı yerinde görünür; `Result<T>`
              POD kalır ve snapshot'a girebilir.
```

### `Result<T>` neden union kullanmıyor?

C++11'de union tabanlı bir varyant, non-trivial tipler için elle ctor/dtor yönetimi
gerektirir ve POD olma özelliğini kaybettirir. Bunun yerine düz `{ErrCode; T;}`
yapısı kullanılıyor:

- Küçük `T` (int, float, uint32) için birkaç baytlık israf **ihmal edilebilir**
- POD kalır → `memcpy` ile kopyalanabilir, snapshot'a girebilir
- `T`'nin trivially copyable olduğu `static_assert` ile zorlanır

## Karar 2 — Hata kodu taksonomisi

```text
Problem:      {subsystem, code} nasıl temsil edilecek?
Constraints:  Karar mantığı koda bakar, metne bakmaz (ARCHITECTURE §16.2);
              log kaydında saklanacak → küçük olmalı (TASK-005 halka tamponu);
              karşılaştırma ucuz olmalı (app_core 10 Hz'de çalışıyor)
Approaches:   (a) struct { Subsystem sub; uint8_t code; }  → 2 bayt, iki alan
              (b) düz enum, üst bayt alt sistemi kodlar    → 1 uint16, tek karşılaştırma
Selected:     (b) — `enum class ErrCode : uint16_t`, üst bayt = Subsystem.
              `subsystemOf(ErrCode)` ile alt sistem çıkarılır.
Gerekçe:      Tek tam sayı karşılaştırması; switch/case'te doğrudan kullanılabilir;
              log kaydında 2 bayt yer kaplar.
```

### Kapsam disiplini (P7)

Hata kodları **yalnızca `ARCHITECTURE.md` §16.3 arıza matrisinde ve
`REQUIREMENTS.md` §9'da belgelenmiş** arıza modları için tanımlanır.
Spekülatif kod üretilmez. Yeni kod, ona ihtiyaç duyan task tarafından eklenir.

## Karar 3 — Zaman tipleri: neden ayrı tipler

```text
Problem:      Monotonik süre ile duvar saati karışırsa ne olur?
Gerçek risk:  SNTP senkronizasyonu duvar saatini GERİYE alabilir.
              "Pompa 3 saattir çalışıyor" hesabı duvar saatiyle yapılırsa
              maxRunTime koruması (TASK-029) sessizce bozulur → donanım kaybı.
Constraints:  Tip düzeyinde ayrım isteniyor (task Acceptance Criteria);
              düz `typedef` ayrım sağlamaz — ikisi de uint32 olur ve karışır
Selected:     Üç ayrı POD sarmalayıcı yapı:
                Millis        — monotonik zaman damgası (uptime)
                Duration      — süre farkı
                EpochSeconds  — duvar saati
              Aralarında örtük dönüşüm YOK. Karıştırmak derleme hatası verir.
```

### Taşma güvenliği (Z5)

`millis()` ~49.7 günde taşar; sistem aylarca çalışacaktır. İki zaman damgası
**doğrudan karşılaştırılmaz**; her zaman fark alınır:

```text
YANLIŞ:  if (now.v > deadline.v)        → taşmada yanlış sonuç
DOĞRU:   if (elapsed(now, start) >= d)  → unsigned çıkarma doğal olarak sarar
```

Bu, `Millis` üzerinde `<`/`>` operatörlerinin **bilinçli olarak tanımlanmaması**
ile zorlanır — yanlış kullanım derlenmez.

## Karar 4 — Yardımcı tipler

| Tip | Neden gerekli | Kim kullanacak |
|---|---|---|
| `FixedString<N>` | `String` sınıfı heap kullanır; state/config POD olmalı | TASK-006 SystemState, TASK-014 Config |
| `Range<T>` | Aralık doğrulaması iki yerde tekrarlanacak | TASK-014 config doğrulama, TASK-023 sensör doğrulama |

İkisi de POD kalır; `static_assert` ile zorlanır.

## Karar 5 — Doğrulama yöntemi

```text
Problem:      "Host tarafında derleme testi" isteniyor ama native test ortamı
              TASK-064 scope'unda — bu task'ta kurulamaz
Selected:     Doğrulama `static_assert` ile header'ların İÇİNDE yapılır.
              POD olma, boyut sınırları ve taşma davranışı derleme zamanında
              zorlanır — hedef derlemede de host derlemesinde de geçerli.
Gerekçe:      BoardPins.h'de kanıtlanan desen: kısıt dokümantasyon değil,
              derleyici garantisi olur. TASK-064 bunun üstüne çalışma zamanı
              testleri ekleyecek.
```

## Kapsam dışı bırakılanlar

- `Diagnostics` implementasyonu → TASK-005
- `SystemState` alanları → TASK-006
- Alt sistemlerin kendi enum'ları (sensör kalitesi, aktüatör durumu vb.) → kendi task'ları

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Hata dönüş modeli seçildi ve gerekçesi yazıldı (STEP 1, Karar 1)
- [x] Hata kodu taksonomisi `{subsystem, code}` — `uint16` içinde kodlandı,
      `subsystemOf()` ile çıkarılıyor, `static_assert` ile doğrulandı
- [x] Log seviyeleri tanımlı — `INFO`/`WARNING`/`ERROR`/`CRITICAL`
- [x] **Monotonik/duvar saati ayrımı tip düzeyinde açık** — negatif testle kanıtlandı
- [x] Tüm tipler POD; dinamik ayırma yok — 8 adet `is_trivially_copyable` assert'i
- [x] `millis()` taşması güvenli yardımcı mevcut — `elapsed()` / `hasElapsed()`
- [x] Kullanılmayan tip veya alan yok — hata kodları yalnızca ARCHITECTURE §16.3
      matrisinde ve REQUIREMENTS §9'da belgelenmiş arıza modları için tanımlandı

## Test Plan

- [x] **Derleme testi** — hedef derleme SUCCESS, 0 uyarı.
      *Not:* native/host ortamı TASK-064 scope'unda. Doğrulama `static_assert`
      ile header'ların içinde yapıldı; bu kontroller host derlemesinde de aynen
      geçerlidir (donanıma bağlı değiller).
- [x] **Taşma sınırı testi — derleme zamanında kanıtlandı** (`Time.h`):

```text
elapsed(5, 0xFFFFFFFB)            == 10 ms    → tasma sinirinda dogru
elapsed(0, 0xFFFFFFFF)            ==  1 ms    → tam tasma noktasi
hasElapsed(5, 0xFFFFFFFB, 10ms)   == true     → zaman asimi dogru tetikleniyor
hasElapsed(5, 0xFFFFFFFB, 11ms)   == false    → erken tetiklenmiyor
```

- [x] **Boyutlar ölçüldü** (şablon örnekleme ile):

| Tip | Boyut |
|---|---|
| `Result<int32_t>` | **8 bayt** |
| `Result<float>` | **8 bayt** |
| `FixedString<32>` | **34 bayt** |
| `Millis` / `Duration` | 4 bayt |
| `EpochSeconds` | 8 bayt |

- [x] **POD doğrulaması derleme zamanı kontrolüyle yapıldı** — `Types.h`,
      `Result.h`, `Time.h` içinde toplam 8 `is_trivially_copyable` /
      `is_standard_layout` assert'i

### Negatif testler — tip ayrımı gerçekten zorlanıyor mu?

Tasarımın işe yaradığını kanıtlamak için iki kasıtlı hata denendi:

**1. İki zaman damgasını doğrudan karşılaştırma (taşma hatası kaynağı):**

```text
src/main.cpp:36: error: no match for 'operator<'
                 (operand types are 'core::Millis' and 'core::Millis')
```

**2. Monotonik zamanı duvar saatine atama (maxRunTime korumasını bozan hata):**

```text
src/main.cpp:36: error: conversion from 'core::Millis'
                 to non-scalar type 'core::EpochSeconds' requested
```

Her ikisi de derlenmiyor. `CODING_STANDARDS` Z4 ve Z5 kuralları artık
dokümantasyon değil, **derleyici garantisi**.

## Review Checklist

- [x] Architecture'a uygun mu? — §16.1 seviyeler, §16.2 `{subsystem, code}` yapısı
      birebir uygulandı
- [x] Gereksiz abstraction var mı? — 4 header, 7 tip. `Status` alias'ı bilinçli
      olarak **eklenmedi** (ErrCode'un eşanlamlısı olurdu, P7). Union tabanlı
      varyant reddedildi (C++11'de POD'luğu bozardı).
- [x] Blocking işlem var mı? — N/A (yalnızca tip tanımları)
- [x] Shared state güvenli mi? — POD/kopyalanabilirlik `static_assert` ile
      zorlandı; StateStore snapshot deseninin ön koşulu sağlandı
- [x] Memory problemi var mı? — boyutlar ölçüldü, heap kullanımı yok,
      `String` sınıfı kullanılmadı
- [x] Error handling var mı? — modelin kendisi bu task
- [x] ESP32 resource kullanımı uygun mu? — C++11 kısıtına uyuldu, `<type_traits>`
      dışında bağımlılık yok
- [x] Task sorumluluğu doğru mu? — N/A
- [x] Eski kod gereksiz şekilde kopyalanmış mı? — **hayır.** Eski projede hata
      modeli, Result tipi veya zaman tipi soyutlaması hiç yoktu; sıfırdan tasarım.

## Bulgular

Header-only bir task olduğu için `static_assert`'ler ancak bir `.cpp` tarafından
include edildiğinde çalışır. Doğrulama geçici include ile yapıldı ve geri alındı.
**Kalıcı derleme kapsaması TASK-005'in `Diagnostics.cpp` dosyasından gelecek**
(o dosya `Types.h` ve `ErrorCodes.h` include eder). Ayrı bir issue açılmadı:
boşluk aynı batch içinde kapanıyor.

## Durum

**TASK-004: TAMAMLANDI.**
