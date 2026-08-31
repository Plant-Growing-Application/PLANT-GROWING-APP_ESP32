# TASK-028 — Actuator Model & Constraints

**Phase:** 6 — Safety & Actuator System · **Priority:** **P0**

## Objective

Mantıksal aktüatör kavramını fiziksel röleden ayırmak ve çalışma kısıtlarını (min/max süre,
cooldown) veri modeli düzeyinde tanımlamak.

## Scope

- Aktüatör kimlikleri: `WATER_PUMP`, `AIR_PUMP`, `AUX`
- Mantıksal kimlik → fiziksel röle eşlemesi (yapılandırılabilir)
- Kısıt modeli: `minRunTime`, `maxRunTime`, `cooldown`
- Çalışma sayaçları: toplam süre, çevrim sayısı, son değişim zamanı
- Komut kaynağı enum'u: `MANUAL`, `AUTO`, `SAFETY`
- Komut sonucu enum'u (§10.4)

## Out of Scope

- Kısıtların uygulanması (TASK-029)
- Güvenlik kilitleri (TASK-030)
- Röle sürücüsü (TASK-017)

## Dependencies

- TASK-004, TASK-014

## Requirements

- `REQUIREMENTS.md` — §4 (aktüatörler), §11-Critical (pompa güvenlik koşulları yok)

## Architecture References

- §10.1 Katmanlama (mantıksal/fiziksel ayrım)
- §10.2 Aktüatör kataloğu · §10.4 Komut sonucu sözleşmesi

## Expected Design

### Mantıksal / fiziksel ayrım — neden önemli

`REQUIREMENTS.md` §12'de RELAY2'nin gerçekte neye bağlı olduğu açık bir sorudur (ISSUE-004).
Bu ayrım sayesinde donanım netleştiğinde **yalnızca yapılandırma değişir, kod değişmez**.
Ayrıca bir mantıksal aktüatörün birden fazla röle sürmesi veya röle polaritesinin değişmesi
domain katmanını etkilemez.

### Kısıt modeli — anlamları

| Kısıt | Amaç | İhlal sonucu |
|---|---|---|
| `minRunTime` | Açıldıktan sonra en az bu kadar çalışsın (kısa çevrim koruması) | Kapatma ertelenir → `DEFERRED_MIN_RUNTIME` |
| `maxRunTime` | Bu süreden uzun çalışmasın (kuru çalışma / taşma koruması) | Zorla kapatılır + WARNING |
| `cooldown` | Kapandıktan sonra bu kadar beklesin (pompa ömrü) | Açma ertelenir → `DEFERRED_COOLDOWN` |

Bu kısıtlar **config'ten gelir** ve TASK-014'te aralık doğrulaması yapılır
(`minRunTime < maxRunTime` zorunlu).

## Implementation Notes

- Süre ölçümleri **monotonik zaman** ile yapılmalı; NTP senkronizasyonu duvar saatini
  geriye alabilir ve "pompa 3 saattir çalışıyor" hesabını bozar.
- `millis()` taşması süre hesaplarında doğru ele alınmalı (TASK-004 yardımcısı).
- Toplam çalışma süresi ve çevrim sayısı bakım göstergesi olarak değerlidir; kalıcı
  saklama kararı verilmeli (flash aşınması için seyrek yazma).
- Model POD olmalı; `SystemState` içinde taşınacak.
- `AUX` aktüatörü şu an kullanılmıyorsa **eklenmemeli** (P7). Yalnızca donanımda karşılığı
  varsa tanımlanmalı.
- ISSUE-004 bu task'ta kapatılmalı: RELAY2'nin gerçek yükü belirlenip isimlendirme
  düzeltilmeli. Mevcut arayüzdeki "Oksijen Sensörü" etiketi bir aktüatör için yanlıştır.

## Files

- `src/domain/models/Actuator.h` (yeni)
- `src/domain/models/ActuatorCommand.h` (yeni)
- `src/core/Config.h` (güncelleme — aktüatör kısıt bölümü)

## Acceptance Criteria

- [ ] Mantıksal kimlik ile fiziksel röle eşlemesi yapılandırılabilir
- [ ] Üç kısıt modellendi ve config'ten okunuyor
- [ ] Çalışma sayaçları tanımlı
- [ ] Komut kaynağı ve sonuç enum'ları §10.4 ile uyumlu
- [ ] Süre hesapları monotonik zaman kullanıyor
- [ ] `millis()` taşması güvenli
- [ ] Model POD
- [ ] Kullanılmayan aktüatör tanımlanmamış
- [ ] ISSUE-004 kapandı; isimlendirme düzeltildi

## Test Plan

- [ ] Host tarafında kısıt hesapları test edildi (min/max/cooldown sınırlarında)
- [ ] Taşma sınırında süre hesabı doğru
- [ ] Config'ten farklı kısıt değerleri yükleniyor
- [ ] Mantıksal → fiziksel eşleme değiştirildiğinde kod değişikliği gerekmiyor
- [ ] Model boyutu ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§10.1, §10.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — sonuç enum'u eksiksiz mi
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — doğrudan `digitalWrite` deseni taşınmamalı

## Definition of Done

Ortak DoD + kısıt hesapları host tarafında test edildi + ISSUE-004 kapatıldı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — ISSUE-010 uygulanıyor: hangi tip nerede

```text
ZATEN TANIMLI (yeniden tanimlanmayacak):
  SystemState.h  → ActuatorId, ControlSource, ActuatorStatus (yayinlanan state)
  Command.h      → CommandResult                             (TASK-008)
  Config.h       → ActuatorConfig (minRun/maxRun/cooldown)   (TASK-014)

BU TASK'IN TANIMLADIKLARI (calisma tipleri — yayinlanmaz):
  ActuatorRuntime   → ic zamanlayicilar ve sayaclar
  ConstraintVerdict → kisit degerlendirmesinin sonucu
  saf kisit fonksiyonlari
```

