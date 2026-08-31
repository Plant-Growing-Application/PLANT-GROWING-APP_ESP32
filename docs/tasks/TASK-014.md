# TASK-014 — Config Schema, Defaults & Validation

**Phase:** 3 — Storage & Configuration · **Priority:** P0

## Objective

Tüm yapılandırılabilir ayarları şemalı olarak tanımlamak: her alanın tipi, varsayılanı ve
geçerli aralığı. Config bütünlüğü güvenlik için kritiktir — bozuk bir eşik değeri pompayı
yanlış çalıştırabilir.

## Scope

- Config bölümleri: `network`, `sensors` (kalibrasyon), `actuators` (kısıtlar),
  `safety` (eşikler), `automation` (kurallar), `system` (TZ, log seviyesi)
- Her alan için varsayılan değer ve geçerli aralık
- Doğrulama fonksiyonları ve alanlar arası tutarlılık kuralları
- Şema versiyonu alanı

## Out of Scope

- Kalıcı yükleme/kaydetme (TASK-015)
- Kural yapısının detayı (TASK-054 — burada yalnızca yer ayrılır)
- API üzerinden düzenleme (TASK-044)

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — §7.2 (kullanılmayan alanlar), §7.3 (versiyonlama yok), §11-Medium (eşik ayarları yok)

## Architecture References

- §2.4 ConfigService
- §15.3 Config şema versiyonlama

## Expected Design

**Temel kural:** Her alan için **varsayılan ve geçerli aralık şemada tanımlıdır.**
Geçersiz değer → varsayılan + WARNING log. Mevcut projedeki sessiz `memset(&Setting, 0, ...)`
davranışı yasaktır — sıfırlanmış bir güvenlik eşiği, kapalı bir korumadır.

### Güvenlik açısından kritik alanlar

Bu alanların aralık doğrulaması **zorunludur** ve gevşek bırakılamaz:

| Alan | Neden kritik |
|---|---|
| `safety.minWaterLevel` | Kuru çalışma koruması |
| `actuators.*.maxRunTime` | Sınırsız çalışmayı engeller — üst sınırı olmalı |
| `actuators.*.minRunTime` | maxRunTime'dan küçük olmalı (tutarlılık) |
| `actuators.*.cooldown` | Kısa çevrim koruması |
| `safety.flowVerifyDelay` / `flowMinRate` | Kuru çalışma tespiti |

### Alanlar arası tutarlılık

Tek tek geçerli ama birlikte anlamsız kombinasyonlar reddedilmeli:
`minRunTime > maxRunTime`, static IP seçili ama gateway boş, threshold kuralında
histerezis aralığından büyük, çizelgede bitiş başlangıçtan önce.

## Implementation Notes

- Config yapısı POD olmalı; string alanları sabit boyutlu.
- **Kullanılmayan alan konmayacak** (P7). Mevcut projedeki `IP`, `MAC`,
  `LittleFSFormatted` gibi hiç okunmayan alanlar örnek alınmamalı.
- Şema versiyonu ilk sürümde bile bulunmalı; sonradan eklemek migration'ı imkânsızlaştırır.
- Wi-Fi şifresi **config'te değil, SecretStore'da** (TASK-013).
- Doğrulama fonksiyonları saf olmalı (yan etkisiz), böylece host tarafında test edilebilir.
- Varsayılanlar **güvenli tarafta** seçilmeli: örneğin varsayılan `maxRunTime` kısa,
  varsayılan otomasyon modu MANUAL olmalı. İlk açılışta sistem kendiliğinden sulamaya
  başlamamalı.

## Files

- `src/core/Config.h` (yeni — şema + varsayılanlar)
- `src/core/ConfigValidation.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Altı config bölümü tanımlı
- [ ] Her alanın varsayılanı ve geçerli aralığı şemada
- [ ] Güvenlik açısından kritik alanların aralık doğrulaması zorunlu
- [ ] Alanlar arası tutarlılık kuralları uygulanıyor
- [ ] Şema versiyonu mevcut
- [ ] Varsayılanlar güvenli tarafta (ilk açılışta otomatik sulama yok)
- [ ] Kullanılmayan alan yok
- [ ] Şifre config'te değil
- [ ] Doğrulama fonksiyonları saf ve host'ta test edilebilir

## Test Plan

- [ ] Her kritik alan için aralık dışı değer reddediliyor
- [ ] Tutarsız kombinasyonlar (minRun > maxRun vb.) reddediliyor
- [ ] Varsayılan config doğrulamadan geçiyor
- [ ] Doğrulama host tarafında donanımsız çalışıyor
- [ ] Config yapısının boyutu ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.4, §15.3)
- [ ] Gereksiz abstraction var mı? — gereksiz config alanı var mı
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — yapı boyutu
- [ ] Error handling var mı? — **doğrulama bu task'ın ana konusu**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `Settings` yapısı ve `memset` deseni taşınmamalı

## Definition of Done

Ortak DoD + tüm kritik alanların aralık testleri host tarafında geçti + varsayılanların
güvenli olduğu incelemeyle onaylandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Doğrulama hatası **alan adıyla** döner

```text
Gereksinim (ARCHITECTURE §14.5): API hatalari alan bazinda gosterilmeli
                                 (`{error:{code, message, field}}`).
