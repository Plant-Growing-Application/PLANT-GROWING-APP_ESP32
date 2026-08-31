# TASK-055 — Threshold Rule Evaluation (Hysteresis)

**Phase:** 12 — Automation · **Priority:** P1

## Objective

Sensör değerine bağlı kuralları değerlendirmek ve **histerezis** ile röle çırpınmasını
önlemek.

## Scope

- Eşik karşılaştırması ve histerezis bandı
- Sensör kalitesi kontrolü (yalnızca `OK` değerle karar)
- Minimum tetikleme aralığı uygulaması
- Kural durumunun taşınması (aktif/pasif, son tetikleme)

## Out of Scope

- Çizelge kuralları (TASK-056)
- Motor entegrasyonu (TASK-057)
- Aktüatör kısıtları (TASK-029)

## Dependencies

- TASK-054, TASK-027

## Requirements

- `REQUIREMENTS.md` — §11-Critical (otomasyon), §3 (sensör kalitesi)

## Architecture References

- §11.2 Kural modeli (histerezis zorunlu) · §11.3 sensor based control satırı

## Expected Design

### Histerezis — neden zorunlu

```text
Histerezissiz:  EC eşiği 1.0. Ölçüm 0.99 → pompa AÇ. Ölçüm 1.01 → KAPAT.
                Gürültü nedeniyle saniyede birkaç kez açılıp kapanabilir.
                Röle ve pompa hızla yıpranır.

Histerezisli:   AÇ eşiği 0.9, KAPAT eşiği 1.1.
                Bir kez açıldıktan sonra 1.1'e ulaşana kadar açık kalır.
```

Histerezis bandı kural yapısında tanımlı ve **pozitif olması zorunludur** (TASK-054
doğrulaması).

### Sensör kalitesi kontrolü — zorunlu

```text
Kalite OK değilse → kural DEĞERLENDİRİLMEZ
```

`FAULT` veya `OUT_OF_RANGE` bir sensörle karar vermek, bozuk bir termometreye bakıp
ısıtıcıyı açmaya benzer. Kural değerlendirilmediğinde aktüatör **mevcut durumunu korur**
mu, yoksa **güvenli duruma mı geçer**? Bu bir tasarım kararıdır.

### Karar gerektiren nokta — Kalite bozulduğunda davranış

```text
Problem:      Kural çalışırken sensör arızalandı. Aktüatör ne yapmalı?
Constraints:  Açık kalması taşma/aşırı besleme riski;
              kapanması bitki susuz kalması riski;
              güvenlik kilitleri zaten ayrı çalışıyor (TASK-030)
Approaches:   (a) mevcut durumu koru
              (b) güvenli duruma geç (kapat)
              (c) kısa süre koru, uzun sürerse kapat + uyarı
Recommended:  (c) — anlık sensör hatası sistemi durdurmasın,
              kalıcı hata güvenli tarafa düşürsün
```

## Implementation Notes

- Değerlendirme **saf fonksiyon** olmalı: girdi (snapshot, kural, kural durumu, zaman),
  çıktı (istenen durum). Host tarafında test edilebilmeli.
- Kural durumu (histerezis nedeniyle "şu an aktif mi") saklanmalı; yalnızca anlık değere
  bakmak histerezisi imkânsız kılar.
- Minimum tetikleme aralığı, kuralın çok sık açıp kapamasını sınırlar; histerezisten
  ayrı bir koruma katmanıdır.
- Birden fazla threshold kuralı aynı aktüatörü hedefliyorsa öncelik alanına göre
  çözülmeli; belirsizlik bırakılmamalı.
- Kural tetiklenmesi loglanmalı ancak her döngüde değil — yalnızca durum değişiminde.
- Otomasyon **güvenlik kilidini bilmez**: kural "açık olsun" der, `ActuatorManager`
  güvenliğe danışır. Bu ayrım korunmalı.

## Files

- `src/domain/rules/ThresholdRuleEvaluator.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Eşik + histerezis doğru çalışıyor
- [ ] Sensör kalitesi `OK` değilse kural değerlendirilmiyor
- [ ] Kalite bozulduğundaki davranış kararı verildi ve uygulandı
- [ ] Kural durumu saklanıyor
- [ ] Minimum tetikleme aralığı uygulanıyor
- [ ] Çoklu kural çakışması önceliğe göre çözülüyor
- [ ] Durum değişimi loglanıyor; log seli yok
- [ ] Değerlendirme saf fonksiyon; host'ta test edilebilir
- [ ] Güvenlik kilidi bilgisi kural motorunda yok

## Test Plan

- [ ] Host tarafında histerezis davranışı sentetik veriyle test edildi
- [ ] Gürültülü sensör verisinde röle çırpınması olmuyor
- [ ] Sensör `FAULT` olduğunda kural değerlendirilmiyor
- [ ] Kalıcı sensör hatasında seçilen davranış uygulanıyor
- [ ] Minimum tetikleme aralığı sınırlıyor
- [ ] Çakışan kurallarda öncelik doğru uygulanıyor
- [ ] Donanımda gerçek sensörle bir kural uçtan uca doğrulandı
- [ ] Güvenlik kilidi aktifken kural "açık" dese bile aktüatör açılmıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§11.2, §11.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — kural durumu tek task'ta
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **sensör kalitesi kontrolü**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — otomasyon güvenliğe karışmıyor mu
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + histerezis host testleriyle kanıtlandı + güvenlik vetosunun kuralı geçtiği
donanımda doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Kalite bozulduğunda: (c) kısa süre koru, uzun sürerse kapat

```text
Problem: Kural calisirken sensor arizalandi. Aktuator ne yapmali?

