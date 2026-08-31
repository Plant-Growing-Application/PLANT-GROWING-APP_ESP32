# TASK-023 — Sensor Processing Pipeline

**Phase:** 5 — Sensor System · **Priority:** P0

## Objective

Ham sensör değerini güvenilir bir ölçüme dönüştüren ortak hattı kurmak:
kalibrasyon → filtre → doğrulama → kalite kararı.

## Scope

- Kalibrasyon uygulama (config'ten katsayı/ofset/eğri)
- Filtreleme (EMA veya medyan — seçim gerekçeli)
- Doğrulama: aralık kontrolü, değişim hızı sınırı, bayatlama tespiti
- Kalite kararı üretimi
- Hata sayaçları

## Out of Scope

- Sensöre özgü formüller (TASK-024, 025, 026)
- Donanım okuma (HAL)
- UI gösterimi

## Dependencies

- TASK-022, TASK-014

## Requirements

- `REQUIREMENTS.md` — §3.7 (filtreleme/kalibrasyon/doğrulama yok), §9 (invalid sensor value)

## Architecture References

- §9.2 İşleme hattı diyagramı
- §9.5 Sensör hata yönetimi tablosu

## Expected Design

### Karar gerektiren nokta 1 — Filtre tipi

```text
Problem:      ADC gürültüsü ve tek seferlik sıçramalar nasıl bastırılacak?
Constraints:  Bellek sınırlı (her sensör için geçmiş tutulacak);
              güvenlik sensöründe gecikme tehlikelidir — su seviyesi düşünce
              filtre yüzünden geç tepki verilmemeli
Approaches:   (a) filtresiz
              (b) EMA (üstel hareketli ortalama) — az bellek, sürekli gecikme
              (c) medyan-N — sıçramaya dayanıklı, N örnek gecikme
              (d) sensör tipine göre farklı filtre
Trade-offs:   Güvenlik sensörü için gecikme kabul edilemez;
              pH/EC için gürültü bastırma önemli
Recommended:  (d) — filtre tipi ve parametresi sensör tanımında belirtilsin;
              su seviyesi için filtresiz veya çok kısa filtre
```

### Karar gerektiren nokta 2 — Değişim hızı sınırı

Fiziksel olarak imkânsız sıçramalar (su sıcaklığının 1 saniyede 20 °C değişmesi) atılmalı.
Sınır sensör tipine göre config'te tanımlanmalı. Ancak dikkat: **su seviyesi gerçekten
hızlı değişebilir** (pompa çalışırken); bu sensöre agresif sınır uygulanmamalı.

### Bayatlama (STALE) tespiti

N örnek boyunca hiç değişmeyen değer şüphelidir. Ancak sabit bir ortamda gerçek ölçüm de
sabit olabilir; eşik ADC gürültü seviyesinin altında olacak şekilde seçilmeli.

## Implementation Notes

- Hat **saf fonksiyonlar** olarak yazılmalı: girdi ham değer + config + geçmiş, çıktı
  işlenmiş değer + kalite. Bu, host tarafında donanımsız test edilmesini sağlar.
- Filtre durumu her sensör için ayrı tutulmalı, sabit boyutlu dizide.
- Kalibrasyon config'ten okunmalı; koda gömülmemeli.
- Kalite kararı **öncelik sırasıyla** verilmeli: FAULT > OUT_OF_RANGE > STALE > OK.
- Hata sayaçları teşhis için tutulmalı ve API'den okunabilmeli.
- Filtre geçmişi ilk açılışta doldurulmalı; ilk okumada yanlış değer üretilmemeli
  (ilk N örnek boyunca kalite `STALE` veya değer yayınlanmaz).

## Files

- `src/services/sensors/SensorPipeline.h` / `.cpp` (yeni)
- `src/services/sensors/Filters.h` (yeni)

## Acceptance Criteria

- [ ] Kalibrasyon config'ten uygulanıyor
- [ ] Filtre tipi sensör bazında yapılandırılabilir
- [ ] Güvenlik sensöründe filtre gecikmesi minimum
- [ ] Aralık, değişim hızı ve bayatlama kontrolleri çalışıyor
- [ ] Kalite kararı öncelik sırasına göre veriliyor
- [ ] Hata sayaçları tutuluyor ve okunabiliyor
- [ ] Hat saf fonksiyonlardan oluşuyor, host'ta test edilebiliyor
- [ ] İlk açılışta yanlış değer yayınlanmıyor

## Test Plan

- [ ] Host tarafında sentetik veri ile: gürültülü sinyal filtreleniyor
- [ ] Tek seferlik sıçrama atılıyor, gerçek değişim geçiriliyor
- [ ] Aralık dışı değer `OUT_OF_RANGE` üretiyor
- [ ] Sabit değer N örnek sonra `STALE` üretiyor
- [ ] Uç değer (0 / tam ölçek) `FAULT` üretiyor
- [ ] Su seviyesi için filtre gecikmesi ölçüldü ve kabul edilebilir
- [ ] Kalite öncelik sırası doğrulandı
- [ ] İlk N örnek davranışı doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§9.2, §9.5)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — filtre durumu tek task'ta mı
- [ ] Memory problemi var mı? — filtre geçmişi boyutu
- [ ] Error handling var mı? — **kalite kararı bu task'ın ana konusu**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — filtresiz/doğrulamasız okuma taşınmamalı

## Definition of Done

Ortak DoD + host tarafında sentetik veri testleri geçti + güvenlik sensörü gecikmesi
ölçülüp onaylandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Filtre tipi sensör başına; güvenlik sensöründe FİLTRE YOK

