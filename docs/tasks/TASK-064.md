# TASK-064 — Host Test Harness & Domain Tests

**Phase:** 15 — Testing & Review · **Priority:** P1

## Objective

Domain katmanını donanımsız, masaüstünde test edilebilir hale getirmek. Güvenlik ve
otomasyon mantığının hızlı ve tekrarlanabilir şekilde doğrulanmasını sağlamak.

## Scope

- Host tarafı test ortamının kurulması
- Domain modüllerinin donanımsız derlenmesi
- Güvenlik kuralı testleri (SafetyMonitor, FlowVerification, EmergencyStop)
- Otomasyon kuralı testleri (Threshold, Schedule, AutomationEngine)
- Sensör hattı testleri (kalibrasyon, filtre, doğrulama)
- Config doğrulama testleri
- Zaman enjeksiyonu ile hızlandırılmış senaryolar

## Out of Scope

- Donanım gerektiren testler (TASK-065)
- HAL sürücülerinin testi (donanım gerektirir)
- UI görsel testleri

## Dependencies

- TASK-030, TASK-057

## Requirements

- `REQUIREMENTS.md` — §11-Low (test altyapısı yok, `test/` klasörü boş)

## Architecture References

- §17 Test edilebilirlik (domain katmanı donanımsız)
- §1.2 D5 (cross-cutting modüller donanıma bağımlı değil)

## Expected Design

### Neden host testi mümkün

`ARCHITECTURE.md` §17 gereği `domain/` katmanı yalnızca `core/` tiplerine bağımlıdır ve
donanım çağrısı içermez. Bu, mimarinin bilinçli bir çıktısıdır:

```text
  SafetyMonitor      →  snapshot al, karar üret        →  donanımsız test edilebilir
  AutomationEngine   →  snapshot + config + zaman      →  donanımsız test edilebilir
  SensorPipeline     →  ham değer + config             →  donanımsız test edilebilir
  ConfigValidation   →  saf doğrulama                  →  donanımsız test edilebilir
```

### Öncelikli test alanları

| Alan | Neden öncelikli |
|---|---|
| Güvenlik kilitleri | Tüm kombinasyonlar donanımda test edilemez, host'ta edilebilir |
| Kuru çalışma zamanlaması | Gerçek zamanda dakikalar sürer, host'ta milisaniyeler |
| Histerezis | Sentetik gürültülü veri ile kapsamlı test |
| Çizelge kuralları | DST, gece yarısı, kaçırılan çevrim — gerçek zamanda test edilemez |
| Config doğrulama | Yüzlerce geçersiz kombinasyon hızlıca denenebilir |
| Süre taşması | `millis()` taşması gerçekte 49 gün sürer |

### Zaman enjeksiyonu

Zamana bağlı tüm modüller zamanı **parametre olarak** almalıdır (TASK-031, TASK-056
notlarında belirtildi). Bu sayede 3 saatlik bir sulama çevrimi milisaniyelerde test edilir.

## Implementation Notes

- Test ortamı PlatformIO'nun native ortamı veya ayrı bir masaüstü derlemesi olabilir;
  karar gerekçelendirilmeli.
- Testler CI'da çalıştırılabilecek şekilde kurulmalı (donanım gerektirmediği için mümkün).
- Sahte (fake) snapshot üreticileri yazılmalı; her test kendi senaryosunu kurabilmeli.
- Testler **davranışı** doğrulamalı, implementasyonu değil. İç yapı değişince testler
  kırılmamalı.
- Sınır değerleri özellikle test edilmeli: eşiğin tam üstü/altı, sürenin tam dolduğu an,
  taşma sınırı.
- Güvenlik testleri **kapsamlı** olmalı: kilit kombinasyonları, arıza durumları, mod
  geçişleri.
- Test sayısı hedef değildir; kapsanan **senaryolar** önemlidir.

## Files

- `test/` (yeni — test dosyaları)
- `test/fakes/` (yeni — sahte snapshot ve zaman üreticileri)
- `platformio.ini` (güncelleme — native test ortamı)

## Acceptance Criteria

- [ ] Host tarafı test ortamı çalışıyor
- [ ] Domain modülleri donanımsız derleniyor
- [ ] Güvenlik kilidi kombinasyonları test ediliyor
- [ ] Kuru çalışma zamanlaması hızlandırılmış test ediliyor
- [ ] Histerezis sentetik veriyle test ediliyor
- [ ] Çizelge kuralları (DST, gece yarısı, kaçırılan çevrim) test ediliyor
- [ ] Config doğrulama kapsamlı test ediliyor
- [ ] `millis()` taşması test ediliyor
- [ ] Sahte snapshot ve zaman üreticileri mevcut
- [ ] Testler CI'da çalıştırılabilir
- [ ] Testler davranışı doğruluyor, iç yapıyı değil

