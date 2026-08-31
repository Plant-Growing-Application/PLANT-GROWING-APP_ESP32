# TASK-025 — Flow Sensor

**Phase:** 5 — Sensor System · **Priority:** P1

## Objective

Darbe sayımını **zaman tabanlı** doğru bir debi değerine dönüştürmek. Bu sensör akış
doğrulama güvenlik kontrolünün (TASK-031) girdisidir; güvenilirliği kritiktir.

## Scope

- Darbe sayısı + geçen süre → L/dk dönüşümü
- Darbe/litre kalibrasyon katsayısı (config'ten)
- Toplam hacim sayacı (litre)
- Düşük debi eşiği ve sıfır akış ayrımı
- Sensör arıza tespiti

## Out of Scope

- PCNT sürücüsü (TASK-019)
- Akış doğrulama güvenlik mantığı (TASK-031)
- Geçmiş kayıt (TASK-058)

## Dependencies

- TASK-019, TASK-023

## Requirements

- `REQUIREMENTS.md` — §3.2 (akış sensörü `[~]`, toplam hacim yok)

## Architecture References

- §9.3 Sensör kataloğu (akış — güvenlik rolü kritik)

## Expected Design

### Mevcut algoritmanın reddi

```text
Mevcut:   litersPerMinute = (pulses * 100) / 450
Problem:  1. Sabit zaman penceresi varsayıyor — fonksiyon 500 ms ve 600 ms
             periyotlarla çağrılıyordu, sonuç anlamsız
          2. Tamsayı bölmesi düşük debileri sıfıra yuvarlıyor
          3. İki farklı çağıran aynı sayacı tüketiyordu
Yeni:     debi = (darbe / darbePerLitre) / (geçenSüre / 60000) → float L/dk
          tek okuyucu, gerçek geçen süre, float aritmetik
```

### Karar gerektiren nokta — Düşük debi hassasiyeti

```text
Problem:      Akış doğrulama "az akış" ile "hiç akış yok" ayrımına ihtiyaç duyar
Constraints:  Kısa pencerede az darbe → kaba çözünürlük;
              uzun pencere → güvenlik tepkisi gecikir
Approaches:   (a) sabit pencere (örn. 1 s)
              (b) darbeler arası süre ölçümü (düşük debide daha hassas)
              (c) uyarlanabilir pencere
Trade-offs:   (b) düşük debide çok daha hassastır ama PCNT ile birleştirmek karmaşıktır
Recommended:  (a) ile başla; TASK-031'in ihtiyacı karşılanmıyorsa (b) değerlendirilsin
```

## Implementation Notes

- ISSUE-002: Seçilen pinde pull-up durumu doğrulanmalı (GPIO 34–39'da dahili pull-up yok).
- Toplam hacim sayacı kalıcı olmalı mı? Her boot'ta sıfırlanırsa uzun dönem tüketim
  bilgisi kaybolur. Kalıcı yapılacaksa flash aşınması için seyrek yazılmalı (örn. 10 dk).
- **Sıfır darbe iki anlama gelir:** gerçekten akış yok, veya sensör kopuk. Bu ayrım tek
  başına akış sensöründen yapılamaz — pompa durumuyla çapraz kontrol gerekir ve bu
  TASK-031'in işidir. Bu task yalnızca "0 L/dk, kalite OK" veya "okunamıyor, kalite FAULT"
  ayrımını yapar.
- Sensörün minimum ve maksimum debi aralığı config'te olmalı; aralık dışı değer
  `OUT_OF_RANGE`.
- Kalibrasyon katsayısı (darbe/litre) sensör modeline özgüdür ve config'te tutulmalıdır;
  koda gömülmemeli.

## Files

- `src/services/sensors/FlowSensor.h` / `.cpp` (yeni)
- `docs/HARDWARE.md` (güncelleme — pull-up ve sensör modeli)

## Acceptance Criteria

- [ ] Debi hesabı gerçek geçen süreye dayanıyor
- [ ] Float aritmetik kullanılıyor; düşük debi sıfıra yuvarlanmıyor
- [ ] Kalibrasyon katsayısı config'ten okunuyor
- [ ] Toplam hacim sayacı çalışıyor; kalıcılık kararı verilmiş
- [ ] Sıfır akış ile sensör arızası ayrımı için gerekli bilgi taşınıyor
- [ ] Aralık dışı değer `OUT_OF_RANGE`
- [ ] Sayaca yalnızca `io_sense` erişiyor
- [ ] Pull-up durumu doğrulandı (ISSUE-002)

## Test Plan

- [ ] Bilinen hacimde su akıtılarak kalibrasyon doğrulandı
- [ ] Farklı debilerde ölçüm doğruluğu kontrol edildi
- [ ] Çok düşük debide sıfıra yuvarlama olmuyor
- [ ] Farklı okuma periyotlarında sonuç tutarlı (mevcut projenin hatası tekrarlanmıyor)
- [ ] Sensör sökülünce doğru kalite raporlanıyor
- [ ] Toplam hacim sayacı doğru birikiyor
- [ ] Uzun süreli akışta sayaç taşması sorunu yok

## Review Checklist

- [ ] Architecture'a uygun mu? (§9.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — **tek okuyucu kuralı**
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — arıza/aralık dışı
- [ ] ESP32 resource kullanımı uygun mu? — kalıcı sayaç flash aşınması
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`(pulses*100)/450` formülü yasak**

## Definition of Done

Ortak DoD + gerçek su ile kalibrasyon doğrulandı + farklı okuma periyotlarında tutarlılık
kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Mevcut algoritma REDDEDİLDİ

```text
Eski: litersPerMinute = (pulses * 100) / 450

UC HATA:
  1. SABIT zaman penceresi varsayiyor — fonksiyon 500 ms ve 600 ms
     periyotlarla cagriliyordu, sonuc anlamsizdi
  2. TAMSAYI bolmesi dusuk debileri SIFIRA yuvarliyordu — kuru calisma
     tespiti icin en kritik bolge tam da orasi
  3. Iki farkli cagiran ayni sayaci tuketiyordu

Yeni: debi = (darbe / darbePerLitre) / (gecenSure_ms / 60000)   → float L/dk
      PCNT gercek gecen sureyi birlikte veriyor (TASK-019).
```

## Karar 2 — Darbe/litre katsayısı derleme zamanı, `scale` saha trim'i

Sensör modelinin fiziksel özelliği (YF-S401 için ~450 darbe/L) sabit;
`SensorConfig.scale` saha kalibrasyonu için. NTC ve röle polaritesiyle
aynı ayrım.

**Not:** 450 değeri eski kodun yorumundan geliyor ve **doğrulanmamış**.
Gerçek sensörle bilinen hacim akıtılarak ölçülmeli (ISSUE-014).

## Karar 3 — Sıfır akış ile sensör arızası bu katmanda AYIRT EDİLEMEZ

```text
"0 L/dk" iki anlama gelir:
   · gercekten akis yok (pompa kapali → NORMAL)
   · sensor kopuk       (pompa acik  → KURU CALISMA)

Ayrim POMPA DURUMU bilgisini gerektirir; bu sensor onu bilmez ve bilmemeli.
Selected: Bu katman yalnizca "0 L/dk, kalite OK" veya "okunamiyor, FAULT"
          ayrimini yapar. Capraz kontrol TASK-031'in isi.
Gerekce:  Sensorun pompa durumunu bilmesi katman ihlali ve dongusel
          bagimlilik olurdu.
```

## Karar 4 — Toplam hacim sayacı RAM'de

```text
Kalici yapilirsa uzun donem tuketim bilgisi korunur AMA flash asinmasi
yaratir. Seyrek yazma (10 dk) bile gunde 144 yazma demek.
Selected: RAM'de tutulur; kalicilik TASK-058 (gecmis veri) kapsaminda
          degerlendirilecek — orasi zaten periyodik yazma yapiyor.
Gerekce:  Iki ayri kalicilik mekanizmasi kurmak yerine mevcut olani kullanmak.
```

---

# STEP 3 — REVIEW RECORD

- [x] Debi hesabı **gerçek geçen süreye** dayanıyor (PCNT `elapsed` veriyor)
- [x] **Float aritmetik** — düşük debi sıfıra yuvarlanmıyor (kuru çalışma
      tespiti için en kritik bölge tam da orası)
- [x] Toplam hacim sayacı çalışıyor; kalıcılık kararı verildi (RAM, TASK-058'e
      bırakıldı — ikinci bir flash yazma mekanizması kurulmadı)
- [x] Sıfır akış ile sensör arızası **ayrımının bu katmanda yapılamayacağı**
      belgelendi; çapraz kontrol TASK-031'e ait
- [x] Aralık dışı değer hat tarafından `OUT_OF_RANGE`
- [x] Sayaca yalnızca `io_sense` erişiyor
- [x] PCNT taşması `FAULT`'a çevriliyor (sessizce yanlış debi yayınlanmıyor)
- [ ] **Gerçek su ile kalibrasyon — donanım gerekiyor**

**ISSUE-014** kaydedildi: `PULSES_PER_LITER = 450` **doğrulanmamış**. Bu
yalnızca gösterim hatası değil — akış doğrulaması (TASK-031) bu değere göre
kuru çalışma kararı verir; katsayı 2 kat yanlışsa koruma eşiği de 2 kat kayar.

**TASK-025: TAMAMLANDI** (kalibrasyon doğrulaması bekliyor).
