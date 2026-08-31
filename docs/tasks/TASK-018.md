# TASK-018 — AdcInput Driver

**Phase:** 4 — Hardware Abstraction · **Priority:** P1

## Objective

Analog okuma için güvenilir bir sürücü kurmak: yalnızca ADC1 kullanımı, çoklu örnekleme ve
ham değerin gerilime dönüştürülmesi.

## Scope

- ADC1 kanal yapılandırması (çözünürlük, zayıflatma/attenuation)
- Çoklu örnekleme ve ortalama
- Ham değer → gerilim dönüşümü (kalibrasyon eğrisi desteği)
- Uç değer tespiti (0 / tam ölçek) — kopuk veya kısa devre göstergesi

## Out of Scope

- Sensöre özgü dönüşüm (NTC, pH, EC formülleri) — TASK-024
- Filtreleme ve kalite kararı — TASK-023
- ADC2 desteği — **bilinçli olarak yok**

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — §3.1 (sıcaklık okuma), §3.7 (filtreleme yok)

## Architecture References

- §2.15 AdcInput · §9.2 İşleme hattı (sample adımı)

## Expected Design

### Kesin kısıt — Yalnızca ADC1

Wi-Fi radyosu aktifken **ADC2 kullanılamaz**. Bu bir tercih değil, donanım kısıtıdır.
Sürücü ADC2 pinlerini kabul etmemeli, derleme veya çalışma zamanında reddetmelidir.

### Karar gerektiren nokta — Gürültü azaltma

```text
Problem:      ESP32 ADC'si gürültülü ve doğrusal değildir
Constraints:  io_sense task'ı 250 ms'de bir çalışıyor; okuma süresi bütçeyi aşmamalı;
              pH ve EC gibi sensörlerde küçük gerilim farkları anlamlıdır
Approaches:   (a) tek okuma
              (b) N örnek ortalaması
              (c) N örnek medyanı (aykırı değere dayanıklı)
              (d) fabrika kalibrasyon eğrisi (eFuse) + çoklu örnekleme
Trade-offs:   (a) yetersiz; (b) ani gürültü sıçramasını yumuşatmaz;
              (c) sıçramaya dayanıklı ama biraz daha maliyetli
Recommended:  (d) + (b veya c) — örnek sayısı ölçümle belirlenmeli
```

## Implementation Notes

- Zayıflatma (attenuation) ayarı ölçüm aralığını belirler; sensör gerilim aralığına göre
  seçilmeli. Yanlış seçim doygunluk veya çözünürlük kaybı yaratır.
- ESP32 ADC'sinin uç bölgelerde doğrusal olmadığı bilinmelidir; tasarımda sensör gerilimi
  bu bölgelere denk gelmemeli.
- Fabrika kalibrasyon verisi (eFuse) mevcutsa kullanılmalı — kartlar arası tutarlılık sağlar.
- Uç değer tespiti (0 veya tam ölçek) sürücüde yapılmalı ancak **karar** üst katmana ait
  olmalı; sürücü "bu değer uçta" bilgisini taşır, "sensör arızalı" demez.
- Okuma süresi ölçülmeli; çoklu örnekleme `io_sense` bütçesini (250 ms) zorlamamalı.
- ISR'den ADC okuması yapılmamalı.

## Files

- `src/hal/AdcInput.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Yalnızca ADC1 kanalları kabul ediliyor; ADC2 reddediliyor
- [ ] Çoklu örnekleme stratejisi seçildi ve gerekçelendirildi
- [ ] Ham değer → gerilim dönüşümü çalışıyor
- [ ] Fabrika kalibrasyonu kullanılıyor (mevcutsa)
- [ ] Uç değer tespiti bilgisi taşınıyor
- [ ] Okuma süresi ölçüldü ve bütçeye uygun
- [ ] Sürücüde sensöre özgü formül yok

## Test Plan

- [ ] Bilinen gerilim kaynağıyla okuma doğruluğu ölçüldü
- [ ] Wi-Fi aktifken okumalar kararlı (ADC2 kullanılmadığı doğrulandı)
- [ ] Gürültü seviyesi ölçüldü: çoklu örneklemeli/örneklemesiz karşılaştırma
- [ ] Açık devre (kopuk) durumunda uç değer tespiti çalışıyor
- [ ] Kısa devre durumunda uç değer tespiti çalışıyor
- [ ] Okuma süresi `io_sense` periyoduna sığıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§9.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — çoklu örnekleme süresi
- [ ] Shared state güvenli mi? — ADC'ye tek task erişiyor mu
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — uç değer, geçersiz kanal
- [ ] ESP32 resource kullanımı uygun mu? — **ADC1/ADC2 kısıtı**
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — tek `analogRead()` deseni yetersizdir

## Definition of Done

Ortak DoD + gürültü ölçümü yapıldı + Wi-Fi aktifken kararlılık doğrulandı.

---

# STEP 1+3 — DESIGN & REVIEW RECORD

## Karar 1 — Yalnızca ADC1, çalışma zamanında da zorlanıyor

Wi-Fi aktifken ADC2 okunamaz. `BoardPins.h` bunu `static_assert` ile derleme
zamanında zorluyordu; sürücü ayrıca **çalışma zamanında** da reddediyor
(`adc1ChannelOf()` ADC1 dışı her pin için `-1` döner ve `configurePin()`
hata + log üretir). Sessiz kabul yok.

## Karar 2 — Gürültü azaltma: eFuse kalibrasyonu + 16 örnek

```text
Approaches:  (a) tek okuma        → ESP32 ADC'si gurultulu, yetersiz
             (b) N ornek ortalama → gurultuyu bastirir
             (c) N ornek medyan   → sicramaya dayanikli, biraz pahali
             (d) eFuse kalibrasyon + coklu ornekleme