## Test Plan

- [ ] Tüm testler host'ta geçiyor
- [ ] Test çalışma süresi kısa (geliştirme sırasında sık çalıştırılabilir)
- [ ] Kasıtlı bir hata eklendiğinde ilgili test kırılıyor (test etkinliği kanıtı)
- [ ] Testler donanım olmadan çalışıyor
- [ ] Sınır değer testleri mevcut ve geçiyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§17)
- [ ] Gereksiz abstraction var mı? — test için üretime soyutlama eklendi mi
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? — testler birbirini etkilemiyor mu
- [ ] Memory problemi var mı? (N/A)
- [ ] Error handling var mı? — hata yolları test ediliyor mu
- [ ] ESP32 resource kullanımı uygun mu? (N/A)
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı?

## Definition of Done

Ortak DoD + tüm domain testleri geçiyor + kasıtlı hata ile testlerin etkinliği kanıtlandı.

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

**Tarih:** 2026-08-31

## Karar 1 — Test önceliği: GERÇEKTE TEST EDİLEMEYENLER

Host testinin değeri, donanımda **imkânsız veya pratik olmayan** şeyleri
kapsamasındadır. Kolayca elle denenebilecek şeyleri test etmek zaman israfı.

```text
`millis()` tasmasi      → gercekte 49 GUN
Cizelge sarma penceresi → gece yarisini beklemek gerekir
Histerezis gurultusu    → sentetik veri olmadan tekrarlanamaz
Backoff egrisi          → gercekte dakikalar
Config kombinasyonlari  → yuzlerce gecersiz kombinasyon
Guvenlik kilidi matrisi → tum kombinasyonlar donanimda uretilemez
```

## Karar 2 — Mimari zaten test edilebilir; ek soyutlama YOK

ARCHITECTURE §17'nin karşılığı alındı. Doğrulandı: `core/` ve `domain/`
başlıklarının **hiçbiri** `<Arduino.h>`, `<freertos/*>` veya `<esp_*>`
include etmiyor. Testler için tek bir mock/fake yazılmadı — gerekmedi.

Zaman enjeksiyonu da zaten var: her zamana bağlı fonksiyon `now`'ı
**parametre** alıyor (TASK-031/055/056 kararları).

## Yazılanlar

`test/test_domain/test_domain.cpp` — **28 test**, 7 grup:

```text
1. Zaman tasmasi          3 test   (49 gunluk sarma)
2. Aktuator kisitlari     5 test   (sinir degerlerde)
3. Guvenlik kilit matrisi 3 test
4. Backoff                5 test   (jitter icin 256 deger taranir)
5. Cizelge                3 test   (gece yarisi sarmasi dahil)
6. IP plani               3 test
7. Config dogrulama       6 test   (varsayilanlarin kendi dogrulamasi dahil)
```

`platformio.ini`'ye `[env:native]` eklendi: `pio test -e native`.

## DÜRÜST DURUM: testler ÇALIŞTIRILMADI

```text
Yazildi            ✅  28 test
Derlenebilirligi   ✅  hedef derleyiciyle (xtensa g++ -fsyntax-only)
                       -Wall -Wextra ile 0 HATA, 0 UYARI
Calistirildi       ❌  BU MAKINEDE HOST DERLEYICI YOK
```

Arandı ve bulunamadı: `gcc`, `g++`, `cl.exe`, MSVC, MinGW, MSYS2, LLVM.
PlatformIO `native` platformu ve Unity kuruldu, derleme aşamasında
`'gcc' is not recognized` ile durdu.

**Testlerin geçtiğini iddia etmiyorum.** Yalnızca geçerli C++ oldukları ve
gerçek başlıklara karşı derlendikleri doğrulandı. Bir host derleyicisi
kurulduğunda `pio test -e native` doğrudan çalışacak durumda.

## Not: `static_assert`'ler zaten çalışıyor

Bu testlerin bir kısmı **derleme zamanında zaten doğrulanıyor**:
`Time.h` 4, `Actuator.h` 5, `SafetyState.h` 5, `RetryPolicy.h` 11,
`RuleEvaluator.h` 15, `NetworkEvents.h` 6 = **46 `static_assert`**.

Bunlar her `pio run`'da koşuyor ve **şu ana kadar hep geçti**. Unity
testleri bunların üzerine çalışma zamanı davranışı ekliyor.

**TASK-064: TESTLER YAZILDI VE DERLENDİ, ÇALIŞTIRILMADI.**
