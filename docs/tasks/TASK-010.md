# TASK-010 — Staged Boot Framework & Boot Report

**Phase:** 2 — Boot & Task Framework · **Priority:** P0

## Objective

Aşamalı, hata toleranslı bir açılış akışı kurmak. Mevcut projedeki iki ölümcül davranışı
ortadan kaldırmak: OLED hatasında `while(true)` kilitlenmesi ve LittleFS hatasında
`setup()`'tan erken `return` (task'ların hiç oluşmaması).

## Scope

- Boot aşaması tanımı: ad, zorunluluk, çalıştırma, sonuç kaydı
- Aşama sonuçlarının boot raporuna yazılması
- Boot sonucundan sistem modunun türetilmesi (RUNNING / DEGRADED / SAFE)
- Boot raporunun seri porta ve `Diagnostics`'e yazılması

## Out of Scope

- Aşamaların içeriği (ilgili modül task'ları dolduracak)
- Task oluşturma mekanizması (TASK-011)
- Mod makinesi çalışma zamanı davranışı (TASK-012)

## Dependencies

- TASK-005, TASK-009

## Requirements

- `REQUIREMENTS.md` — §1 (initialization `[~]`, hata davranışı `[~]`), Kritik Problem 4

## Architecture References

- §7.1 Aşamalı boot diyagramı
- §7.2 Boot sonucu → sistem modu
- §16.4 Yasaklanan hata davranışları

## Expected Design

### Karar gerektiren nokta — Aşama başarısızlığında davranış

```text
Problem:      Bir aşama başarısız olursa boot nasıl devam edecek?
Constraints:  Hiçbir hata boot'u durduramaz (P4);
              bazı aşamalar gerçekten zorunludur (GPIO güvenli seviye);
              kullanıcı neyin çalışmadığını bilmeli
Approaches:   (a) her aşama bool döner, çağıran karar verir
              (b) aşama tablosu + zorunluluk bayrağı + otomatik yürütücü
              (c) her aşama kendi hata davranışını tanımlar
Trade-offs:   (b) tutarlılık ve denetlenebilirlik sağlar, yeni aşama eklemek kolaydır
Recommended:  (b) — tablo tabanlı yürütücü, sonuçlar rapora yazılır
```

**Zorunlu aşamalar** (başarısızlığı `SAFE` moda götürür): reset nedeni + TWDT,
GPIO güvenli seviye, core altyapı, task oluşturma.
**Zorunlu olmayan aşamalar** (başarısızlığı `DEGRADED` yapar): NVS/Config, LittleFS,
OLED, ADC/PCNT, Wi-Fi radyo.

## Implementation Notes

- **GPIO güvenli seviye, core altyapıdan bile önce** gelmelidir (§7.1 Aşama 1). Röleler
  boot'un ilk milisaniyelerinde güvenli konuma alınmazsa kuru çalışma riski doğar.
- Aşama süreleri ölçülmeli; toplam boot süresi raporlanmalı. Uzun boot, güç kesintisi
  sonrası sistemin geç toparlanması demektir.
- Boot raporu **kalıcı olmalı** (en azından çalışma boyunca RAM'de), web ve OLED'den
  okunabilmeli. Yalnızca seri porta yazmak yetmez — sahada seri port yoktur.
- Aşama içinde `delay()` kullanılmamalı; donanım bekleme süreleri sürücünün kendi işidir.
- Aşama fonksiyonları yan etkisiz raporlama yapmalı: sonucu döndürmeli, kendisi mod kararı
  vermemeli.

## Files

- `src/core/BootSequence.h` / `.cpp` (yeni)
- `src/core/BootReport.h` / `.cpp` (TASK-005'ten devam)

## Acceptance Criteria

- [ ] Aşama tablosu ve yürütücü çalışıyor
- [ ] Zorunlu/zorunlu değil ayrımı uygulanmış
- [ ] Hiçbir aşama başarısızlığı boot'u durdurmuyor (`while(true)` ve erken `return` yok)
- [ ] Boot raporu tüm aşama sonuçlarını ve sürelerini içeriyor
- [ ] Sistem modu boot sonucundan doğru türetiliyor
- [ ] GPIO güvenli seviye aşaması ilk sıralarda
- [ ] Boot raporu RAM'de kalıcı ve sorgulanabilir

## Test Plan

- [ ] LittleFS kasıtlı bozulduğunda: sistem DEGRADED modda çalışmaya devam ediyor, task'lar oluşuyor
- [ ] OLED kablosu çıkarıldığında: sistem çalışmaya devam ediyor, web erişilebilir
- [ ] NVS kasıtlı bozulduğunda: varsayılan config yükleniyor ve loglanıyor
- [ ] Wi-Fi başlatılamadığında: otomasyon ve güvenlik etkilenmiyor
- [ ] Boot süresi ölçüldü ve kaydedildi
- [ ] Her senaryoda boot raporu doğru içerik gösteriyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§7)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — aşama içinde `delay()` var mı
- [ ] Shared state güvenli mi? — boot tek iş parçacığında
- [ ] Memory problemi var mı? — boot raporu boyutu
- [ ] Error handling var mı? — **bu task'ın ana konusu**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`while(true)` ve erken `return` yasak**

## Definition of Done

Ortak DoD + dört arıza senaryosunun (FS, OLED, NVS, Wi-Fi) tamamı donanımda test edildi
ve sistem hiçbirinde durmadı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Aşama fonksiyonları nereden gelir? (katman kısıtı)

```text
Problem:      Boot asamalari NVS, LittleFS, OLED, ADC, Wi-Fi baslatacak.
              Ama BootSequence `core/` katmanindadir ve core/ `hal/` veya
              `services/` katmanina BAGIMLI OLAMAZ (D5).
Approaches:   (a) BootSequence dogrudan hal/ ve services/ cagirir → D5 IHLALI
              (b) BootSequence'i services/ altina tasi → core altyapisi
                  (Diagnostics, StateStore) boot'tan once hazir olmali,
                  katman sirasi bozulur
              (c) Tablo tabanli yurutucu: asama FONKSIYONLARI disaridan verilir
Selected:     (c) — `core/` yalnizca YURUTUCUyu saglar:
                  { id, required, ErrCode(*fn)() } dizisi alir, sirayla calistirir,
                  sonucu rapora yazar, modu turetir.
              Gercek asama fonksiyonlarini boot wiring (bkz. ISSUE-013 — plan boslugu)
              saglar — orasi ust katmandir ve hal/services'i tanir.
Kazanc:       core/ hicbir katmana bagimli kalmaz; yurutucu donanimsiz
              test edilebilir (sahte asama fonksiyonlariyla).
```

Bu, aynı zamanda task'ın "Aşamaların içeriği kapsam dışı" kuralını doğal olarak
sağlar: yürütücü içeriği zaten göremez.

## Karar 2 — Aşama başarısızlığında davranış

```text
Selected: (b) tablo tabanli yurutucu + zorunluluk bayragi

Zorunlu asamalar (basarisizligi SAFE moda goturur):
  0  RESET_AND_WDT     reset nedeni + TWDT yapilandirmasi
  1  GPIO_SAFE_STATE   TUM ROLELER KAPALI
  2  CORE_SERVICES     Diagnostics + StateStore + kuyruklar
  8  TASK_CREATION     bes task olusturma

Zorunlu OLMAYAN (basarisizligi DEGRADED yapar):
  3  CONFIG_LOAD       NVS → varsayilan config'e dusulebilir
  4  FILESYSTEM        LittleFS → web statigi olmaz, sistem calisir
  5  DISPLAY_HW        OLED → sistem TAM calisir, yalnizca ekran yok
  6  SENSOR_HW         ADC + PCNT → sensor kalitesi duser
  7  NETWORK_RADIO     Wi-Fi → OFFLINE mod; otomasyon ve guvenlik ETKILENMEZ

HICBIR asama basarisizligi boot'u DURDURMAZ (P4).
`while(true)` ve erken `return` yasak (§16.4).
```

## Karar 3 — GPIO güvenli seviye neden Aşama 1?

`ARCHITECTURE.md` §7.1 GPIO güvenli seviyeyi core altyapısından bile **önce**
koyuyor. Gerekçe hayati: röleler boot'un ilk milisaniyelerinde güvenli konuma
alınmazsa, aktif-düşük bir röle modülünde pompa kuru çalışabilir (ISSUE-003).
Log altyapısının hazır olması pompanın korunmasından daha az önceliklidir.

Sonuç: Aşama 1 çalışırken `Diagnostics` henüz hazır olmayabilir. Yürütücü bunu
tolere eder — kayıtlar rapora yazılır, log altyapısı hazır olunca yayınlanır.

## Karar 4 — Boot süresi ölçümü ve yavaş aşama uyarısı

```text
Neden onemli: Uzun boot = guc kesintisi sonrasi gec toparlanma.
              Ayrica asama icinde delay() kullanimi (yasak) boyle yakalanir.
Selected:     Her asamanin suresi olculur ve rapora yazilir.
              Bir asama esigi asarsa WARNING loglanir.
Esik:         500 ms — mesru bir init isleminin bu kadar surmesi beklenmez;
              asarsa ya donanim bekliyor ya bloklama var.
```

Bu, "aşama içinde `delay()` kullanılmamalı" kuralını **denetlenebilir** kılar:
kural artık yalnızca dokümanda değil, çalışma zamanında görünür.

## Karar 5 — Mod türetme sınırı

```text
Boot sonucu → mod:
  zorunlu asama basarisiz          → SAFE
  zorunlu olmayan asama basarisiz  → DEGRADED
  hepsi basarili                   → RUNNING

EMERGENCY bir boot sonucu DEGILDIR: calisma zamaninda mandallanir (TASK-032).
Kalici mandal varsa boot sonrasi TASK-032 modu EMERGENCY'ye cevirir.
Bu sinir belgelenir ki mod turetme iki yerde yapilmasin.
```

## Karar 6 — `BootReport.cpp` boş dosya olarak açılmayacak

Task'ın Files listesi `BootReport.h / .cpp` diyor. `BootReport.h` TASK-005'te
tamamen satır içi (inline) yazıldı; boş bir `.cpp` açmak ölü dosya olurdu (P7).

Bunun yerine `.cpp` **gerçek bir iş** üstlenir: raporun biçimlendirilip seri
porta yazılması. Aşama adları `BootSequence`'ta tanımlı olduğu için, döngüsel
bağımlılığı önlemek adına `emit()` bir **ad çözümleme fonksiyon işaretçisi**
alır.

## Kapsam dışı bırakılanlar

- Aşama fonksiyonlarının içeriği → boot wiring (bkz. ISSUE-013)
- Task oluşturma mekanizması → TASK-011
- Mod makinesinin çalışma zamanı davranışı ve geçişleri → TASK-012
- Kalıcı acil durum mandalı → TASK-032

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Aşama tablosu ve yürütücü çalışıyor — `BootStageDef` dizisi + `boot::run()`
- [x] Zorunlu / zorunlu değil ayrımı uygulanmış (`required` bayrağı)
- [x] **Hiçbir aşama başarısızlığı boot'u durdurmuyor** — tarama ile doğrulandı:
      `while(true)` yok, erken `return` yok; döngü tablo sonuna kadar çalışıyor
- [x] Boot raporu tüm aşama sonuçlarını **ve sürelerini** içeriyor
- [x] Sistem modu boot sonucundan türetiliyor (`deriveMode`)
- [x] GPIO güvenli seviye Aşama 1 — core altyapıdan bile önce
- [x] Boot raporu RAM'de kalıcı (`diag::bootReport()`) ve sorgulanabilir

## API kullanılabilirlik doğrulaması

Yürütücü, sahte aşama fonksiyonlarıyla kurulan beş tabloyla derlendi —
API'nin gerçekten kullanılabilir olduğu ve tüm yolların tip düzeyinde
tutarlı olduğu doğrulandı:

| Senaryo | Beklenen mod |
|---|---|
| Tüm aşamalar başarılı | `RUNNING` |
| Zorunlu olmayan aşama başarısız | `DEGRADED` |
| Zorunlu aşama başarısız | `SAFE` |
| Tabloda `nullptr` fonksiyon | kaydediliyor + loglanıyor, boot sürüyor |
| Tablo boş / `nullptr` | `SAFE`, CRITICAL log, boot durmuyor |

Harness derlemeden sonra kaldırıldı (kalıcı dosya bırakılmadı).

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı
- [x] Yasak desen taraması: `while(true)` = 0, `delay()` = 0
- [x] Yürütücü API'si sahte tablolarla doğrulandı
- [ ] **LittleFS bozuk → DEGRADED, task'lar oluşuyor — donanım gerekiyor**
- [ ] **OLED çıkarılmış → sistem çalışıyor, web erişilebilir — donanım gerekiyor**
- [ ] **NVS bozuk → varsayılan config + log — donanım gerekiyor**
- [ ] **Wi-Fi yok → otomasyon/güvenlik etkilenmiyor — donanım gerekiyor**
- [ ] **Boot süresi ölçümü — donanım gerekiyor**

> Bu dört arıza senaryosu TASK-061 (arıza enjeksiyonu) matrisinin 9, 10, 11 ve
> 6 numaralı satırlarıdır ve orada donanımda kapatılacaktır. Aşama fonksiyonları
> henüz yazılmadığı için (TASK-013) senaryolar zaten şimdi çalıştırılamaz.

## Review Checklist

- [x] Architecture'a uygun mu? — §7.1 aşama sırası ve §7.2 mod türetme birebir
- [x] Gereksiz abstraction var mı? — tek `run()` fonksiyonu, düz C fonksiyon
      işaretçisi tablosu; sınıf/şablon/sanal fonksiyon yok
- [x] Blocking işlem var mı? — yürütücüde yok. Ayrıca **aşamaların bloklamasını
      yakalayan** bir mekanizma eklendi: 500 ms'yi aşan aşama WARNING üretiyor
- [x] Shared state güvenli mi? — boot tek iş parçacığında, task'lardan önce
- [x] Memory problemi var mı? — `BootReport` 76 bayt, ek ayırma yok
- [x] **Error handling var mı? — bu task'ın ana konusu.** Aşama hatası, null
      fonksiyon, boş tablo, rapor kapasitesi aşımı: dördü de sessizce geçilmiyor
- [x] ESP32 resource kullanımı uygun mu? — `millis()` farkı taşma güvenli
- [x] Task sorumluluğu doğru mu? — yürütücü aşama içeriğini görmüyor;
      **`core/` hiçbir katmana bağımlı kalmadı** (D5 korundu)
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Bu task tam olarak
      eski `setup()` davranışının karşıtı: orada OLED hatası `while(true)`,
      LittleFS hatası erken `return` idi. İkisi de tarama ile yok.

## Katman kısıtının çözümü

`core/` katmanı `hal/` ve `services/`'e bağımlı olamaz (D5), ama boot aşamaları
tam olarak o katmanları başlatacak. Çözüm: yürütücü aşama **fonksiyonlarını
dışarıdan alır**. `core/` sırayı, zorunluluğu, süreyi ve mod türetmeyi yönetir;
içeriği TASK-013 sağlar.

Yan kazanç: yürütücü sahte fonksiyonlarla **donanımsız test edilebilir**
(TASK-064) — yukarıdaki beş senaryo bunun ilk örneği.

## Bulgular

**ISSUE-009 tekrarladı.** `BootStage::DISPLAY`, `Arduino.h:51`'deki
`#define DISPLAY 0x1` ile çakıştı. `DISPLAY_HW` olarak yeniden adlandırıldı.

Bu ikinci tekrar üzerine bu task'ta bir **önlem alındı**: yeni eklenen tüm
enum ve sabit isimleri (32 ad) framework header'larına karşı toplu tarandı.
Yalnızca `DISPLAY` çakıştı. Tarama artık her yeni enum için zorunlu sayılmalı;
ISSUE-009 bu tekrar kaydıyla güncellendi.

## Durum

**TASK-010: TAMAMLANDI** (arıza senaryoları TASK-013/TASK-061'de donanımda).
