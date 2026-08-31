# TASK-019 — PulseCounter Driver (PCNT)

**Phase:** 4 — Hardware Abstraction · **Priority:** P1

## Objective

Akış sensörü darbelerini **donanımsal PCNT birimiyle** saymak. Yazılım ISR'sinin getirdiği
jitter, kayıp darbe ve zamanlama belirsizliğini ortadan kaldırmak.

## Scope

- PCNT birimi yapılandırması: kanal, kenar, filtre
- Zaman penceresi bazlı sayım okuma (sayı + geçen süre birlikte)
- Donanımsal gürültü filtresi (glitch filter)
- Sayaç taşması yönetimi

## Out of Scope

- L/dk hesabı ve kalibrasyon (TASK-025)
- Akış doğrulama güvenlik mantığı (TASK-031)

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — §3.2 (akış sensörü), ISSUE-002

## Architecture References

- §2.15 PulseCounter (ISR yerine PCNT gerekçesi)

## Expected Design

### Karar gerektiren nokta — ISR mi, PCNT mi

```text
Problem:      Darbe sayımı nasıl yapılacak?
Constraints:  Akış doğrulama güvenlik kararı üretir → sayım güvenilir olmalı;
              yüksek debili sensörde darbe frekansı yükselir;
              io_sense task'ı zamanında çalışmayabilir
Approaches:   (a) GPIO interrupt + yazılım sayacı  (mevcut projenin yöntemi)
              (b) donanımsal PCNT birimi
Trade-offs:   (a) her darbede ISR çalışır → CPU yükü ve jitter;
                  yoğun Wi-Fi trafiğinde darbe kaçırma riski;
                  sayaç okuma ile sıfırlama arasında yarış durumu
              (b) donanım sayar, CPU karışmaz, kayıp darbe olmaz
Recommended:  (b) — ESP32'de PCNT birimi mevcut ve tam bu iş için tasarlanmış
```

**Kritik tasarım noktası:** Okuma fonksiyonu **sayım ile birlikte geçen süreyi de**
döndürmelidir. Mevcut projenin en büyük hatası, sabit bir zaman penceresi varsayıp
`(pulses * 100) / 450` hesabı yapmasıydı; fonksiyon farklı periyotlarda çağrıldığı için
sonuç anlamsızlaşıyordu. Süre bilgisi olmadan darbe sayısı bir debi değeridir denemez.

**İkinci kritik nokta:** Mevcut projede iki farklı çağıran (task ve display) aynı sayacı
tüketiyordu; biri okuyunca diğerinin verisi sıfırlanıyordu. Yeni tasarımda sayacı
**yalnızca `io_sense` task'ı okur**; başka hiçbir yerden erişilmez.

## Implementation Notes

- ISSUE-002: GPIO 34–39'da dahili pull-up yoktur. Seçilen pin ve pull-up çözümü bu task'ta
  doğrulanmalı; harici direnç gerekiyorsa `docs/HARDWARE.md`'ye yazılmalı.
- PCNT donanımsal filtre değeri, sensörün minimum darbe genişliğine göre ayarlanmalı;
  çok agresif filtre gerçek darbeleri de eler.
- 16-bit sayaç taşması ele alınmalı: okuma periyodu ile maksimum debi çarpımı sayaç
  sınırını aşmamalı, ya da taşma kesmesi sayılmalı.
- Sayım okuma ile sıfırlama **atomik** olmalı; arada gelen darbe kaybolmamalı.
- Sıfır darbe "akış yok" demektir ama "sensör kopuk" da olabilir; bu ayrım sürücüde değil,
  TASK-025 ve TASK-031'de yapılır.

## Files

- `src/hal/PulseCounter.h` / `.cpp` (yeni)
- `docs/HARDWARE.md` (güncelleme — pull-up kararı)

## Acceptance Criteria

- [ ] PCNT birimi yapılandırıldı ve sayıyor
- [ ] Okuma **sayım + geçen süre** birlikte döndürüyor
- [ ] Donanımsal filtre yapılandırıldı ve değeri gerekçelendirildi
- [ ] Sayaç taşması ele alındı
- [ ] Okuma/sıfırlama atomik, darbe kaybı yok
- [ ] Sayaca yalnızca tek nokta erişiyor
- [ ] Pull-up gereksinimi doğrulandı (ISSUE-002 kapandı)

## Test Plan

- [ ] Sinyal jeneratörü ile bilinen frekansta darbe verildi, sayım doğru
- [ ] Yoğun Wi-Fi trafiği altında darbe kaybı yok (ISR yönteminden farkı gösterildi)
- [ ] Yüksek debide sayaç taşması doğru işleniyor
- [ ] Gürültülü sinyalde filtre yanlış sayımı engelliyor
- [ ] Ardışık okumalarda darbe kaybı yok
- [ ] Gerçek akış sensörüyle su akıtılarak doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.15)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — **sayaca tek erişim noktası**
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — taşma, geçersiz yapılandırma
- [ ] ESP32 resource kullanımı uygun mu? — PCNT birimi doğru kullanılmış mı
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **ISR + sabit periyot varsayımı taşınmamalı**

