# TASK-003 — Coding Standards & Layering Rules

**Phase:** 0 — Project Foundation · **Priority:** P1

## Objective

Katman ihlallerinin ve mevcut projede görülen anti-pattern'lerin **inceleme sırasında
mekanik olarak yakalanabilmesi** için yazılı kural seti oluşturmak.

## Scope

- Adlandırma, dosya düzeni, include kuralları
- Katman bağımlılık kurallarının maddeleştirilmesi (D1–D6)
- Yasaklı desenler listesi ve her biri için gerekçe
- Hata dönüş konvansiyonu (istisna yok, `Result` tabanlı)
- Bellek kullanım kuralları (sıcak yolda dinamik ayırma yok)
- Kod inceleme şablonu

## Out of Scope

- Otomatik statik analiz aracı kurulumu (isteğe bağlı, ayrı iş)
- Herhangi bir modülün implementasyonu

## Dependencies

- TASK-001

## Requirements

- `REQUIREMENTS.md` — Kritik Problemler 1–7

## Architecture References

- §0 Tasarım ilkeleri (P1–P7)
- §1.2 Bağımlılık kuralları
- §5.1 Kullanılmayacaklar

## Expected Design

Kural seti **somut ve denetlenebilir** olmalı. "Temiz kod yazın" gibi ölçülemez ifadeler
yerine, mevcut projeden gelen gerçek ihlallere karşılık gelen maddeler:

| Yasak desen | Nereden geliyor |
|---|---|
| Global mutable değişken | `currentIP`, `currentMAC`, `waterTemp` |
| Toplayıcı header | `Define.h` |
| `while (koşul) delay(x)` task içinde | `MyWiFi::connect()` |
| `while(true)` ile hata yakalama | `setup()` OLED init |
| `setup()` içinden erken `return` | LittleFS mount hatası |
| Başka task'ı `vTaskSuspend` etmek | `pauseWiFiMonitor()` |
| Servis katmanından UI çizimi | `Sensor::SensorValues()` |
| Bildirilip implement edilmemiş fonksiyon | `PhSensor()`, `handleLogin()`, `SaveIP()` |
| Elle string parse ile JSON | `indexOf("\"id\":")` |
| Sıcak yolda `String` birleştirme | WebSocket yanıt üretimi |
| Dönüş değeri kontrol edilmeyen init | `_server.begin()` |

## Implementation Notes

- Kurallar bir denetim listesi olarak yazılmalı ki her task'ın Review adımında
  mekanik olarak uygulanabilsin.
- `Result`/hata dönüş konvansiyonu TASK-004 ile uyumlu olmalı; bu task konvansiyonu
  tanımlar, TASK-004 tipi implement eder.