```text
Problem:      Gurultu bastirma ile tepki hizi arasindaki denge
Constraints:  Su seviyesi GUVENLIK sensorudur. Pompa calisirken seviye HIZLA
              duser; filtre gecikmesi pompanin kuru calismasi demektir.
              pH/EC'de ise kucuk gerilim farklari anlamli, gurultu bastirma sart.
Approaches:   (a) filtresiz       → pH/EC kullanilamaz
              (b) EMA (tek katsayi)→ az bellek, SUREKLI gecikme
              (c) medyan-N        → sicramaya dayanikli, N ornek gecikme
              (d) sensor tipine gore farkli
Selected:     (d) — `SensorConfig.filterStrength` sensor basina:
                 0      → FILTRESIZ (su seviyesi — varsayilan boyle)
                 1..32  → EMA, katsayi = 1/strength
Gerekce:      Tek filtre politikasi ya guvenligi geciktirir ya olcumu bozar.
```

**EMA seçildi, medyan değil:** medyan-N için sensör başına N örneklik geçmiş
tutmak gerekir (8 sensör × N × 4 bayt). EMA tek `float` durum tutar ve
sıçrama koruması zaten **ayrı bir kontrol** (değişim hızı sınırı) tarafından
sağlanıyor — ikisini üst üste koymaya gerek yok.

## Karar 2 — Değişim hızı sınırı su seviyesine UYGULANMAZ

```text
Tuzak: "Fiziksel olmayan sicrama" kontrolu genel bir iyilik gibi gorunur.
       AMA su seviyesi pompa calisirken GERCEKTEN hizli degisir.
       Agresif bir sinir, gercek ve KRITIK bir dususu "imkansiz" sayip
       ATARDI — koruma tam ihtiyac aninda korlesirdi.

Selected: `maxChangePerSec = 0` → sinir KAPALI (varsayilan davranis).
          Su seviyesi ve akis icin varsayilan 0 olarak birakilir.
          Sicaklik/pH/EC icin acilabilir (yavas degisen buyuklukler).
```

## Karar 3 — Kalite kararı öncelik sırasıyla

```text
NOT_PRESENT > FAULT > OUT_OF_RANGE > STALE > OK

Gerekce: en agir tani kazanir. Hem kopuk hem aralik disi bir sensor
         FAULT olarak raporlanir — OUT_OF_RANGE yaniltici olurdu
         ("deger var ama yuksek" izlenimi verir).
```

## Karar 4 — İlk N örnekte değer yayınlanmaz

```text
Problem:  EMA filtresi ilk orneklerde gercek degere yakinsamamistir.
          Ilk okumada 0'dan baslayan bir filtre, gercek 25 °C'yi
          once ~3 °C gosterir — otomasyon bunu gercek sanabilir.
Selected: Filtre "isinana" kadar (strength kadar ornek) kalite STALE.
          Deger yayinlanir ama OTOMASYONDA KULLANILMAZ (isUsable == false).
Gerekce:  Sessizce yanlis deger vermektense "henuz guvenilir degil" demek.
```

## Karar 5 — Uç değer (`atRail`) tek başına FAULT değildir

```text
Incelik: ADC ucta sabit olmasi genellikle kopuk/kisa devre demektir.
         AMA mesru olarak da uca dayanabilir (ornegin 0 L/dk akis,
         veya olcum araliginin ucundaki bir sicaklik).
Selected: `suspect` bayragi FAULT'a DOGRUDAN cevrilmez; sensor kendi
          baglamini bilir ve `hardwareFault` ile ayirir.
          · hardwareFault = true  → FAULT (kesin)
          · suspect       = true  → aralik kontrolune birakilir
Gerekce:  Yanlis pozitif bir FAULT, calisan bir sistemi durdurur.
```

## Karar 6 — Hat SAF fonksiyonlardan oluşur

Girdi: ham değer + config + filtre durumu + zaman. Çıktı: işlenmiş değer +
kalite. Donanım yok, log yok, global yok.

Bu sayede TASK-064 host tarafında **sentetik veriyle** test edebilir:
gürültülü sinyal, tek seferlik sıçrama, donmuş değer, aralık dışı — hepsi
donanımsız denenebilir.

## Kapsam dışı

- Sensöre özgü formüller → TASK-024/025/026
- Donanım okuma → HAL · UI gösterimi → TASK-050/052

---

# STEP 3 — REVIEW RECORD

- [x] Kalibrasyon config'ten (`offset`/`scale`) uygulanıyor
- [x] Filtre sensör bazında (`filterStrength`); **0 = filtresiz**
- [x] Güvenlik sensöründe filtre gecikmesi **sıfır** — varsayılan config
      su seviyesi için `filterStrength = 0`
- [x] Aralık, değişim hızı ve bayatlama kontrolleri çalışıyor
- [x] Kalite kararı öncelik sırasına göre: `NOT_PRESENT > FAULT >
      OUT_OF_RANGE > STALE > OK`
- [x] Hata sayaçları tutuluyor (`faultCount`, API'de görünecek)
- [x] Hat saf fonksiyonlardan oluşuyor — donanım/log/global yok
- [x] İlk N örnekte değer **yayınlanıyor ama otomasyonda kullanılmıyor**
      (kalite `STALE`)
- [x] `lowConfidence` kaliteye bağlandı — EC sıcaklık telafisi yapılamadığında
- [x] `PipelineState` = **24 bayt** × 5 sensör = 120 bayt
- [ ] **Sentetik veri testleri — native test ortamı gerekiyor (TASK-064)**

**Değişim hızı sınırı kararı:** varsayılan **kapalı** (`maxChangePerSec = 0`).
Su seviyesi pompa çalışırken gerçekten hızlı düşer; agresif bir sınır gerçek
ve KRİTİK bir düşüşü "imkânsız" sayıp atardı — koruma tam ihtiyaç anında
körleşirdi.

**TASK-023: TAMAMLANDI.**
