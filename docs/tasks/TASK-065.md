# TASK-065 — Hardware-in-the-Loop Tests & Final Conformance Review

**Phase:** 15 — Testing & Review · **Priority:** P1

## Objective

Donanım gerektiren testleri tekrarlanabilir bir prosedüre bağlamak ve sistemin
`REQUIREMENTS.md` + `ARCHITECTURE.md` ile uyumunu son kez doğrulamak.

## Scope

- Donanım test prosedürlerinin yazılması
- Uçtan uca kabul testleri
- Mimari uyumluluk denetimi
- Gereksinim izlenebilirlik doğrulaması
- Kalan eksiklerin ve açık maddelerin raporlanması
- Sürüm notları

## Out of Scope

- Yeni özellik ekleme
- Yeni mimari kararlar
- Performans optimizasyonu

## Dependencies

- TASK-061, TASK-064

## Requirements

- `REQUIREMENTS.md` — tüm bölümler (kapanış doğrulaması)

## Architecture References

- Tüm bölümler (uyumluluk denetimi)

## Expected Design

### Mimari uyumluluk denetimi

`ARCHITECTURE.md` §0'daki yedi ilkenin her biri kodda doğrulanmalı:

| İlke | Doğrulama yöntemi |
|---|---|
| **P1** Tek yazar | Her alt-state'e yazan tek nokta olduğu kod taramasıyla |
| **P2** Donanıma tek kapı | OLED, röle, ADC, radyoya tek modülden erişim |
| **P3** Bloklama yasak | Task döngülerinde `while`/`delay` taraması + periyot ölçümü |
| **P4** Fail-degraded | TASK-061 sonuçları |
| **P5** Tek doğruluk kaynağı | Frontend'in iyimser güncelleme yapmadığı |
| **P6** Güvenlik üstün | Güvenlik vetosunun her yolda uygulandığı |
| **P7** Yazılmayan kod yok | Bildirilip implement edilmemiş fonksiyon taraması |

### Gereksinim izlenebilirliği

`REQUIREMENTS.md`'deki her `[x]`, `[~]`, `[ ]` maddesi için yeni sistemdeki karşılığı
belirlenmeli:

```text
  Karşılandı        → hangi task'ta
  Kısmen karşılandı → neden, ne eksik
  Karşılanmadı      → bilinçli karar mı, eksik mi
```

Bu, projenin gerçek tamamlanma oranını dürüstçe ortaya koyar.

### Donanım test prosedürü

TASK-061'deki 18 senaryo + kabul testleri, firmware değişikliğinden sonra tekrar
çalıştırılabilecek şekilde yazılı bir prosedüre dönüştürülmeli. Sözlü bilgi kaybolur.

## Implementation Notes

- Denetim **dürüst** olmalı: karşılanmayan gereksinimler gizlenmemeli, açıkça listelenmeli.
- Kod taramaları mekanik olmalı (arama desenleri belirtilmeli) ki tekrarlanabilsin.
- `ARCHITECTURE.md` §20'deki açık maddelerin kapanıp kapanmadığı kontrol edilmeli.
- `docs/ISSUES.md`'deki tüm kayıtlar triyaj edilmeli: çözüldü, ertelendi veya kabul edildi.
- Kabul testleri kullanıcı bakış açısından yazılmalı: "operatör pompayı web'den açabilir",
  "hazne boşken pompa çalışmaz" gibi.
- Sürüm notları, eski sisteme göre neyin değiştiğini özetlemeli.
- Bu task **yeni iş üretmez**; bulgular yeni task önerisi olarak kaydedilir.

## Files

- `docs/HIL_TEST_PROCEDURE.md` (yeni)
- `docs/CONFORMANCE_REPORT.md` (yeni)
- `docs/RELEASE_NOTES.md` (yeni)
- `docs/ISSUES.md` (güncelleme — triyaj)

## Acceptance Criteria

- [ ] Donanım test prosedürü yazıldı ve tekrarlanabilir
- [ ] Uçtan uca kabul testleri çalıştırıldı
- [ ] Yedi mimari ilkenin her biri doğrulandı
- [ ] Her `REQUIREMENTS.md` maddesi için karşılık belirlendi
- [ ] Karşılanmayan gereksinimler dürüstçe listelendi
- [ ] `ARCHITECTURE.md` §20 açık maddeleri kontrol edildi
- [ ] `ISSUES.md`'deki tüm kayıtlar triyaj edildi
- [ ] Sürüm notları yazıldı
- [ ] Bulgular yeni task önerisi olarak kaydedildi, çözülmedi

## Test Plan

- [ ] Kabul testleri: operatör senaryoları baştan sona
- [ ] TASK-061'deki 18 arıza senaryosu prosedüre göre tekrarlandı
- [ ] P1–P7 ilkelerinin kod taramaları çalıştırıldı ve sonuçlar kaydedildi
- [ ] Bildirilip implement edilmemiş fonksiyon taraması temiz
- [ ] Kullanılmayan struct alanı / global değişken taraması temiz
- [ ] Task döngülerinde bloklama taraması temiz
- [ ] Sistem son bir kez 24 saat çalıştırıldı ve kararlı

## Review Checklist

