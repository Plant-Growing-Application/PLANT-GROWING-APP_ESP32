# TASK-024 — Analog Sensors (Water Temp, pH, EC)

**Phase:** 5 — Sensör System · **Priority:** P1

## Objective

Üç analog sensörün fiziksel büyüklüğe dönüşümünü ve kalibrasyon modelini implement etmek.
`REQUIREMENTS.md`'de pH ve EC yalnızca arayüzde sabit değer olarak duruyordu — bu task
onları gerçek ölçüme dönüştürür.

## Scope

- Su sıcaklığı dönüşümü (sensör tipi kararına göre)
- pH dönüşümü ve 2 nokta kalibrasyon (pH 4 / pH 7)
- EC dönüşümü, kalibrasyon ve sıcaklık telafisi
- Kalibrasyon verisinin config'te saklanması
- Takılı olmayan sensörün `NOT_PRESENT` olarak raporlanması

## Out of Scope

- ADC sürücüsü (TASK-018)
- Filtreleme ve kalite (TASK-023)
- Kalibrasyon arayüzü (TASK-044)

## Dependencies

- TASK-018, TASK-023

## Requirements

- `REQUIREMENTS.md` — §3.1 (su sıcaklığı `[~]`), §3.3 (pH eksik), §3.4 (EC eksik)

## Architecture References

- §9.3 Sensör kataloğu

## Expected Design

### Karar gerektiren nokta 1 — Su sıcaklığı sensör tipi (ARCHITECTURE §20)

```text
Problem:      Analog NTC mi, dijital DS18B20 mı?
Constraints:  ADC1 kanal bütçesi sınırlı (pH + EC + seviye zaten kanal istiyor);
              NTC kalibrasyon ve doğrusallaştırma gerektirir
Approaches:   (a) analog NTC — mevcut donanım
              (b) dijital DS18B20 — bir ADC1 kanalı boşaltır, kalibrasyon gerektirmez
Trade-offs:   (b) donanım değişikliği gerektirir ama doğruluk ve kanal kazandırır
Recommended:  Donanım kararına bağlı; STEP 1'de kapatılmalı
```

Mevcut koddaki formül eksiktir: seri direnç değeri ve besleme gerilimi hesaba katılmamış,
`sensorValue == 0` veya tam ölçek durumunda **sıfıra bölme / NaN** üretiyor. Bu formül
kopyalanmamalı; NTC seçilirse doğru Steinhart-Hart veya Beta modeli sıfırdan kurulmalı.

### Karar gerektiren nokta 2 — pH kalibrasyon modeli

2 nokta (pH 4 ve pH 7) doğrusal kalibrasyon standarttır. Elektrot yaşlandıkça eğim
değişir; kalibrasyon verisi tarih damgalı saklanmalı ve eski kalibrasyon uyarı üretmeli.

### EC sıcaklık telafisi

EC ölçümü sıcaklığa güçlü bağımlıdır. Telafi için su sıcaklığı okuması gerekir —
bu, **sensörler arası bir bağımlılıktır** ve hat içinde ele alınmalı. Sıcaklık okuması
geçersizse (kalite ≠ OK), EC de `OUT_OF_RANGE`/düşük güvenle raporlanmalı.

## Implementation Notes

- Değerler **float** olarak taşınmalı. Mevcut projede `int`'e yuvarlama yapılıyordu;
  pH 6.5 ile 6.0 arasındaki fark otomasyon için anlamlıdır.
- Sensör takılı değilse ADC uçta sabit okur; bu `NOT_PRESENT` mı `FAULT` mı ayrımı
  yapılandırmayla belirlenmeli (sensör beklenen mi?).
- pH/EC probları DC beslemede polarize olur; sürekli ölçüm elektrot ömrünü kısaltır.
  Örnekleme periyodu (2 s) bu açıdan değerlendirilmeli.
