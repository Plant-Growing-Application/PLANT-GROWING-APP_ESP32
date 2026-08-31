# TASK-051 — Screen Framework & Navigation

**Phase:** 11 — Display · **Priority:** P2

## Objective

Ekranlar arası gezinme, sayfa içi düzenleme ve öncelikli ekran (acil durum) mantığını
kuran çerçeveyi oluşturmak.

## Scope

- Ekran kayıt ve gezinme modeli
- Girdi olaylarının navigasyona çevrilmesi
- Sayfa içine girme / çıkma (düzenleme modu)
- Öncelikli ekran mekanizması (acil durum diğer ekranların üzerine gelir)
- Ekran değişiminde tam yeniden çizim

## Out of Scope

- Ekran içerikleri (TASK-052)
- ViewModel üretimi (TASK-050)
- Komut gönderimi (TASK-053)

## Dependencies

- TASK-050, TASK-021

## Requirements

- `REQUIREMENTS.md` — §6.1 (menü sistemi `[~]`, kullanıcı etkileşimi)

## Architecture References

- §13.3 Ekran yapısı · §13.4 Girdi işleme

## Expected Design

### Ekran yapısı (§13.3)

```text
  ┌─ STATUS BAR (tüm ekranlarda sabit) ────────────────┐
  │  saat · Wi-Fi ikonu+RSSI · mod rozeti · hata sayısı │
  └────────────────────────────────────────────────────┘

  HOME · SENSORS · CONTROL · NETWORK · SYSTEM · ALERTS
  (encoder ile yatay gezinme)

  EMERGENCY → öncelikli, diğerlerinin üzerine gelir
```

### Öncelikli ekran

Acil durum mandallıyken `EMERGENCY` ekranı devreye girer ve **onay verilmeden kapanmaz**.
Kullanıcı diğer ekranlara geçebilmeli mi? Karar gerekçelendirilmeli: tamamen kilitlemek
teşhisi zorlaştırır, hiç kilitlemek uyarıyı görünmez kılar. Öneri: geçişe izin ver ama
durum çubuğunda kalıcı uyarı göster ve acil ekrana dönüşü kolaylaştır.

### Karar gerektiren nokta — Düzenleme modu

```text
Problem:      OLED'den ayar değiştirme (örn. mod seçimi) nasıl olacak?
Constraints:  Tek encoder + birkaç buton;
              yanlışlıkla ayar değiştirmek tehlikeli olabilir
Approaches:   (a) sayfa içine gir → encoder değeri değiştirir → onayla çık
              (b) yalnızca görüntüleme, ayar web'den
              (c) sınırlı düzenleme: yalnızca kritik olmayan alanlar
Trade-offs:   (b) en güvenli ama ağsız senaryoda kullanıcıyı çaresiz bırakır;
              acil durdurma ve mod değişimi OLED'den erişilebilir olmalı
Recommended:  (c) — kritik eylemler (acil durdurma, mod) onay adımıyla;
              detaylı ayarlar web'de
```

## Implementation Notes

- Ekran değişiminde tam yeniden çizim, aynı ekranda kirli alan güncellemesi yapılmalı.
- Gezinme dairesel mi olmalı, uçlarda durmalı mı? Dairesel gezinme az sayıda ekranda
  kullanışlıdır ancak kullanıcı nerede olduğunu kaybedebilir; durum çubuğunda konum
  göstergesi yardımcı olur.
- Zaman aşımı davranışı: kullanıcı bir süre işlem yapmazsa HOME'a dönmeli mi? Sera
  ortamında ekran uzun süre açık kalabilir; OLED yanma (burn-in) riski için ekran
  karartma değerlendirilmeli.
- Düzenleme modundan çıkmadan başka ekrana geçilememeli (yarım kalmış düzenleme).
- Girdi olayları kuyruğu boşaltılmalı; birikmiş olaylar ekranın hızla kaymasına yol açar.
- Acil durum tetiklendiğinde ekran otomatik olarak `EMERGENCY`'ye geçmeli.

## Files