## Karar 2 — Kısıt fonksiyonları SAF

```text
Girdi:  ActuatorConfig + ActuatorRuntime + now
Cikti:  ConstraintVerdict
Yan etki YOK, donanim YOK, log YOK.

Kazanc: TASK-064 host tarafinda min/max/cooldown sinirlarini SINIR
        DEGERLERINDE test edebilir — donanim gerektirmeden.
        Bir pompa kisitini sahada denemek pahali ve yavas.
```

## Karar 3 — `maxRunMs` sahipliği: ActuatorManager, SafetyMonitor değil

```text
Cakisma: TASK-029 scope'u maxRunTime'i "aktuator kisiti" diyor.
         TASK-030 scope'u MAX_RUNTIME_EXCEEDED'i "guvenlik kilidi" diyor.

Ayrim (bu task'ta sabitleniyor):
  ActuatorManager → maxRunMs'i UYGULAR. Zamanlayiciyi o tutar (roleyi ne
                    zaman actigini TAM olarak bilir). Asimda zorla kapatir
                    ve IHLAL SAYACINI artirir.
  SafetyMonitor   → ihlal SAYISINI izler. Esik asilinca ACIL DURUMA gecirir.

Gerekce: Zamanlayiciyi snapshot uzerinden takip etmek bir dongu gecikme
         ekler; roleyi acan modul zamani zaten biliyor. Tekrarlayan ihlal
         ise sistemik bir arizadir ve guvenligin isidir.
```

## Karar 4 — `ActuatorRuntime` yayınlanan state'ten AYRI

```text
`ActuatorStatus` (yayinlanan) → UI ve API'nin gordugu
`ActuatorRuntime` (ic)        → talep edilen durum, zamanlayicilar,
                                ihlal sayaci, son komut kaynagi

Neden ayri: ic zamanlayicilarin arayuze sizmasi gereksiz; state 312 baytta
kalmali. Yayinlanan ozet ic durumdan TURETILIR.
```

## Karar 5 — Süre ölçümleri monotonik ve taşma güvenli

`CODING_STANDARDS` Z4/Z5. NTP duvar saatini geriye alabilir; "pompa 3
saattir çalışıyor" hesabı duvar saatiyle yapılırsa `maxRunMs` koruması
sessizce bozulur. Tüm zamanlayıcılar `Millis` + `hasElapsed()` kullanır —
**ISSUE-012'nin iki kez düştüğüm tuzağı** burada tekrarlanmayacak.

## Karar 6 — ISSUE-004 (RELAY2 gerçek yükü) kapanmadı

```text
Mantiksal kimlik (`AIR_PUMP`) ile fiziksel role (`RELAY_AIR_PUMP`) ayri
oldugu icin donanim netlestiginde YALNIZCA ESLEME degisir, kod degismez.
Ancak yukun gercekte ne oldugu hala DOGRULANMADI (REQUIREMENTS §12) —
issue acik kaliyor. Yazilim bu belirsizlikten etkilenmiyor.
```

---

# STEP 3 — REVIEW RECORD

> **Protokol notu:** Bu inceleme kaydı geriye dönük yazıldı; TASK-028'in
> tasarım kaydı yazılmış ama incelemesi atlanmıştı (TASK-065 denetiminde
> fark edildi). Aşağıdaki maddeler mevcut koda karşı **yeniden** denetlendi.

- [x] Mantıksal kimlik (`ActuatorId`) ile fiziksel röle (`relayIndex`)
      ayrımı yapılandırılabilir
- [x] Üç kısıt modellendi ve config'ten okunuyor
- [x] Çalışma sayaçları tanımlı (`totalRunMs`, `cycleCount`, `maxRunViolations`)
- [x] Komut kaynağı ve sonuç enum'ları §10.4 ile uyumlu — **yeniden
      tanımlanmadı**, `core/`'dan alındı (ISSUE-010)
- [x] Süre hesapları monotonik (`Millis` + `hasElapsed`)
- [x] `millis()` taşması güvenli
- [x] Model POD (`static_assert` ile doğrulanıyor)
- [x] Kullanılmayan aktüatör tanımlanmadı — `AUX_1`/`AUX_2` `ActuatorId`'de
      var ama hiçbir kural/ekran onları kullanmıyor ve config'te `enabled=0`
- [x] Tahkim sırası `static_assert`larla derleme zamanında kilitli
- [x] Kısıt fonksiyonları **saf** → host testlerinde kullanıldı (TASK-064)
- [ ] **ISSUE-004 KAPANMADI** — RELAY2'nin gerçek yükü hâlâ doğrulanmadı.
      Kabul kriteri "ISSUE-004 kapandı" diyordu; **kapatılamadı** çünkü
      donanım bilgisi gerekiyor. Mantıksal/fiziksel ayrım sayesinde yazılım
      bu belirsizlikten etkilenmiyor.

## TASK-057 sonrası eklenen: `releaseSource()`

Tasarım kaydında (Karar 3) tahkim tablosu sabitlenmişti ama **override
süresinin dolması durumu düşünülmemişti**. TASK-057'de kalıcı kilitlenme
ortaya çıktı ve `ActuatorManager`'a `releaseSource()` eklendi.

Bu, TASK-029 inceleme kaydında öngörülen ("MANUAL override'ın süre sınırı
TASK-057'nin işidir") ama bu task'ın modelinde karşılığı olmayan bir
boşluktu.

**TASK-028: TAMAMLANDI** (ISSUE-004 açık kalıyor).
