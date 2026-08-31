# TASK-022 — Sensor Model, Quality & Registry

**Phase:** 5 — Sensor System · **Priority:** P0

## Objective

Tüm sensörler için ortak ama **sığ** bir soyutlama kurmak; sensör değerinin kalite
bilgisiyle birlikte taşınmasını zorunlu kılmak.

## Scope

- `SensorReading` modeli: değer, birim, kalite, zaman damgası
- Kalite enum'u: `OK`, `STALE`, `OUT_OF_RANGE`, `FAULT`, `NOT_PRESENT`
- Sensör tanımlayıcısı (descriptor): kimlik, tip, birim, geçerli aralık, örnekleme periyodu
- Sensör kayıt tablosu (derleme zamanı sabit, dinamik kayıt yok)
- Minimal sensör arayüzü

## Out of Scope

- İşleme hattı (TASK-023)
- Somut sensör implementasyonları (TASK-024, 025, 026)
- SensorService task'ı (TASK-027)

## Dependencies

- TASK-004, TASK-006

## Requirements

- `REQUIREMENTS.md` — §3.7 (ortak sensör altyapısı yok), §3.1/3.2 (hata yönetimi yok)

## Architecture References

- §9.1 Ortak soyutlama gerekli mi (sığ soyutlama kararı)
- §9.5 Sensör hata yönetimi

## Expected Design

### Soyutlama derinliği — kesin sınır

`ARCHITECTURE.md` §9.1 kararı: **tek arayüz + derleme zamanı tablo.** Yasak olanlar:

- Derin sınıf hiyerarşisi
- Fabrika deseni, dinamik kayıt
- Heap üzerinde sensör nesneleri
- İkiden fazla sanal fonksiyon

Gerekçe: dört sensör aynı analog hattı paylaşıyor (kod tekrarını önlemek için soyutlama
gerekli), ancak sensör sayısı sabit ve az (derin soyutlama gereksiz).

### Kalite alanı — zorunlu

Değer **asla kalite bilgisi olmadan taşınmaz**. Bu, mevcut projedeki en tehlikeli
eksikliktir: `Sensor.WaterTemprature = 0` değerinin gerçek bir ölçüm mü yoksa kopuk sensör
mü olduğu ayırt edilemiyordu. Güvenlik kararı veren bir sensörde bu ayrım hayatidir.

| Kalite | Anlam | Otomasyonda kullanılır mı |
|---|---|---|
| `OK` | Geçerli ölçüm | Evet |
| `STALE` | Belirli süredir değişmiyor | Uyarıyla |
| `OUT_OF_RANGE` | Yapılandırılmış aralık dışında | **Hayır** |
| `FAULT` | Kopuk/kısa/okunamıyor | **Hayır** |
| `NOT_PRESENT` | Bu donanımda takılı değil | **Hayır** |

## Implementation Notes

- `NOT_PRESENT` kalitesi önemlidir: pH ve EC sensörleri başlangıçta takılı olmayabilir.
  Arayüz bunu "arıza" olarak değil "yok" olarak göstermelidir.