(a) mevcut durumu koru → arizali sensorle pompa SONSUZA KADAR acik kalabilir
(b) hemen kapat        → anlik bir gurultu darbesi sulamayi keser
(c) SECILDI: SUSPECT_GRACE_MS (30 sn) boyunca mevcut durumu koru;
             asilirsa kurali KAPAT tarafina dusur ve UYARI uret.
```

**Gerekçe:** anlık sensör hatası (tek bir bozuk okuma, geçici gürültü)
sistemi durdurmamalı. Kalıcı hata ise güvenli tarafa düşmeli.

**Bu bir GÜVENLİK katmanı DEĞİLDİR.** `SafetyMonitor` (TASK-030) zaten
ayrı çalışıyor ve seviye/akış sensörü arızasında pompayı kilitliyor.
Buradaki davranış otomasyonun **kendi** girdisine güvenmemesiyle ilgili;
güvenlik zinciri buna bağımlı değil.

## Karar 2 — Yalnızca `OK` kalite ile karar

```text
quality != OK → kural DEGERLENDIRILMEZ (suspect sayaci isler)
```

`FAULT` veya `OUT_OF_RANGE` bir sensörle karar vermek, bozuk bir
termometreye bakıp ısıtıcıyı açmaya benzer. `STALE` de kabul edilmiyor:
bayat bir değer geçmişi anlatır, şimdiyi değil.

## Karar 3 — Histerezis durumu SAKLANIR

`RuleRuntime.active` olmadan histerezis **imkânsızdır**: yalnızca anlık
değere bakan bir kural, iki eşik arasındaki bölgede ne yapacağını bilemez.

```text
onThreshold=0.9, offThreshold=1.1, olcum=1.0 →
    active ise  → ACIK KAL  (1.1'e ulasmadi)
    pasif ise   → KAPALI KAL (0.9'un altina inmedi)
```

## Karar 4 — Minimum tetikleme aralığı, histerezisten AYRI katman

Histerezis **değer gürültüsüne** karşı korur. Minimum aralık **hızlı
salınıma** karşı korur (örneğin gerçekten eşik etrafında gidip gelen bir
sistem). İkisi farklı arıza modlarına bakar; biri diğerinin yerini tutmaz.

Aralık yalnızca **durum değişimini** geciktirir, mevcut durumu bozmaz.

## Karar 5 — Değerlendirme SAF

`evaluateThreshold(rule, runtime, sample, now) → RuleVerdict`

Yan etki yok, donanım yok, global yok. `runtime` referansla güncellenir ve
bu tek yan etkidir — çağıranın sahip olduğu bellek üzerinde. TASK-064
histerezis sınırlarını ve suspect zamanlayıcısını donanımsız koşturabilir.

---

# STEP 3 — REVIEW RECORD

- [x] Eşik karşılaştırması ve histerezis bandı
- [x] Yalnızca `OK` kalite ile karar; `STALE` dahil hiçbiri kabul edilmiyor
- [x] Kalite bozulduğunda (c): 30 sn koru → aşılırsa kapat + uyarı
- [x] Minimum tetikleme aralığı uygulanıyor; **mevcut durumu bozmuyor**,
      yalnızca değişimi geciktiriyor
- [x] Kural durumu (`RuleRuntime.active`) saklanıyor — histerezis onsuz
      imkânsız
- [x] Değerlendirme **saf**: donanım yok, global yok
- [x] Çakışma önceliğe göre çözülüyor (motor tarafında)
- [ ] **Gerçek sensörle davranış testi — donanım gerekiyor**

## Yön çelişkisi yapısal olarak imkânsız

`onAbove` benzeri bir bayrak **yok**. Yön iki eşikten türetiliyor:

```text
onThreshold < offThreshold → dusunce AC   (EC dustu → besin)
onThreshold > offThreshold → cikinca AC   (sicaklik yukseldi → fan)
```

Ayrı bir bayrak eşiklerle çelişebilirdi ve çelişki doğrulanmadan geçerse
kural **tersine** çalışırdı. Alan var olmadığı için o hata sınıfı yok.

**TASK-055: TAMAMLANDI.**