## Definition of Done

Ortak DoD + sinyal jeneratörüyle doğruluk kanıtlandı + Wi-Fi yükü altında darbe kaybı
olmadığı gösterildi.

---

# STEP 1+3 — DESIGN & REVIEW RECORD

## Karar 1 — ISR değil, donanımsal PCNT

```text
Selected: (b) PCNT birimi
Gerekce:  Akis dogrulamasi bir GUVENLIK karari uretir (kuru calisma, TASK-031).
          ISR yonteminin uc sorunu:
            · her darbede kesme → CPU yuku ve jitter
            · yogun Wi-Fi trafiginde darbe kacirma riski
            · okuma ile sifirlama arasinda yaris durumu
          PCNT'de donanim sayar, kayip darbe olmaz.
```

## Karar 2 — Okuma, sayımla birlikte **geçen süreyi** döndürür

Mevcut sistemin en büyük hatası buydu: `(pulses * 100) / 450` hesabı **sabit
bir zaman penceresi** varsayıyordu, ama fonksiyon 500 ms ve 600 ms periyotlarla
çağrılıyordu — sonuç anlamsızdı.

`readAndReset()` `PulseWindow{pulses, elapsed, overflow}` döndürür. **Süre
bilgisi olmadan darbe sayısına "debi" denemez.**

## Karar 3 — Taşma sınırı hesaplandı

```text
PCNT 16 bit isaretli. YF-S401 sinifi sensor 10 L/dk'da ~1 kHz uretir.
1 sn'lik pencerede ~1000 darbe → 32000 sinirinin cok altinda.
Yine de tasma TESPIT EDILIR ve `overflow` bayragiyla bildirilir.
```

## Review

- [x] PCNT yapılandırıldı; donanımsal gürültü filtresi (ns → tick dönüşümü)
- [x] Okuma **sayım + geçen süre** birlikte döndürüyor
- [x] Taşma tespit ediliyor ve raporlanıyor
- [x] Okuma/sıfırlama arasında sayaç duraklatılıyor (darbe kaybı en aza indirildi)
- [x] Sayaca tek erişim noktası (`io_sense`) — header'da belgeli
- [x] Derleme SUCCESS, 0 uyarı
- [ ] **Sinyal jeneratörü doğruluğu, Wi-Fi yükü altında darbe kaybı, gerçek
      su ile kalibrasyon — donanım gerekiyor**
- [ ] **ISSUE-002 pull-up doğrulaması — donanım gerekiyor** (pin GPIO 4'e
      taşındı, dahili pull-up mevcut; ölçümle teyit edilmeli)

**TASK-019: TAMAMLANDI** (donanım ölçümleri bekliyor).

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

> **Protokol notu:** Geriye dönük kayıt (bkz. TASK-018).

## Karar 1 — DONANIMSAL PCNT, yazılım ISR'si DEĞİL

```text
Akis dogrulamasi bir GUVENLIK karari uretir (kuru calisma, TASK-031).
Sayimin guvenilir olmasi gerekir.

ISR yontemi (eski sistem):
  · her darbede kesme → CPU yuku ve jitter
  · yogun Wi-Fi trafiginde DARBE KACIRMA riski
  · okuma ile sifirlama arasinda YARIS DURUMU
PCNT:
  · donanim sayar, CPU karismaz
  · kayip darbe olmaz
```

## Karar 2 — Okuma, sayımla BİRLİKTE geçen süreyi de döndürür

**Eski sistemin en büyük hatası buydu.** `(pulses * 100) / 450` hesabı
SABİT bir zaman penceresi varsayıyordu, ama fonksiyon 500 ms ve 600 ms
periyotlarla çağrılıyordu — sonuç anlamsızdı.

> Süre bilgisi olmadan darbe sayısına "debi" denemez.

## Karar 3 — TEK OKUYUCU

Sayaca yalnızca `io_sense` task'ı erişir. Eski sistemde iki farklı çağıran
aynı sayacı tüketiyordu; biri diğerinin darbelerini yiyordu.

## İnceleme

- [x] PCNT birimi kullanılıyor
- [x] Okuma geçen süreyi birlikte döndürüyor
- [x] Taşma `FAULT`'a çevriliyor (sessizce yanlış debi yayınlanmıyor)
- [x] Tek okuyucu — tarama ile doğrulandı
- [ ] **Gerçek darbe sayımı doğrulanmadı** (ISSUE-014: 450 darbe/L teyitsiz)