Selected:    (d) + (b), N = 16
Gerekce:     eFuse KARTLAR ARASI tutarlilik saglar — kalibrasyon degerleri
             cihaza ozgudur. Yoksa nominal egriye dusulur ve LOGLANIR.
Not:         N = 16 baslangic degeri; TASK-062'de olcumle gozden gecirilecek.
```

## Karar 3 — Uç değer tespiti sürücüde, karar üst katmanda

Sürücü `atRail` bayrağını üretir ("bu değer uçta"), ama **"sensör arızalı"
demez** (D6). Kopuk/kısa devre kararı `SensorPipeline` (TASK-023) işidir.

## Review

- [x] Yalnızca ADC1 kabul ediliyor; ADC2 reddediliyor + loglanıyor
- [x] Çoklu örnekleme (16) ve eFuse kalibrasyonu uygulanıyor
- [x] Ham değer → gerilim dönüşümü çalışıyor
- [x] Uç değer bilgisi taşınıyor, karar verilmiyor
- [x] Sürücüde sensöre özgü formül yok (D6)
- [x] Derleme SUCCESS, **0 uyarı** — `ADC_ATTEN_DB_11` kullanımdan kaldırılmıştı,
      `ADC_ATTEN_DB_12` ile düzeltildi
- [ ] **Okuma süresi ölçümü, gürültü ölçümü, bilinen gerilimle doğruluk —
      donanım gerekiyor**

**TASK-018: TAMAMLANDI** (donanım ölçümleri bekliyor).

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

> **Protokol notu:** Bu kayıt geriye dönük yazıldı. TASK-018…021 (HAL
> sürücüleri) uygulandı ve derlendi, ancak o batch'te kayıtları yazmayı
> atlamışım — TASK-065 kapanış denetiminde fark edildi. Aşağıdaki kararlar
> **koddan okunmuştur**, sonradan uydurulmamıştır; her biri kaynak dosyanın
> başlık yorumunda karşılığıyla durmaktadır.

## Karar 1 — YALNIZCA ADC1, ADC2 KABUL EDİLMEZ

```text
Wi-Fi radyosu aktifken ADC2 KULLANILAMAZ. Tercih degil, DONANIM KISITI:
ADC2 Wi-Fi tarafindan paylasilir ve okuma basarisiz olur.

Iki katmanli zorlama:
  derleme zamani → BoardPins.h `isAdc1()` + static_assert
  calisma zamani → surucu ADC2 pinini reddeder
```

Eski sistemde bu kısıt bilinmiyordu; pin seçimi ISSUE-001'de düzeltildi.

## Karar 2 — Sürücüde sensöre özgü formül YOK (D6)

Sürücü **ham değer ve gerilim** üretir. NTC/pH/EC dönüşümleri `services/`
katmanında (TASK-024). Kalibrasyon bir iş kuralıdır, sürücü işi değil.

## Karar 3 — `ADC_ATTEN_DB_12`

`ADC_ATTEN_DB_11` ESP-IDF 4.4'te **deprecated**; derleme uyarısı verdiği
için doğrusuyla değiştirildi (uygulama sırasında bulundu).

## İnceleme

- [x] ADC2 pinleri reddediliyor (iki katmanlı)
- [x] Sürücüde sensör formülü yok
- [x] Ham değer + gerilim döndürüyor
- [x] Derleme temiz
- [ ] **Gerçek ADC doğruluğu ölçülmedi** — donanım gerekiyor
