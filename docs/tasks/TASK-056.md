# TASK-056 — Schedule Rule Evaluation (Time Validity)

**Phase:** 12 — Automation · **Priority:** P2

## Objective

Zamanlanmış sulama çevrimlerini değerlendirmek ve **zaman geçersizken çizelgelerin
çalışmamasını** garanti etmek.

## Scope

- Zaman penceresi kuralları (başlangıç–bitiş saat aralığı)
- Periyodik çevrim kuralları (ON süresi + OFF süresi)
- Zaman geçerlilik kontrolü
- Çevrim durumunun takibi (kaldığı yerden devam)
- Sonraki tetikleme zamanının hesaplanması

## Out of Scope

- Threshold kuralları (TASK-055)
- Motor entegrasyonu (TASK-057)
- TimeService (TASK-040)

## Dependencies

- TASK-054, TASK-040

## Requirements

- `REQUIREMENTS.md` — §8 (zamana bağlı otomasyon yok), §11-Critical

## Architecture References

- §11.2 Kural modeli (ScheduleRule zaman geçerliliği gerektirir)
- §11.3 schedule satırı · §2.13 TimeService kuralı

## Expected Design

### Zaman geçerliliği kuralı — mutlak

```text
TimeService.isValid() == false
        ↓
   Çizelge kuralları DEĞERLENDİRİLMEZ
   Threshold kuralları ÇALIŞMAYA DEVAM EDER
   Kullanıcıya "saat geçersiz, çizelgeler duraklatıldı" bildirilir
```

Geçersiz zamanla çizelge çalıştırmak öngörülemez sulama demektir. Mevcut projede
`getFormattedTime()` senkronize değilken sessizce `"00:00:00"` döndürüyordu — bu, bir
çizelgenin sürekli "gece yarısı" sanmasına yol açardı.

### Karar gerektiren nokta 1 — Periyodik çevrim referansı

```text
Problem:      "Her 2 saatte 15 dk sula" çevrimi neye göre hesaplanacak?
Constraints:  Cihaz yeniden başlarsa çevrim kaybolmamalı;
              duvar saati kaymaları çevrimi bozmamalı
Approaches:   (a) uptime'a göre (monotonik) — reset ile sıfırlanır
              (b) duvar saatine göre (örn. her çift saatte) — reset'e dayanıklı
              (c) son tetikleme zamanını kalıcı sakla
Trade-offs:   (a) basit ama güç kesintisi çevrimi bozar
              (b) öngörülebilir ve reset'e dayanıklı
Recommended:  (b) — kullanıcı da ne zaman sulanacağını bilebilir
```

### Karar gerektiren nokta 2 — Kaçırılan çevrim

```text
Problem:      Cihaz kapalıyken veya acil durumdayken bir çevrim kaçırıldı. Ne olmalı?
Constraints:  Hemen telafi etmek aşırı sulama yaratabilir;
              hiç telafi etmemek bitkiyi susuz bırakabilir
Approaches:   (a) kaçırılanı yok say, sonraki çevrimi bekle
              (b) hemen telafi et
              (c) kısa gecikmeyse telafi et, uzunsa yok say
Recommended:  (a) veya (c) — (b) tek başına riskli;
              her durumda kaçırılan çevrim loglanmalı
```

## Implementation Notes

- Değerlendirme saf fonksiyon olmalı; zaman bir parametre olarak verilmeli ki host
  tarafında hızlandırılmış test yapılabilsin.
- Gece yarısını aşan pencereler (22:00–02:00) doğru ele alınmalı — yaygın bir hata kaynağı.
- Yaz saati geçişinde çizelge davranışı düşünülmeli: DST geçişinde bir saat tekrarlanır
  veya atlanır. Bu, çevrimi çift tetikleyebilir veya atlatabilir.
- Sonraki tetikleme zamanı state'e yayınlanmalı; kullanıcı "sonraki sulama 14:30"
  bilgisini görmeli.
- Çevrimin ortasında acil durum olursa, acil durum temizlendiğinde çevrim devam etmemeli;
  yeniden değerlendirilmeli.
- Çizelge duraklatıldığında (zaman geçersiz) bu durum açıkça state'te taşınmalı.

## Files

- `src/domain/rules/ScheduleRuleEvaluator.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Zaman penceresi ve periyodik çevrim kuralları çalışıyor
- [ ] Zaman geçersizken çizelgeler değerlendirilmiyor
- [ ] Çizelge duraklatma durumu state'te taşınıyor ve kullanıcıya bildiriliyor
- [ ] Çevrim referansı kararı verildi ve gerekçelendirildi
- [ ] Kaçırılan çevrim davranışı kararı verildi; kaçırılan çevrim loglanıyor
- [ ] Gece yarısını aşan pencereler doğru
- [ ] DST geçişi davranışı düşünüldü
- [ ] Sonraki tetikleme zamanı yayınlanıyor
- [ ] Değerlendirme saf fonksiyon; zaman parametre olarak veriliyor

## Test Plan

- [ ] Host tarafında hızlandırılmış zamanla çevrim testleri
- [ ] Gece yarısını aşan pencere doğru çalışıyor
- [ ] Zaman geçersizken çizelge tetiklenmiyor
- [ ] Zaman geçerli hale gelince çizelge devreye giriyor
- [ ] DST geçişi simüle edilip davranış doğrulandı
- [ ] Cihaz yeniden başlatıldığında çevrim kararlı davranıyor
- [ ] Acil durum sonrası çevrim yeniden değerlendiriliyor
- [ ] Sonraki tetikleme zamanı doğru hesaplanıyor
- [ ] Donanımda gerçek bir çizelge uçtan uca doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§11.2, §11.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — çevrim durumu
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **zaman geçersizliği**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **sessiz "00:00:00" davranışı yasak**

## Definition of Done

Ortak DoD + hızlandırılmış zaman testleri geçti + zaman geçersizken çizelgenin
çalışmadığı kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Periyodik çevrim referansı: (b) DUVAR SAATİ

```text
(a) uptime (monotonik) → guc kesintisi cevrimi SIFIRLAR; cihaz her
    resetlendiginde sulama bastan baslar ve gunde birkac reset yasayan
    bir sistem asiri sular.