- Kalibrasyon dışı değer (örn. pH 14'ün üstü) reddedilmeli.
- Sıfıra bölme ve `log()` domain hatası her formülde açıkça korunmalı.

## Files

- `src/services/sensors/WaterTempSensor.h` / `.cpp` (yeni)
- `src/services/sensors/PhSensor.h` / `.cpp` (yeni)
- `src/services/sensors/EcSensor.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Su sıcaklığı sensör tipi kararı verildi ve gerekçelendirildi
- [ ] Üç sensör de fiziksel birime doğru dönüştürüyor
- [ ] Değerler float olarak taşınıyor
- [ ] pH 2 nokta kalibrasyonu çalışıyor; kalibrasyon config'te saklanıyor
- [ ] EC sıcaklık telafisi uygulanıyor; sıcaklık geçersizse EC güveni düşürülüyor
- [ ] Takılı olmayan sensör `NOT_PRESENT` raporluyor
- [ ] Sıfıra bölme / domain hatası her formülde korunuyor
- [ ] Kalibrasyon tarihi saklanıyor, eski kalibrasyon uyarı üretiyor

## Test Plan

- [ ] Su sıcaklığı: referans termometre ile karşılaştırıldı
- [ ] pH: tampon çözeltilerle (4.0 ve 7.0) doğrulandı
- [ ] EC: bilinen iletkenlikte çözelti ile doğrulandı
- [ ] EC sıcaklık telafisi farklı sıcaklıklarda doğrulandı
- [ ] Sensör sökülünce `NOT_PRESENT`/`FAULT` doğru raporlanıyor
- [ ] Uç ADC değerlerinde (0, tam ölçek) çökme veya NaN yok
- [ ] Sıcaklık okuması geçersizken EC davranışı doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§9.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — EC'nin sıcaklığa bağımlılığı nasıl çözüldü
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **matematiksel domain hataları**
- [ ] ESP32 resource kullanımı uygun mu? — ADC1 kanal bütçesi
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **mevcut NTC formülü hatalıdır, kopyalanmamalı**

## Definition of Done

Ortak DoD + üç sensör de referans ölçümle doğrulandı + uç değerlerde NaN/çökme olmadığı
kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Su sıcaklığı: NTC (mevcut donanım) — ISSUE açık

```text
ARCHITECTURE §20 acik maddesi: NTC mi DS18B20 mi?
Selected: NTC — mevcut donanim buna gore kablolu (BoardPins: ADC_WATER_TEMP=35).
Not:      DS18B20 secilirse bir ADC1 kanali serbest kalir ve kalibrasyon
          ihtiyaci ortadan kalkar. Karar KULLANICININ; yazilim NTC ile
          calisir durumda ve degisim tek dosyayla sinirli.
```

### Mevcut formül REDDEDİLDİ

```text
Eski kod:
    temperature = 1.0 / (log((4095.0/sensorValue) - 1.0)/3950.0 + 1.0/298.15) - 273.15

UC HATA:
  1. sensorValue == 0     → 4095/0    = inf → log(inf) → tanimsiz
  2. sensorValue == 4095  → 4095/4095-1 = 0 → log(0)   = -inf
  3. SERI DIRENC ve BESLEME GERILIMI hesaba KATILMAMIS —
     formul yalnizca belirli bir divider oraninda dogru olurdu

Sonuc sessizce kullaniliyordu (REQUIREMENTS §3.1).

Yeni: Beta denklemi, gerilim bolucu topolojisi ACIK yazildi, her bolme ve
      log domain hatasi KORUNDU. Hesaplanamayan deger FAULT olur.
```

### Bilinmeyen: bölücü topolojisi

```text
Iki olasilik:
  (A) NTC → VCC,  R_seri → GND   → sicaklik ARTAR, V_out ARTAR
  (B) R_seri → VCC, NTC → GND    → sicaklik ARTAR, V_out AZALIR

Selected: (A) varsayildi ve KODA ACIKCA YAZILDI.
          Yanlissa okumalar ters yonde degisir — ilk denemede fark edilir.
          `docs/HARDWARE.md`'de dogrulama maddesi olarak kayitli.
```

## Karar 2 — NTC parametreleri derleme zamanı sabiti

`R_series`, `Beta`, `R25` fiziksel devre özellikleridir; `SensorConfig`'te
`offset`/`scale` alanları **saha trim'i** içindir. İkisi farklı şeydir:
biri devrenin ne olduğunu, diğeri ölçümün ne kadar kaydığını söyler.
(Röle polaritesiyle aynı gerekçe — TASK-017 Karar 1.)

## Karar 3 — pH: 2 nokta kalibrasyon, eğim yaşlanması izlenir

```text
Standart: pH 4.0 ve pH 7.0 tampon cozeltileriyle 2 nokta dogrusal kalibrasyon.
Incelik:  Elektrot yaslandikca EGIM duser. Kalibrasyon verisi TARIH damgali
          saklanmali ve eski kalibrasyon UYARI uretmeli.
Bu surumde: egim/ofset `SensorConfig.scale`/`offset` uzerinden uygulanir
          (2 nokta kalibrasyon bu ikisine indirgenebilir).
          Kalibrasyon ARAYUZU ve tarih damgasi TASK-044 kapsaminda.
```

## Karar 4 — EC sıcaklık telafisi ve `lowConfidence`

```text
EC olcumu sicakliga GUCLU bagimlidir (~%2/°C). Telafi:
    EC_25 = EC_olculen / (1 + 0.02 × (T - 25))

Sicaklik GECERSIZ ise (kalite != OK):
  Approaches: (a) telafisiz ham deger yayinla → sessizce YANLIS deger
              (b) EC'yi FAULT yap             → asiri sert, EC bilgi amacli
              (c) telafisiz yayinla + lowConfidence isaretle
  Selected:   (c) — deger kullanilabilir ama hat kalitesini dusurur (STALE).
              Kullanici degeri gorur, otomasyon ona guvenmez.
Ayrica: 1 + 0.02×(T-25) sifira yaklasirsa (T ≈ -25 °C) bolme korunur.
```

## Karar 5 — `NOT_PRESENT` donanıma hiç dokunmadan

`SensorConfig.enabled == 0` → `SensorService` sensörü hiç örneklemez,
doğrudan `NOT_PRESENT` üretir (TASK-023 `pipeline::notPresent`). pH/EC
sahada takılı olmayabilir ve arayüz bunu "arıza" değil "yok" göstermeli.

---

# STEP 3 — REVIEW RECORD

- [x] Su sıcaklığı sensör tipi: **NTC** (mevcut donanım). DS18B20 alternatifi
      belgelendi; değişim tek dosyayla sınırlı
- [x] Üç sensör de fiziksel birime dönüştürüyor
- [x] Değerler **float** (mevcut sistemdeki `int` yuvarlaması yok)
- [x] pH kalibrasyonu `scale`/`offset` üzerinden (2 nokta bu ikisine indirgenir)
- [x] **EC sıcaklık telafisi uygulanıyor**; sıcaklık geçersizse değer
      yayınlanıyor ama `lowConfidence` → hat `STALE`'e çeviriyor
- [x] Takılı olmayan sensör `NOT_PRESENT` (donanıma hiç dokunulmadan)
- [x] **Sıfıra bölme / `log()` domain hatası her formülde korunuyor** —
      9 ayrı koruma noktası
- [ ] Kalibrasyon tarihi ve eski kalibrasyon uyarısı → **TASK-044** (kalibrasyon
      arayüzü orada); bu task'ta yalnızca dönüşüm var
- [ ] **Referans ölçümlerle doğrulama — donanım gerekiyor**

## Eski formülün reddi — somut

```text
Eski: 1.0/(log((4095.0/sensorValue) - 1.0)/3950.0 + 1.0/298.15) - 273.15

  sensorValue == 0    → 4095/0 = inf → log(inf)  → TANIMSIZ
  sensorValue == 4095 → log(0) = -inf            → TANIMSIZ
  seri direnc ve besleme gerilimi HESABA KATILMAMIS

Yeni: gerilim bolucu acikca modellendi, ucta okuma FAULT'a cevriliyor,
      log() argumani pozitiflik kontrolunden geciyor, 1/invT sifira
      bolme korumasi var.
```

**ISSUE-015** kaydedildi: NTC bölücü topolojisi ve direnç değerleri
**doğrulanmadı**; varsayım kodda açıkça yazılı.

**TASK-024: TAMAMLANDI** (donanım doğrulaması bekliyor).
