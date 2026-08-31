# TASK-016 — FileStore (LittleFS)

**Phase:** 3 — Storage & Configuration · **Priority:** P1

## Objective

LittleFS erişimini tek bir sürücü arkasında toplamak ve mount hatasını **sistemi durduran
bir istisna değil, okunabilir bir durum** haline getirmek.

## Scope

- LittleFS mount ve durum raporlama
- Dosya okuma/yazma/silme, varlık kontrolü
- Serbest alan ve kullanım istatistikleri
- Gzip'li varlık çözümleme desteği (`x.css` isteği → `x.css.gz` varsa onu bildir)
- Tek mount noktası — çift mount edilmemesi

## Out of Scope

- HTTP servisi (TASK-041)
- Halka dosya geçmiş verisi (TASK-058)
- Varlıkların üretilmesi/gzip'lenmesi (TASK-047)

## Dependencies

- TASK-004, TASK-005

## Requirements

- `REQUIREMENTS.md` — §1 (LittleFS iki kez mount ediliyor), §9 Storage failure `[~]`

## Architecture References

- §2.15 FileStore · §15.1 Veri sınıflandırması
- §16.3 Storage failure davranışı

## Expected Design

- Mount **tam olarak bir kez**, boot aşamasında yapılmalı. Mevcut projede hem `setup()`
  hem `WebServerManager::begin()` mount ediyor — bu tekrar kaldırılmalı.
- Mount başarısızlığı bir **durum** olmalı: `isMounted()` sorgulanabilir, üst katman buna
  göre davranır (web statik dosya servis edemez ama sistem çalışır).
- Otomatik biçimlendirme (`format on fail`) **dikkatli** ele alınmalı: tüm web varlıklarını
  ve geçmiş veriyi siler. Otomatik yapılacaksa mutlaka loglanmalı ve raporlanmalı.
- Gzip çözümleme sürücü seviyesinde olmalı ki web katmanı bu ayrıntıyı bilmesin.

## Implementation Notes

- Dosya işlemleri **yavaştır ve değişken sürelidir**; AsyncTCP callback'inden doğrudan
  büyük dosya taraması yapılmamalı (§14.6).
- Aynı dosyaya farklı task'lardan eşzamanlı erişim ele alınmalı; LittleFS iş parçacığı
  güvenliği garantili değildir.
- Serbest alan izlenmeli: dolu dosya sistemi geçmiş veri yazımını sessizce başarısız kılar.
- Yazma hatası her zaman raporlanmalı.
- Dosya tanıtıcılarının (handle) sızdırılmaması için erişim deseni disiplinli olmalı.

## Files

- `src/hal/FileStore.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Mount tam olarak bir kez yapılıyor
- [ ] Mount durumu sorgulanabilir; hata sistemi durdurmuyor
- [ ] Okuma/yazma/silme/varlık kontrolü çalışıyor
- [ ] Gzip'li varlık çözümleme sürücü seviyesinde
- [ ] Serbest alan istatistiği alınabiliyor
- [ ] Yazma hataları raporlanıyor
- [ ] Otomatik biçimlendirme kararı verilmiş ve loglanıyor
- [ ] Eşzamanlı erişim güvenli

## Test Plan

- [ ] Mount edilmemiş durumda sistem boot ediyor ve çalışıyor (DEGRADED)
- [ ] Dosya yaz → yeniden başlat → oku döngüsü içeriği koruyor
- [ ] Dosya sistemi doldurulduğunda yazma hatası doğru raporlanıyor
- [ ] Gzip'li varlık doğru çözümleniyor
- [ ] İki task'tan eşzamanlı erişimde bozulma yok
- [ ] Uzun süreli kullanımda dosya tanıtıcı sızıntısı yok

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.15, §15.1)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — AsyncTCP bağlamında uzun işlem yapılmıyor mu
- [ ] Shared state güvenli mi? — eşzamanlı dosya erişimi
- [ ] Memory problemi var mı? — dosya tanıtıcı sızıntısı
- [ ] Error handling var mı? — mount/yazma/dolu disk
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **çift mount taşınmamalı**

## Definition of Done

Ortak DoD + mount hatası senaryosunda sistemin çalışmaya devam ettiği doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Mount başarısızsa otomatik biçimlendirme: koşullu

```text
Problem:      LittleFS.begin(true) mount basarisizsa BICIMLENDIRIR.
              Bu web varliklarini ve gecmis veriyi siler.
Ikilem:       Web varliklari silinirse kullanici onlari geri yukleyemez —
              yukleme arayuzu de silinmis olur (tavuk-yumurta).
Approaches:   (a) her zaman bicimlendir → kurtarilamaz kayip riski
              (b) hic bicimlendirme → bozuk FS ile sistem hep DEGRADED
              (c) once bicimlendirmeden dene; basarisizsa bicimlendir + CRITICAL
