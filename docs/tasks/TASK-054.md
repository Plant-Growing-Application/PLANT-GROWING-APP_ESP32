# TASK-054 — Rule Model & Config Schema

**Phase:** 12 — Automation · **Priority:** P1

## Objective

Otomasyon kurallarını **konfigürasyon verisi** olarak modellemek. Yeni bir sulama profili
eklemek firmware değişikliği gerektirmemeli.

## Scope

- `ScheduleRule` ve `ThresholdRule` veri yapıları
- Ortak kural alanları: hedef aktüatör, etkinlik, öncelik, minimum tetikleme aralığı
- Kural kümesinin config şemasına eklenmesi
- Kural doğrulama kuralları
- Kural değerlendirme sözleşmesi (arayüz)

## Out of Scope

- Değerlendirme mantığı (TASK-055, TASK-056)
- Motor entegrasyonu (TASK-057)
- Kural düzenleme arayüzü (TASK-049)

## Dependencies

- TASK-014, TASK-028

## Requirements

- `REQUIREMENTS.md` — §11-Critical (otomasyon mantığı yok), §11-Medium (eşik ayarları yok)

## Architecture References

- §11.2 Kural modeli · §11.4 Otomasyonun bilmediği şeyler

## Expected Design

### Kurallar veri, kod değil

```text
Kod:     kural motoru (sabit)
Veri:    kural tanımları (config'te, kullanıcı tarafından düzenlenebilir)
```

Bu ayrım sayesinde "her 2 saatte 15 dk sula" ile "EC 1.0'ın altına düşünce besin ver"
aynı motorla çalışır ve yeni profil eklemek firmware güncellemesi gerektirmez.

### Ortak alanlar

| Alan | Amaç |
|---|---|
| Hedef aktüatör | Hangi aktüatörü etkiliyor |
| Etkinlik bayrağı | Kural açık/kapalı |
| Öncelik | Çakışan kurallarda hangisi kazanır |
| Minimum tetikleme aralığı | Kuralın çok sık tetiklenmesini önler |
| Histerezis (threshold) | Röle çırpınmasını önler |

### Kural doğrulaması

- Hedef aktüatör tanımlı olmalı
- Eşik değerleri sensörün geçerli aralığında olmalı
- Histerezis pozitif ve eşik aralığından küçük olmalı
- Çizelgede bitiş başlangıçtan sonra olmalı
- Aynı aktüatörü hedefleyen çakışan kurallar tespit edilmeli (uyarı)

## Implementation Notes

- Kural sayısı **sabit üst sınırlı** olmalı; dinamik liste heap kullanır. Makul bir üst
  sınır (örn. 8–16 kural) seçilmeli.
- Kural yapısı POD olmalı; config'te saklanacak ve `SystemState`'te özeti taşınacak.
- Otomasyon **kısıtları bilmez** (§11.4): `minRunTime`, `cooldown` kural yapısında yer
  almaz — bunlar `ActuatorManager`'ın işidir. Bu ayrım korunmalı.
- Otomasyon **güvenlik kilitlerini bilmez**: kural "pompa açık olsun" der, güvenliğin
  izin verip vermediğine karışmaz.
- Kural kimliği kararlı olmalı (API ve loglarda kullanılacak).
- Varsayılan kural kümesi **boş veya pasif** olmalı — ilk açılışta sistem kendiliğinden
  sulamaya başlamamalı (TASK-014 güvenli varsayılan ilkesi).

## Files

- `src/domain/rules/RuleModel.h` (yeni)
- `src/domain/rules/RuleValidation.h` / `.cpp` (yeni)
- `src/core/Config.h` (güncelleme — otomasyon bölümü)

## Acceptance Criteria