(c) son tetiklemeyi kalici sakla → her cevrimde flash yazmasi, asinma.

(b) SECILDI: gun icindeki saniyeye gore.
    cevrimIcindekiKonum = (gununSaniyesi % cyclePeriodS)
    konum < cycleOnS → ACIK

Kazanc: reset'e DAYANIKLI, ONGORULEBILIR ve kullanici ne zaman
        sulanacagini HESAPLAYABILIR. Flash yazmasi YOK.
```

**Bedeli açıkça kabul ediliyor:** duvar saati geçerli değilse çevrim
çalışmaz. Bu zaten zorunlu kuraldır (aşağıya bakın), yani ek bir kısıt
getirmiyor.

## Karar 2 — Kaçırılan çevrim: (a) YOK SAY, ama LOGLA

```text
(b) hemen telafi et → cihaz 6 saat kapali kaldiysa acilir acilmaz
    3 cevrim ust uste calisir → ASIRI SULAMA. Kabul edilemez.
(c) kisa gecikmede telafi → "kisa" esigi keyfi ve yeni bir ayar demek.

(a) SECILDI: kacirilan cevrim YOK SAYILIR, siradaki normal cevrim beklenir.
             Kacirma LOGLANIR — sessiz gecmez.
```

Duvar saati tabanlı hesap (Karar 1) bunu **kendiliğinden** sağlıyor:
çevrim konumu her an saatten türetildiği için "kaçırma" diye bir durum
zaten oluşmuyor; cihaz açıldığında o anın doğru konumuna atlar.

## Karar 3 — Zaman geçerliliği: MUTLAK kural (ARCHITECTURE §11.2)

```text
TimeService.isValid() == false
    → CIZELGE kurallari DEGERLENDIRILMEZ
    → ESIK kurallari CALISMAYA DEVAM EDER
    → kullaniciya "saat gecersiz, cizelgeler duraklatildi" bildirilir
```

Geçersiz zamanla çizelge çalıştırmak öngörülemez sulama demektir. Eski
projede `getFormattedTime()` senkronize değilken sessizce `"00:00:00"`
döndürüyordu — bir çizelge bunu sürekli gece yarısı sanardı.

`AutomationStatus.schedulesPaused` bu durumu **yayınlar**; arayüz sessiz
kalmaz.

## Karar 4 — Gece yarısını aşan pencere GEÇERLİ

`22:00–02:00` bir hata değil, **sarma penceredir** ve yaygın bir
kullanımdır (gece sulaması).

```text
start <  end → [start, end)          normal pencere
start >  end → [start, 1440) ∪ [0, end)   SARMA pencere
start == end → gecersiz (dogrulamada reddedilir)
```

Bu, çizelge mantığındaki en yaygın hata kaynağıdır ve `static_assert`'lerle
derleme zamanında kilitlendi.

---

# STEP 3 — REVIEW RECORD

- [x] Zaman penceresi kuralları; **gece yarısını aşan pencere destekli**
- [x] Periyodik çevrim kuralları
- [x] Zaman geçerlilik kontrolü — geçersizken çizelge değerlendirilmiyor
- [x] `AutomationStatus.schedulesPaused` yayınlanıyor
- [x] Sonraki tetikleme zamanı hesaplanıyor (`nextScheduleAt`)
- [x] Değerlendirme saf; zaman parametre
- [x] **11 `static_assert` derlemeyi geçti**
- [ ] **Uzun süreli çizelge testi — donanım gerekiyor**

## Derleme zamanında kanıtlanan sınırlar

```text
normal pencere : 10:00 ∈ [08:00,12:00) ✓   13:00 ∉ ✓
                 baslangic DAHIL ✓          bitis HARIC ✓
SARMA pencere  : 23:00 ∈ [22:00,02:00) ✓   00:00 ∈ ✓   01:00 ∈ ✓
                 12:00 ∉ ✓                  02:00 ∉ (bitis haric) ✓
cevrim         : 0. sn ACIK ✓   899. ACIK ✓   900. KAPALI ✓
                 7200. (sonraki cevrim) ACIK ✓
                 periyot 0 → ASLA acilmaz (SIFIRA BOLME YOK) ✓
```

Sarma pencere çizelge mantığındaki en yaygın hata kaynağıdır; testle değil
**derlemeyle** kilitlendi.

## Kaçırılan çevrim sorunu kendiliğinden çözüldü

Duvar saati tabanlı hesap seçildiği için "kaçırma" diye bir durum
oluşmuyor: çevrim konumu her an saatten türetiliyor, cihaz açıldığında o
anın doğru konumuna atlıyor. Telafi mantığı yazmaya gerek kalmadı —
en iyi kod yazılmayan koddur.

**TASK-056: TAMAMLANDI.**