Selected:     (c) — gecici bir mount hatasi veriyi kurtarabilir; kalici
              bozulmada ise bicimlendirmek tek secenektir.
Kural:        Bicimlendirme SESSIZ DEGIL. CRITICAL loglanir, `wasFormatted()`
              ile sorgulanir, boot raporuna ve arayuze yansir (§16.4).
```

## Karar 2 — Mount hatası bir **durum**, istisna değil

Mevcut sistemde LittleFS mount hatası `setup()`'tan erken `return` ile
sonuçlanıyor ve **hiçbir task oluşmuyordu** (Kritik Problem 4).

Burada mount hatası `isMounted()` ile sorgulanan bir durumdur. Web katmanı
statik dosya servis edemez ama **otomasyon, güvenlik ve sensörler tam çalışır**
(ARCHITECTURE §16.3).

## Karar 3 — Eşzamanlılık koruması sürücüde

```text
LittleFS'in is parcacigi guvenligi GARANTILI DEGIL.
Erisecekler: `store` task'i (gecmis veri) + AsyncTCP callback'i (web varliklari)
             → gercek eszamanli erisim var.
Selected:    Surucu icinde mutex. Sinirli bekleme (sonsuz yok).
Not:         Bu bir IS KURALI degil, kaynak korumasidir — D6 ihlali sayilmaz.
```

## Karar 4 — Gzip çözümleme sürücüde

`/style.css` istendiğinde `/style.css.gz` varsa onun kullanılacağı bilgisi
sürücüde üretilir. Web katmanı bu ayrıntıyı bilmez; yalnızca dönen
`gzipped` bayrağına göre `Content-Encoding` başlığı ekler.

Gerekçe: mevcut sistemdeki 298 KB sıkıştırılmamış Bootstrap dosyası hem flash
hem bant genişliği israfıydı (REQUIREMENTS Donanım tablosu).

## Karar 5 — Dosya tanıtıcısı sızıntısına karşı kapsam disiplini

Her açma işlemi aynı fonksiyon içinde kapatılır; `File` nesnesi dışarı
verilmez. Uzun ömürlü tanıtıcı gerekiyorsa (TASK-058 halka dosyası) o modül
kendi yaşam döngüsünü yönetir.

## Kapsam dışı

- HTTP servisi → TASK-041 · Halka dosya → TASK-058
- Varlıkların üretilmesi/gzip'lenmesi → TASK-047

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Mount **tam olarak bir kez** yapılıyor (tekrar çağrı zararsız);
      mevcut sistemdeki çift mount deseni taşınmadı
- [x] Mount durumu `isMounted()` ile sorgulanabiliyor; **hata sistemi durdurmuyor**
- [x] Okuma/yazma/ekleme/silme/varlık kontrolü çalışıyor
- [x] Gzip'li varlık çözümleme **sürücü seviyesinde** (`resolveAsset`)
- [x] Serbest alan istatistiği alınabiliyor
- [x] Yazma hataları raporlanıyor; kısmi yazma `STORAGE_FULL` olarak loglanıyor
- [x] Otomatik biçimlendirme kararı verildi ve **CRITICAL loglanıyor**
- [x] Eşzamanlı erişim mutex ile korunuyor (sınırlı bekleme)

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı
- [x] Dosya tanıtıcıları aynı kapsamda kapatılıyor (kod incelemesi) — sızıntı yok
- [ ] **Mount edilmemişken sistemin çalıştığı — donanım gerekiyor**
- [ ] **Yaz → reset → oku — donanım gerekiyor**
- [ ] **Dolu dosya sistemi — donanım gerekiyor**
- [ ] **Gzip çözümleme — TASK-047 varlıkları üretince**
- [ ] **Eşzamanlı erişim — TASK-059/TASK-041 sonrası**

## Review Checklist

- [x] Architecture'a uygun mu? — §2.15, §15.1, §16.3 degraded davranışı
- [x] Gereksiz abstraction var mı? — ince sarmalayıcı; `File` nesnesi dışarı
      verilmiyor (tanıtıcı sızıntısı yapısal olarak engelli)
- [x] Blocking işlem var mı? — dosya işlemleri doğası gereği senkron;
      mutex beklemesi 2 sn ile **sınırlı**, sonsuz bekleme yok
- [x] Shared state güvenli mi? — mutex; LittleFS'in kendi güvenliği garantili değil
- [x] Memory problemi var mı? — sabit tamponlar; `File` kapsam dışına çıkmıyor
- [x] Error handling var mı? — mount, kısmi yazma, dolu disk, bulunamadı
- [x] ESP32 resource kullanımı uygun mu? — statik mutex
- [x] Task sorumluluğu doğru mu? — sürücü task bilmiyor
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Çift mount ve
      mount hatasında erken `return` deseni taşınmadı.

## Durum

**TASK-016: TAMAMLANDI** (donanım testleri bekliyor).