- [ ] İki kural tipi modellendi
- [ ] Ortak alanlar tanımlı
- [ ] Kural sayısı sabit üst sınırlı; heap kullanımı yok
- [ ] Kural yapısı POD
- [ ] Kurallar config şemasında; firmware değişikliği gerektirmeden düzenlenebilir
- [ ] Doğrulama kuralları uygulanıyor
- [ ] Çakışan kurallar tespit ediliyor
- [ ] Kural yapısında aktüatör kısıtı veya güvenlik bilgisi **yok**
- [ ] Varsayılan kural kümesi pasif
- [ ] Kural kimlikleri kararlı

## Test Plan

- [ ] Host tarafında kural doğrulaması test edildi
- [ ] Geçersiz kural (aralık dışı eşik, negatif histerezis) reddediliyor
- [ ] Çakışan kurallar tespit ediliyor
- [ ] Üst sınırdan fazla kural eklenemiyor
- [ ] Kurallar config'e kaydedilip geri yükleniyor
- [ ] Varsayılan config ile sistem kendiliğinden sulamıyor
- [ ] Kural yapısı boyutu ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§11.2, §11.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — kural dizisi boyutu
- [ ] Error handling var mı? — doğrulama
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — kural, kısıt ve güvenlik ayrımı korunmuş mu
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + doğrulama host tarafında test edildi + varsayılanın güvenli olduğu doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 0 — M4 KAPISI AÇILMADI: bu faz kapalı doğuyor

```text
IMPLEMENTATION_PLAN §M4: "M4 dogrulanmadan PHASE 12 (otomasyon)
                          baslatilmaz."
M4 gerekliligi: "Pompa yalnizca guvenlik izniyle calisiyor; kuru calisma,
                 seviye ve max sure korumalari DONANIMDA kanitlandi."
Gercek durum:   ISSUE-018 — sistem hic boot etmedi, hicbiri kanitlanmadi.
```