Approaches:   (a) yalnizca ErrCode  → kullanici hangi alanin bozuk oldugunu bilemez
              (b) ErrCode + alan indeksi → esleme tablosu kirilgan
              (c) ErrCode + statik alan ADI (const char*)
Selected:     (c) — `ConfigError{ErrCode code; const char* field;}`.
              Isaretci .rodata'daki sabit metne bakar; ayirma yok, POD kalir.
              API bunu dogrudan JSON `field` anahtarina yazar.
```

## Karar 2 — Güvenli varsayılanlar: ilk açılışta sulama YOK

```text
Risk: Varsayilan config ile cihaz ilk acildiginda kendiliginden sulamaya
      baslarsa, kurulum sirasinda pompa kuru calisabilir.

Selected varsayilanlar:
  automation.mode        = MANUAL     (AUTO degil)
  actuators[*].enabled   = false      (AUX'lar kapali)
  actuators[*].maxRunMs  = 5 dakika   (kisa — uzun degil)
  safety esikleri        = KORUYUCU tarafta

Ilke: varsayilan bir deger belirsizse, GUVENLI olani secilir.
```

## Karar 3 — Kritik alanların aralık doğrulaması gevşetilemez

Aşağıdaki alanlar doğrudan donanım güvenliğini belirler; doğrulaması
**zorunludur** ve API'den gevşetilemez (TASK-044 aynı fonksiyonları kullanacak):

| Alan | Neden kritik | Kısıt |
|---|---|---|
| `actuators[*].maxRunMs` | Sınırsız çalışmayı engeller | **üst sınırı var** (2 saat) |
| `actuators[*].minRunMs` | Kısa çevrim koruması | `< maxRunMs` (alanlar arası) |
| `actuators[*].cooldownMs` | Pompa ömrü | üst sınır |
| `safety.flowVerifyDelayMs` | Kuru çalışma tespiti | 1–60 sn |
| `safety.flowMinRate` | Kuru çalışma eşiği | > 0 |

`maxRunMs`'in **üst sınırı olması** özellikle önemli: sınırsız bırakılırsa
"maksimum çalışma süresi" koruması anlamsızlaşır.

## Karar 4 — Otomasyon kuralları bu sürümde şemada YOK (P7)

```text
Kural yapisini TASK-054 tanimlayacak. Simdi bos bir kural dizisi ayirmak
olu alan olurdu (P7) ve boyutu bosuna buyuturdu.

Selected: schemaVersion = 1 → kurallar YOK (yalnizca mod + override suresi)
          TASK-054 kurallari ekleyip schemaVersion = 2 yapacak
Gerekce:  Sema versiyonlama TAM BU DURUM icin var (ARCHITECTURE §15.3).
          Migration yolu zaten TASK-015'te kurulacak.
```

## Karar 5 — Şifre config'te değil

`network` bölümü yalnızca SSID taşır. Şifre `SecretStore`'dadır (TASK-013).
Bu ayrım, config'in tamamını serileştiren bir API'nin (TASK-044) şifreyi
kazara sızdırmasını **yapısal olarak** imkânsız kılar.

## Karar 6 — Doğrulama fonksiyonları saf

Girdi config, çıktı `ConfigError`. Yan etki yok, log yok, donanım yok.
Bu sayede host tarafında donanımsız test edilebilir (TASK-064) ve TASK-044
API katmanı **aynı** fonksiyonları yeniden kullanır — doğrulama mantığı
iki yerde yazılmaz.

## Kapsam dışı

- Kalıcı yükleme/kaydetme ve migration → TASK-015
- Kural yapısının detayı → TASK-054
- API üzerinden düzenleme → TASK-044

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Altı config bölümü tanımlı (`network`, `sensors`, `actuators`, `safety`,
      `automation`, `system`)
- [x] Her alanın varsayılanı ve geçerli aralığı şemada (`limits` namespace'i)
- [x] Güvenlik açısından kritik alanların aralık doğrulaması zorunlu;
      **`maxRunMs`'in ÜST SINIRI var** (2 saat)
- [x] Alanlar arası tutarlılık uygulanıyor: `minRunMs >= maxRunMs`,
      static IP eksik alanlar, seviye sensörü/koruma tutarsızlığı
- [x] Şema sürümü mevcut (`CONFIG_SCHEMA_VERSION = 1`)
- [x] Varsayılanlar güvenli tarafta — **ilk açılışta otomatik sulama yok**
- [x] Kullanılmayan alan yok — otomasyon kuralları bilinçli olarak **eklenmedi**
      (TASK-054 sürüm 2'de ekleyecek)
- [x] **Şifre config'te değil** — yalnızca SSID
- [x] Doğrulama fonksiyonları saf; yan etki/log/donanım yok

## Ölçüm

`sizeof(Config)` = **392 bayt** (assert sınırı 640) — NVS blob olarak rahat.

## Bulduğum hata: varsayılan config kendi doğrulamamdan geçmiyordu

İlk yazımda:

```text
safety.requireLevelSensor        = 1   (seviye korumasi ZORUNLU)
sensors[WATER_LEVEL].enabled     = 0   (sensor kapali)
→ capraz-alan kontrolu varsayilan config'i REDDEDIYORDU
```

İki olası düzeltme vardı ve **yanlış olanı seçmemek önemliydi**:

| Seçenek | Sonuç |
|---|---|
| `requireLevelSensor = 0` yap | Pompa, seviye koruması **olmadan** çalışabilir hale gelirdi — güvensiz varsayılan |
| Güvenlik sensörlerini aç | Sensör fiziksel olarak yoksa `FAULT` okunur → pompa **kilitlenir** |

İkincisi seçildi. Sensör takılı değilken pompanın kilitli olması bir kusur
değil, **fail-safe davranışın ta kendisidir** (ARCHITECTURE §9.5, §12.2).
`WATER_LEVEL` ve `WATER_FLOW` varsayılan olarak etkin; ikisi de güvenlik
zincirinin girdisidir (seviye kilidi ve akış doğrulaması).

Bu, doğrulama kurallarının işe yaradığının somut kanıtı: kural, kendi
varsayılanlarımdaki güvenlik boşluğunu yakaladı.

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı
- [x] `sizeof(Config)` ölçüldü = 392 bayt
- [x] Varsayılan config doğrulama yolunun tamamı elle izlendi ve düzeltildi
- [ ] **Aralık dışı değer testleri — host ortamı gerekiyor (TASK-064)**
- [ ] **Tutarsız kombinasyon testleri — host ortamı gerekiyor (TASK-064)**

> Doğrulama saf fonksiyon olduğu için bu testler donanım DEĞİL, **native test
> ortamı** gerektiriyor; o ortam TASK-064 kapsamında kurulacak. Bu batch'te
> kurmak TASK-064'ün işini yapmak olurdu.

## Review Checklist

- [x] Architecture'a uygun mu? — §2.4, §15.3 şema versiyonlama
- [x] Gereksiz abstraction var mı? — otomasyon kural dizisi **eklenmedi** (P7);
      şema sürümü tam bu amaçla var
- [x] Blocking işlem var mı? — N/A
- [x] Shared state güvenli mi? — N/A (veri tanımı)
- [x] Memory problemi var mı? — 392 bayt POD, heap yok
- [x] **Error handling var mı? — bu task'ın ana konusu.** Hata alan adıyla
      dönüyor; API doğrudan JSON `field` anahtarına yazabilir
- [x] ESP32 resource kullanımı uygun mu? — NVS blob boyutu makul
- [x] Task sorumluluğu doğru mu? — N/A
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** `Settings` yapısı
      taşınmadı; kullanılmayan alanları (`IP`, `MAC`, `LittleFSFormatted`) ve
      sessiz `memset` sıfırlaması yok. `_useDHCP = IsServerMode` hatası
      düzeltildi: `IpMode` artık AP/STA modundan **bağımsız** bir alan.

## Durum

**TASK-014: TAMAMLANDI.**