- `src/interfaces/ui/ScreenFramework.h` / `.cpp` (yeni)
- `src/interfaces/ui/Navigation.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Altı ana ekran + öncelikli acil durum ekranı kayıtlı
- [ ] Encoder ile gezinme çalışıyor
- [ ] Sayfa içine girme/çıkma çalışıyor
- [ ] Düzenleme modu kararı verildi ve uygulandı
- [ ] Acil durum tetiklendiğinde ekran otomatik geçiyor
- [ ] Acil durum ekranı davranışı gerekçelendirildi
- [ ] Ekran değişiminde tam, aynı ekranda kirli alan güncellemesi
- [ ] Düzenleme yarıda bırakılamıyor
- [ ] Girdi olayları birikmiyor
- [ ] Ekran karartma/zaman aşımı kararı verildi

## Test Plan

- [ ] Tüm ekranlar arasında gezinme çalışıyor
- [ ] Hızlı encoder çevirmede ekran atlaması/kayması olmuyor
- [ ] Sayfa içine girip çıkmada durum korunuyor
- [ ] Acil durum tetiklendiğinde ekran otomatik geçiyor
- [ ] Acil durum sırasında diğer ekranlarda uyarı görünüyor
- [ ] Düzenleme modunda başka ekrana geçilemiyor
- [ ] Ekran değişim süresi ölçüldü
- [ ] Uzun süreli kullanımda ekran kararlı

## Review Checklist

- [ ] Architecture'a uygun mu? (§13.3, §13.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — navigasyon durumu tek task'ta
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — OLED yokken çerçeve çalışıyor mu
- [ ] ESP32 resource kullanımı uygun mu? — I2C yükü
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — sayfa geçişinde Wi-Fi modu değiştirme deseni yasak

## Definition of Done

Ortak DoD + gezinme akışları test edildi + acil durum ekranı otomatik geçişi doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Düzenleme modu: (c) sınırlı düzenleme + onay adımı

```text
(b) yalnizca goruntuleme → REDDEDILDI: agsiz senaryoda kullaniciyi caresiz
    birakir. Acil durdurma OLED'den erisilebilir OLMALIDIR.
(a) her sey duzenlenebilir → yanlislikla ayar degistirmek tehlikeli.

SECILDI (c): OLED'den YALNIZCA su eylemler:
    · ACIL DURDURMA        (onay adimiyla)
    · Acil durumu temizle  (onay adimiyla)
    · Aktuator ac/kapa     (onay adimiyla)
    · Yeniden baslat       (onay adimiyla)
Sayisal ayarlar (esikler, kalibrasyon, zaman dilimi) YALNIZCA web'den.
```

**Onay adımı:** encoder'a bas → "ONAYLA?" → tekrar bas. Tek basışla pompa
çalıştırmak, cebe giren bir cihazda kabul edilemez.

## Karar 2 — Öncelikli ekran: geçişe İZİN VER, uyarıyı KALICI kıl

```text
Tamamen kilitlemek → teshisi zorlastirir; operator sensor ekranina bakip
                     NEDEN acil duruma gecildigini anlayamaz.
Hic kilitlemek     → uyari gorunmez olur.

SECILDI: Acil durum olunca ekran OTOMATIK olarak EMERGENCY'ye gecer.
         Kullanici diger ekranlara GECEBILIR, ama durum cubugunda kalici
         "ACIL" rozeti kalir ve BACK tusu her yerden EMERGENCY'ye doner.
```

## Karar 3 — Gezinme DAİRESEL DEĞİL, uçlarda durur

Dairesel gezinme az sayıda ekranda kullanışlıdır ama kullanıcı nerede
olduğunu kaybeder. Uçlarda durmak "listenin sonundayım" bilgisini
fiziksel olarak verir. Durum çubuğunda ayrıca konum göstergesi var.

## Karar 4 — Zaman aşımı: 60 sn sonra HOME'a dön

Sera ortamında ekranın bir alt menüde takılı kalması, yanına gelen birinin
sistemin durumunu göremeyeceği anlamına gelir. **İstisna:** `EMERGENCY`
ekranından otomatik dönüş YOKTUR ve onay bekleyen bir işlem varsa sayaç
işlemez.

## Karar 5 — Ekran değişiminde TAM yeniden çizim

Aynı ekranda `UiModel` karşılaştırmasıyla kirli tespiti yapılır; ekran
değişiminde koşulsuz tam çizim. Kısmi kalıntı, eski projedeki koordinat
tutarsızlıklarının görünür sonucuydu.

---

# STEP 3 — REVIEW RECORD

- [x] Altı gezinilebilir ekran + öncelikli `EMERGENCY`
- [x] Girdi olayları navigasyona çevriliyor
- [x] **Onay adımı**: her eylem iki basış ister; encoder çevirmek onayı iptal eder
- [x] Acil durumda ekran otomatik geçiyor; BACK her yerden `EMERGENCY`'ye döner
- [x] Gezinme dairesel **değil**, uçlarda duruyor
- [x] Boşta kalma 60 sn sonra HOME'a döner; `EMERGENCY`'de ve onay
      beklerken sayaç işlemez
- [x] Düzen sabitleri merkezî (`layout` namespace); çıplak koordinat yok
- [ ] **Kullanılabilirlik testi bekliyor**

## OLED'den ne yapılabilir, ne yapılamaz

```text
YAPILIR: ACIL DURDURMA · acil durumu temizle · aktuator ac/kapa · yeniden baslat
YAPILMAZ: esikler · kalibrasyon · zaman dilimi · ag ayarlari
```

Sayısal ayarların OLED'den girilmesi tek encoder ile hem yavaş hem hataya
açıktır. Ama "yalnızca görüntüleme" de kabul edilmedi: ağsız bir senaryoda
acil durdurmaya erişememek kabul edilemez.

**TASK-051: TAMAMLANDI.**