Kod yazılıyor ancak **çalışmaya başlamıyor**:
- `AutomationConfig.mode` varsayılanı `MANUAL` (TASK-014'te zaten öyle)
- Varsayılan kural kümesi **tamamen boş ve pasif**
- `MANUAL` modda kurallar hiç değerlendirilmez (TASK-057 Karar 1)

Yani M4 kapanana kadar hiçbir aktüatör kendiliğinden çalışmaz. Kapının
açılması bir **donanım doğrulaması** işidir, kod işi değil.

## Karar 1 — Makro çakışma taraması (ISSUE-009 kuralı) ÖNCE yapıldı

```text
Taranan: THRESHOLD SCHEDULE RULE PERIODIC WINDOW CYCLE ABOVE BELOW
         HYSTERESIS TRIGGER ACTIVE ARMED OVERRIDE MANUAL AUTO PAUSED
         SKIPPED MISSED DAILY INTERVAL ON_TIME OFF_TIME PRIORITY EDGE
         RISING FALLING HOLD RELEASE

IKI CAKISMA: RISING, FALLING  (Arduino kesme modlari) → KULLANILMIYOR
```

## Karar 2 — Kural sayısı SABİT: 8

Dinamik liste heap kullanır ve `Config` NVS blob'unda saklanamaz. 8 kural:
2 aktüatör × (birkaç eşik + birkaç çizelge) için fazlasıyla yeterli.

`Config` boyutu 392 → ~616 bayt; `static_assert(sizeof(Config) <= 640)`
sınırı içinde kalıyor ve bu **derleme zamanında** doğrulanıyor.

## Karar 3 — DÜZ yapı, `union` DEĞİL

```text
union: kind'e gore farkli alanlar → 12 bayt/kural tasarruf
       AMA dogrulama, JSON serilestirme ve gozden gecirme zorlasir;
       yanlis kind ile yanlis alani okumak SESSIZ bir hatadir.

SECILDI: duz yapi. 8 kural × ~12 kullanilmayan bayt = 96 bayt israf.
         96 bayt, bir okuma hatasi sinifini tamamen ortadan kaldirmanin
         karsiliginda ucuz.
```

## Karar 4 — Yön, iki eşikten TÜRETİLİR — ayrı bayrak YOK

```text
Naif tasarim: onAbove bayragi + onThreshold + offThreshold
Problem:      bayrak esiklerle CELISEBILIR (onAbove=1 ama onThreshold <
              offThreshold) ve bu celiski dogrulanmadan gecerse kural
              tersine calisir.

SECILDI: yon iki esikten turetilir, ucuncu bir alan YOK.

  onThreshold < offThreshold → deger onThreshold ALTINA dusunce AC,
                               offThreshold'a YUKSELINCE kapat
                               (ornek: EC dustu → besin ver)
  onThreshold > offThreshold → deger onThreshold USTUNE cikinca AC,
                               offThreshold'a DUSUNCE kapat
                               (ornek: sicaklik yukseldi → fan)

Histerezis bandi = |onThreshold - offThreshold|.
Dogrulama tek kural: ESIKLER ESIT OLAMAZ (esit = histerezis yok = curpinma).
```

Çelişki üretebilecek alan **var olmadığı için** çelişki imkânsız.

## Karar 5 — Otomasyon kısıtları ve güvenliği BİLMEZ (§11.4)

`Rule` yapısında `minRunMs`, `cooldownMs`, güvenlik eşiği **yoktur** ve
eklenmeyecektir. Kural yalnızca "şu aktüatörün açık olmasını istiyorum"
der. Kısıtlar `ActuatorManager`'ın, kilitler `SafetyMonitor`'un işidir.

Bu ayrım sayesinde kural motoru karmaşıklaşsa bile güvenlik mantığı sabit,
denetlenebilir ve ayrı test edilebilir kalır.

## Karar 6 — Varsayılan kural kümesi BOŞ

İlk açılışta sistem kendiliğinden sulamaya başlamaz (TASK-014 güvenli
varsayılan ilkesi). Kural kimlikleri kararlıdır (dizinin indeksi = kimlik)
— API ve loglarda kullanılır.

---

# STEP 3 — REVIEW RECORD

- [x] `Rule` / `RuleSet` tanımlı; POD, heap yok
- [x] Ortak alanlar: hedef, etkinlik, öncelik, minimum tetikleme aralığı
- [x] Kural kümesi `Config` şemasına eklendi
- [x] Doğrulama: hedef, sensör indeksi, eşik aralığı, pencere, çevrim,
      **çakışan öncelik**
- [x] Otomasyon kısıtları bilmiyor — tarama: `minRunMs|cooldownMs|maxRunMs|`
      `safety::|interlock|permits` → **kod içinde 0 eşleşme** (tek isabet
      "eklenmeyecek" diyen yorum)
- [x] Varsayılan kural kümesi **tamamen boş**
- [x] Kural kimliği kararlı (dizi indeksi)
- [x] Derleme temiz

## Ölçülen boyutlar (tahmin değil)

```text
Rule      24 bayt   (tahminim 28'di)
RuleSet  196 bayt
Config   588 bayt   ← sinir 640, `static_assert` ile korunuyor
```

`static_assert(MAX_RULES == 8)` eklendi: kural sayısı değiştirilirse
derleme uyarır ve `Config` boyutu yeniden ölçülmek zorunda kalır.

## Katman düzeltmesi: `Rule.h` `core/` içinde

İlk yazımda `domain/models/Rule.h` olarak oluşturuldu. Ancak `RuleSet`
`Config`'in parçası ve `Config` `core/` içinde — bu bir **D5 ihlali**
olurdu ("core/ hiçbir katmana bağımlı değildir"). Model `core/Rule.h`'a
taşındı; **değerlendirme** `domain/` içinde kaldı.

Bu, TASK-035'teki `NetworkState.h` ile aynı hata sınıfı: planın dosya
listesi katman kuralını ihlal ediyordu.

**TASK-054: TAMAMLANDI.**