- Sensör kimliği kararlı olmalı (API ve config'te kullanılacak); sıralamaya bağlı indeks
  yerine açık enum tercih edilmeli.
- Birim bilgisi modelde taşınmalı ki UI ve API biçimlendirmeyi tahmin etmesin.
- Kayıt tablosu sensör eklemeyi kolaylaştırmalı: yeni sensör = tabloya bir satır + bir
  implementasyon.
- Zaman damgası monotonik olmalı (bayatlama hesabı için).

## Files

- `src/domain/models/SensorReading.h` (yeni)
- `src/services/sensors/SensorDescriptor.h` (yeni)
- `src/services/sensors/SensorRegistry.h` (yeni)
- `src/services/sensors/ISensor.h` (yeni)

## Acceptance Criteria

- [ ] `SensorReading` değer + kalite + birim + zaman damgası taşıyor
- [ ] Beş kalite durumu tanımlı
- [ ] Sensör arayüzü en fazla iki sanal fonksiyon içeriyor
- [ ] Kayıt tablosu derleme zamanı sabit; dinamik kayıt yok
- [ ] Heap kullanımı yok
- [ ] Sensör kimlikleri kararlı ve API'de kullanılabilir
- [ ] Yeni sensör eklemek tabloya satır + implementasyon ile mümkün

## Test Plan

- [ ] Host tarafında sahte sensörle kayıt tablosu doğrulandı
- [ ] Her kalite durumu üretilebiliyor ve doğru taşınıyor
- [ ] `SensorReading` boyutu ölçüldü (state içinde çoğaltılacak)
- [ ] Yeni sensör ekleme akışı bir sahte sensörle denendi

## Review Checklist

- [ ] Architecture'a uygun mu? (§9.1)
- [ ] Gereksiz abstraction var mı? — **bu task'ta en yüksek risk**
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — heap yok, boyut uygun
- [ ] Error handling var mı? — kalite modeli
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `SensorClass` global deseni taşınmamalı

## Definition of Done

Ortak DoD + soyutlama derinliği incelemede onaylandı + kalite alanının zorunlu olduğu
doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — ISSUE-010 uygulanıyor: yayınlanan tipler burada TANIMLANMIYOR

```text
`SensorQuality` ve `SensorSample` TASK-006'da `SystemState.h` icinde ZATEN
tanimli (yayinlanan state tipleri). Bu task onlari YENIDEN TANIMLAMAZ,
include eder.

Bu task'in tanimladiklari (CALISMA tipleri):
  · SensorDescriptor   — sensorun degismeyen ozellikleri
  · SensorRegistry     — derleme zamani tablo
  · ISensor            — minimal arayuz
  · SampleContext      — sensorler arasi baglam (asagida)
```

## Karar 2 — Birim `SensorSample`'da değil, `SensorDescriptor`'da

```text
Task scope'u "deger, birim, kalite, zaman damgasi" diyor.
Ancak BIRIM sensor basina SABITTIR — her ornekte tasimak israftir.

  SensorSample  → 8 slot × her ornekte tekrar → state buyur
  Descriptor    → sensor basina BIR kez

Selected: birim `SensorDescriptor`'da. `SensorSample` degismedi.
Kazanc:   `SystemState` 312 baytta kaldi; UI ve API birimi kimlikten
          bakarak bulur.
```

## Karar 3 — Soyutlama derinliği: **iki sanal fonksiyon, tablo, heap yok**

```text
ARCHITECTURE §9.1 karari: tek arayuz + derleme zamani tablo.

YASAK: derin sinif hiyerarsisi, fabrika deseni, dinamik kayit,
       heap uzerinde sensor nesneleri, ikiden fazla sanal fonksiyon.

ISensor:
   begin(const SensorConfig&) → ErrCode      (donanim hazirligi)
   sample(const SampleContext&) → RawSample  (ham fiziksel deger)
Toplam: 2 sanal fonksiyon.
```

## Karar 4 — Kalibrasyonun bölünmesi: sensöre özgü vs. genel

Bu ayrım TASK-023 ile TASK-024 arasındaki sınırı belirler ve netleştirilmezse
kalibrasyon iki yerde yapılır:

| Katman | Sorumluluk | Örnek |
|---|---|---|
| **Sensör** (TASK-024/025/026) | Ham okumayı FİZİKSEL BİRİME çevirir; sensöre özgü matematik | NTC Beta denklemi, pH 2 nokta eğrisi, darbe→L/dk |
| **Hat** (TASK-023) | Genel son ayar ve doğrulama | `offset`/`scale` trim, filtre, aralık, bayatlama, sıçrama |

Sensör "bu 23.4 °C" der; hat "bu değer güvenilir mi" der.

## Karar 5 — `SampleContext`: sensörler arası bağımlılık

```text
Problem:  EC olcumu SICAKLIGA guclu bagimlidir (TASK-024 sicaklik telafisi).
          Ama sensorler BIRBIRINI CAGIRAMAZ — esit seviyedeler ve
          birbirlerine bagimli olmalari dongusel bagimlilik yaratir.
Selected: `SampleContext` yapisi — SensorService o anki bilinen degerleri
          tasir ve `sample()` cagrisina verir.
          { waterTempC, waterTempValid }
Kazanc:   Bagimlilik ACIK ve TEK YONLU. EC sensoru sicakligi ISTEMEZ,
          verilen baglami KULLANIR. Sicaklik gecersizse EC bunu bilir ve
          kalitesini dusurur (TASK-024).
```

## Karar 6 — `NOT_PRESENT` gerçek bir durumdur

pH ve EC sensörleri sahada başlangıçta takılı olmayabilir. Arayüz bunu
**arıza olarak değil "yok" olarak** göstermelidir (ARCHITECTURE §9.3).
`SensorConfig.enabled == 0` → doğrudan `NOT_PRESENT`; donanım hiç okunmaz.

## Kapsam dışı

- İşleme hattı → TASK-023 · Somut sensörler → TASK-024/025/026
- `SensorService` ve task → TASK-027

---

# STEP 3 — REVIEW RECORD

- [x] `SensorSample` değer + kalite + zaman damgası taşıyor; **birim
      `SensorDescriptor`'da** (sensör başına sabit — her örnekte tekrarlamak
      state'i büyütürdü)
- [x] Beş kalite durumu `SystemState.h`'ta tanımlı; **burada yeniden
      tanımlanmadı** (ISSUE-010 kuralı uygulandı)
- [x] `ISensor` **tam olarak iki sanal fonksiyon** içeriyor
- [x] Kayıt tablosu derleme zamanı sabit; dinamik kayıt/fabrika/heap yok
- [x] Sensör kimlikleri kararlı (`core::SensorId`, API ve config'te kullanılır)
- [x] Yeni sensör = tabloya bir satır + bir implementasyon
- [x] `SensorDescriptor` = **8 bayt**
- [x] `static_assert`'ler güvenlik sensörlerinin işaretini ve seviye
      örnekleme periyodunu (≤ 500 ms) derleme zamanında zorluyor

**Gereksiz abstraction denetimi (bu task'ın en yüksek riski):** iki sanal
fonksiyon, düz `const` tablo, `SampleContext` için sade POD. Fabrika yok,
şablon yok, dinamik kayıt yok.

**TASK-022: TAMAMLANDI.**