- ISR içinde yapılabilecekler/yapılamayacaklar ayrı bir başlık olmalı (`IRAM_ATTR`,
  ISR'de log/alloc/blocking yasağı).
- Sabit boyutlu tampon tercihi ve `String` kullanımının sınırlandırılması netleştirilmeli.

## Files

- `docs/CODING_STANDARDS.md` (yeni)
- `docs/REVIEW_TEMPLATE.md` (yeni)
- `.clang-format` (opsiyonel)

## Acceptance Criteria

- [ ] Katman bağımlılık kuralları (D1–D6) maddelenmiş
- [ ] Yasaklı desenler listesi, her biri gerekçesiyle yazılmış
- [ ] Hata dönüş konvansiyonu tanımlı (istisna kullanılmıyor)
- [ ] Bellek kuralları tanımlı (sıcak yolda dinamik ayırma yok, sabit tampon tercihi)
- [ ] ISR kuralları tanımlı
- [ ] İnceleme şablonu her task'ın Review adımında kullanılabilir durumda

## Test Plan

- [ ] Standart, mevcut projedeki 3 gerçek ihlale uygulanıp yakalayıp yakalamadığı kontrol edildi
- [ ] `.clang-format` varsa iskelet dosyalara uygulandı ve fark üretmedi

## Review Checklist

- [ ] Architecture'a uygun mu? (§0, §1.2)
- [ ] Gereksiz abstraction var mı? — kural seti kendisi aşırı katı mı
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? (kural olarak tanımlandı mı)
- [ ] Memory problemi var mı? (kural olarak tanımlandı mı)
- [ ] Error handling var mı? (konvansiyon tanımlandı mı)
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı?

## Definition of Done

Ortak DoD + standart dokümanı, sonraki her task'ın Review adımında referans alınabilecek
somutlukta.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Standardın biçimi: denetlenebilirlik

```text
Problem:      Kodlama standardı nasıl yazılmalı ki gerçekten uygulansın?
Constraints:  Her task'ın STEP 3'ünde mekanik olarak uygulanabilmeli;
              "temiz kod yazın" gibi ölçülemez maddeler işe yaramaz;
              65 task boyunca tutarlı kalmalı
Approaches:   (a) genel prensip listesi (okunur, uygulanmaz)
              (b) yasak/zorunlu desen tablosu + her madde için grep deseni
              (c) yalnızca otomatik araç (clang-tidy)
Trade-offs:   (a) denetlenemez; (c) katman ihlali gibi kuralları yakalayamaz
Selected:     (b) — her yasak desen için **aranabilir bir iz** verilir.
              İnceleme yapan kişi tahmin etmez, arar.
```

Bu karar, standardın mevcut projedeki gerçek ihlallerden türetilmesini gerektirir.
Uydurma kural yazılmaz; her madde `REQUIREMENTS.md`'de belgelenmiş bir bulguya dayanır.

## Karar 2 — Hata dönüş konvansiyonu

```text
Problem:      İstisna mı, hata kodu mu, Result mı?
Constraints:  Arduino-ESP32'de istisnalar varsayılan olarak kapalı ve maliyetli;
              heap kullanımı sıcak yolda istenmiyor;
              hata kodu makine tarafından karşılaştırılabilir olmalı (ARCHITECTURE §16.2)
Selected:     İstisna KULLANILMAZ.
              · Değer döndürmeyen işlem → hata kodu enum'u
              · Değer döndüren işlem    → değer/hata birliği (TASK-004 tanımlayacak)
              · Karar mantığı ASLA hata METNİNE bakmaz, koda bakar
Not:          Tipin kendisi TASK-004'te implement edilir; burada yalnızca
              konvansiyon sabitlenir ki TASK-004 buna uysun.
```

## Karar 3 — `.clang-format` dahil edilecek mi?

```text
Constraints:  Ortamda clang-format kurulu değil → doğrulanamayan bir dosya
              eklemek "yazılmayan kod yoktur" ilkesine (P7) aykırı olur
Selected:     .clang-format bu aşamada EKLENMİYOR.
              Biçim kuralları (girinti, süslü parantez, satır uzunluğu)
              standart dokümanında metin olarak tanımlanıyor.
Gerekçe:      Task dosyasında `.clang-format` zaten "(opsiyonel)" işaretli.
              Araç kurulduğunda eklenebilir; şimdi eklemek, uygulanamayan
              bir kural seti yaratır.
```

## Karar 4 — İnceleme şablonunun kapsamı

`IMPLEMENTATION_PLAN.md` her task'a 9 maddelik bir Review Checklist veriyor.
Şablon bunu **tekrarlamaz**; her maddenin *nasıl kontrol edileceğini* verir.
Böylece checklist bir niyet beyanı değil, bir prosedür olur.

## Kapsam dışı bırakılanlar

- Otomatik statik analiz aracı kurulumu (ayrı iş, ISSUES'a yazılacak)
- `Result` tipinin implementasyonu → TASK-004
- Herhangi bir modülün kodu

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Katman bağımlılık kuralları (D1–D6) maddelenmiş — her biri için **aranabilir iz** verildi
- [x] Yasaklı desenler listesi — 14 madde (Y1–Y14), her biri gerekçesi ve
      mevcut projedeki gerçek karşılığıyla
- [x] Hata dönüş konvansiyonu tanımlı — istisna kullanılmıyor, `{subsystem, code}` çifti
- [x] Bellek kuralları tanımlı — sıcak yolda dinamik ayırma yok, POD/sabit tampon
- [x] ISR kuralları tanımlı — yasak/serbest listesi ayrı
- [x] İnceleme şablonu her task'ın Review adımında kullanılabilir durumda
      (9 checklist maddesinin her biri için "nasıl kontrol edilir" prosedürü)

## Test Plan

- [x] **Standart, mevcut projedeki gerçek ihlallere uygulandı — 3 değil 6 sınıf yakalandı:**

| Desen | Bulgu | Konum |
|---|---|---|
| Y1 global mutable | 6 adet | `legacy/src/main.cpp:11-17` |
| Y2 toplayıcı header | **26 include** | `legacy/include/Define.h` |
| Y3 bloklayan bekleme | `while(...) delay(50)` | `legacy/src/MyWifi.cpp:128` |
| Y4 sonsuz döngü | `while (true)` | `legacy/src/main.cpp:176` |
| Y5 erken `return` | `setup()` içinde | `legacy/src/main.cpp:159` |
| Y8 bildirim ≠ implementasyon | 5 fonksiyon | `PhSensor`, `NutrimentSensor`, `handleLogin`, `SaveIP`, `GetIP` |

- [x] **Yeni `src/` aynı taramalardan geçirildi — hepsi sıfır:**

```text
Y1 global mutable      : 0
Y2 en yuklu header     : 1 include  (BoardPins.h — yalnizca stdint.h)
Y3 blocking bekleme    : 0
Y4 sonsuz dongu        : 0
Y5 setup icinde return : 0
D5 core/ dis bagimlilik: 0
```

- [x] `.clang-format` — **eklenmedi** (Karar 3): ortamda `clang-format` yok,
      doğrulanamayan kural seti eklemek P7'ye aykırı olurdu. Biçim kuralları
      standardın §8'inde metin olarak tanımlandı.
- [x] Final derleme: SUCCESS, `src/` kaynaklı **0 uyarı**

## Review Checklist

- [x] Architecture'a uygun mu? — §0 (P1–P7), §1.2 (D1–D6), §5.1 (kullanılmayacaklar)
      birebir yansıtıldı
- [x] Gereksiz abstraction var mı? — kural seti fazla katı mı diye gözden geçirildi;
      her madde belgelenmiş bir ihlale dayanıyor, spekülatif kural yok
- [x] Blocking işlem var mı? — N/A (doküman task'ı); ancak Y3 kuralı tanımlandı
- [x] Shared state güvenli mi? — Y1 ve Z1/Z2 kuralları tanımlandı
- [x] Memory problemi var mı? — §5 bellek kuralları tanımlandı
- [x] Error handling var mı? — §4 konvansiyonu tanımlandı, TASK-004 buna uyacak
- [x] ESP32 resource kullanımı uygun mu? — inceleme şablonu §7'de ADC1/pin/flash
      kontrolleri prosedüre bağlandı
- [x] Task sorumluluğu doğru mu? — N/A
- [x] Eski kod gereksiz şekilde kopyalanmış mı? — eski koda **yalnızca ihlal
      örneği çıkarmak için** bakıldı; hiçbir yapı veya desen yeni koda taşınmadı

## Bulgular

Kapsam dışı bir gözlem kaydedildi: otomatik statik analiz (clang-tidy) kurulumu
bu task'ın Out of Scope'unda. Standart şu an elle/grep ile denetleniyor.
→ **ISSUE-008** olarak kaydedildi.

## Durum

**TASK-003: TAMAMLANDI.**