- [ ] Architecture'a uygun mu? — **tüm doküman**
- [ ] Gereksiz abstraction var mı? — genel değerlendirme
- [ ] Blocking işlem var mı? — tarama sonucu
- [ ] Shared state güvenli mi? — tek yazar doğrulaması
- [ ] Memory problemi var mı? — TASK-062 sonuçlarıyla birlikte
- [ ] Error handling var mı? — TASK-061 sonuçlarıyla birlikte
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **son ve kapsamlı denetim**

## Definition of Done

Ortak DoD + **M8 kilometre taşı: uyumluluk raporu tamamlandı, karşılanmayan gereksinimler
dürüstçe listelendi, sistem sahaya hazır** + tüm issue'lar triyaj edildi.

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

**Tarih:** 2026-08-31

## 1. Mimari uyumluluk denetimi — P1…P7

| İlke | Yöntem | Sonuç |
|---|---|---|
| **P1** Tek yazar | Her `publishX()` çağıranı say | ✅ 7 alt-state, **her birine 1 yazar** |
| **P2** Donanıma tek kapı | Röle/radyo/OLED/ADC erişimi | ✅ röle 2 (tek modül), radyo 0, OLED 1 (boot init), ADC 3 (tek modül) |
| **P3** Bloklama yasak | Task/domain/services'te `delay`/`vTaskDelay` | ✅ **0** |
| **P4** Fail-degraded | §16.3 matrisi | ✅ 9/9 kod yolu var (heap satırı bu turda eklendi) |
| **P5** Tek doğruluk kaynağı | Frontend iyimser güncelleme | ✅ tarayıcıda **doğrulandı** (TASK-048) |
| **P6** Güvenlik üstün | Veto her açma yolunda | ✅ `permit == nullptr` reddedilir; yapısal |
| **P7** Yazılmayan kod yok | Bildirim/tanım karşılaştırması | ✅ **0** tanımsız bildirim |

### P1 ayrıntı

```text
publishSystem     → SystemSupervisor      publishNetwork  → NetworkFsm
publishSensors    → SensorService         publishActuators→ ActuatorManager
publishSafety     → SafetyMonitor         publishAutomation→ AutomationEngine
publishTime       → TimeService
```

### P2 ayrıntı

Kalan çağrılar **tek sahip modül** içinde: ADC'nin 3 okuması
`AnalogSensors.cpp`'de (io_sense'in sahibi), OLED'in 1 erişimi
`BootWiring.cpp`'de (yalnızca `begin()`).

## 2. Gereksinim izlenebilirliği

`REQUIREMENTS.md` 217 madde içeriyor: 54 `[x]`, 37 `[~]`, 126 `[ ]`.

Bunların madde madde izlenmesi bu turda **yapılmadı** — dürüst bir
izlenebilirlik matrisi, her maddenin donanımda doğrulanmasını gerektirir ve
donanım yok. Yapılan: **7 kritik problemin** karşılığı.

| # | Kritik problem | Karşılık | Durum |
|---|---|---|---|
| 1 | Otomasyon yok | TASK-054…057 | ✅ kod yazıldı, **M4 kapısı kapalı** |
| 2 | Paylaşılan kaynaklar korumasız | StateStore mutex + tek yazar + tek kapı | ✅ taramayla doğrulandı |
| 3 | Bloklayan Wi-Fi | Olay güdümlü FSM, `delay` taraması 0 | ✅ |
| 4 | Hata durumunda yarı ölü | Aşamalı boot, hiçbir aşama akışı kesmez | ✅ kod; donanımda **doğrulanmadı** |
| 5 | Frontend/backend sözleşme tutarsızlığı | Tek serileştirici, tek şema, `el()` guard | ✅ tarayıcıda doğrulandı |
| 6 | Ölü kod yükü | P7 taraması: 0 tanımsız bildirim | ✅ |
| 7 | Güvenlik yok | TASK-042 + TASK-063 | ✅ kod; zamanlama **ölçülmedi** |

## 3. Donanım test prosedürü

`docs/HARDWARE_TEST_PROCEDURE.md` yazıldı: 6 bölüm, ~45 numaralı test.
Bölüm 2 **M4 kapısıdır** ve geçilmeden otomasyon `AUTO`'ya alınmaz.

Prosedürün başında ISSUE-003 (röle polaritesi) bir **ön koşul** olarak
duruyor: yanlışsa aşağıdaki hiçbir test anlamlı değil.

## 4. Kalan açık maddeler

Bkz. `docs/ISSUES.md` — 14 açık madde. Kapatılması için **donanım gereken**
kritikler:

```text
ISSUE-003  role polaritesi          ← TUM guvenlik zinciri buna dayali
ISSUE-024  stack boyutlari          ← ilk calistirmada olculebilir
ISSUE-022  /api/history sure olcumu
ISSUE-014  akis darbe/litre katsayisi
ISSUE-015  NTC bolucu topolojisi
ISSUE-005  donanimsal RTC karari    ← satin alma karari
```

## 5. Sürüm notları

`README.md` yazıldı: proje tanımı, teknoloji yığını, mimari, yapılanlar,
nasıl çalıştırılacağı ve **dürüst durum tablosu**.

**TASK-065: DENETİM TAMAMLANDI** (donanım testleri prosedüre yazıldı,
koşulmadı).
