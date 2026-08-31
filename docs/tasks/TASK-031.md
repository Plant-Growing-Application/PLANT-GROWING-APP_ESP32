# TASK-031 — Flow Verification & Dry-Run Protection

**Phase:** 6 — Safety & Actuator System · **Priority:** **P0**

## Objective

Pompanın gerçekten su bastığını akış sensörüyle doğrulamak. Kuru çalışmayı tespit edip
pompayı korumak — hidroponik sistemde pompa kaybı bitki kaybı demektir.

## Scope

- Gecikmeli akış kontrolü: pompa açıldıktan T saniye sonra akış eşiğin altındaysa arıza
- Kuru çalışma arızası üretimi ve pompanın kapatılması
- Akış sensörü arızası ile gerçek akış yokluğunun ayrımı
- Doğrulama parametrelerinin config'ten okunması

## Out of Scope

- Akış sensörü okuma (TASK-025)
- Acil durum mandalı (TASK-032)
- Su seviyesi kilidi (TASK-030)

## Dependencies

- TASK-025, TASK-030

## Requirements

- `REQUIREMENTS.md` — §4.1 (kuru çalışma koruması yok), §11-Critical

## Architecture References

- §12.1 Güvenlik zinciri Katman 2 · §11.3 flow verification satırı

## Expected Design

### Temel mantık

```text
  Pompa OFF → ON geçişi
        │
        ▼
  Gecikme başlat (flowVerifyDelay)      ← boru dolma süresi
        │
        ▼
  Süre doldu → akış oku
        │
   ┌────┴────┐
   ▼         ▼
 akış ≥    akış <
 eşik      eşik
   │         │
   ▼         ▼
 NORMAL   KURU ÇALIŞMA ARIZASI
          → pompayı kapat
          → kilit aktifleştir
          → CRITICAL logla
```

### Karar gerektiren nokta 1 — Doğrulama gecikmesi

```text
Problem:      Pompa açıldığında akış anında başlamaz (boru dolması, sensör tepkisi)
Constraints:  Çok kısa gecikme → yanlış alarm, pompa gereksiz durur
              Çok uzun gecikme → kuru çalışma süresi uzar, pompa zarar görür
Approaches:   (a) sabit gecikme (config'ten)
              (b) kademeli: kısa süre sonra uyarı, uzun süre sonra arıza
              (c) sistem geometrisine göre kalibre edilen gecikme
Recommended:  (a) başlangıç için; değer **gerçek sistemde ölçülerek** belirlenmeli.
              Tahmin edilen bir değer kabul edilemez.
```

### Karar gerektiren nokta 2 — Sensör arızası ile akış yokluğu ayrımı

```text
Problem:      "0 L/dk" hem kuru çalışma hem kopuk sensör anlamına gelir
Constraints:  Sensör arızası yüzünden çalışan pompayı durdurmak üretim kaybıdır;
              kuru çalışmayı kaçırmak donanım kaybıdır
Approaches:   (a) ikisini de kuru çalışma say (en güvenli)
              (b) sensör kalitesine bak: FAULT ise ayrı arıza kodu üret
              (c) sensör arızasında pompayı çalıştırma (koruma devre dışıysa risk alma)
Trade-offs:   Donanım kaybı > üretim kaybı
Recommended:  (b) + (c) — ayrı arıza kodu üret ama her iki durumda da pompayı durdur.
              Koruma çalışmıyorsa korunan şey çalıştırılmaz.
```

## Implementation Notes

- Doğrulama yalnızca pompa **çalışırken** aktif olmalı; kapalıyken akış olmaması normaldir.
- Pompanın kapanıp hemen açılması durumunda gecikme sayacı **sıfırdan** başlamalı.
- Kuru çalışma arızası **mandallı olmalı** (TASK-032): koşul düzelse bile operatör onayı
  gerekir. Aralıklı kuru çalışma pompayı yavaşça öldürür ve sessizce tekrarlanmamalı.
- Akış eşiği config'te; sistemin normal debisinin belirgin altında ama sıfırın üstünde
  seçilmeli.
- Bu modül `SafetyMonitor` içinde mi ayrı mı olacağı geliştiricinin kararıdır; ancak
  vetoyu `SafetyMonitor` üzerinden uygulamalıdır (tek veto noktası kuralı).
- Test edilebilirlik: zaman kaynağı enjekte edilebilir olmalı ki host tarafında
  gecikme senaryoları hızlandırılmış test edilebilsin.

## Files

- `src/domain/FlowVerification.h` / `.cpp` (yeni)
- `src/domain/SafetyMonitor.cpp` (güncelleme — kilit entegrasyonu)

## Acceptance Criteria

- [ ] Gecikmeli akış kontrolü çalışıyor
- [ ] Doğrulama gecikmesi **gerçek sistemde ölçülerek** belirlendi
- [ ] Akış eşiğin altındaysa pompa kapatılıyor ve arıza üretiliyor
- [ ] Sensör arızası ayrı kodla raporlanıyor ama pompa yine durduruluyor
- [ ] Arıza mandallı; kendiliğinden temizlenmiyor
- [ ] Gecikme sayacı her açılışta sıfırlanıyor
- [ ] Parametreler config'ten
- [ ] Veto `SafetyMonitor` üzerinden uygulanıyor
- [ ] Zaman kaynağı test için enjekte edilebilir

## Test Plan

- [ ] **Gerçek kuru çalışma testi:** hazne boşken pompa açıldı, T saniye içinde durdu
- [ ] Normal çalışmada yanlış alarm üretilmiyor (uzun süreli test)
- [ ] Akış sensörü kablosu çıkarıldığında pompa durduruluyor, ayrı kod raporlanıyor
- [ ] Çalışırken su kesildiğinde arıza tespit ediliyor
- [ ] Arıza sonrası koşul düzelse bile pompa kendiliğinden çalışmıyor
- [ ] Hızlı kapat/aç döngüsünde gecikme doğru sıfırlanıyor
- [ ] Host tarafında hızlandırılmış zaman senaryoları geçti

## Review Checklist

- [ ] Architecture'a uygun mu? (§12.1 Katman 2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — sensör arızası ayrımı
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + **gerçek kuru çalışma testi donanımda yapıldı ve pompa korundu** + yanlış alarm
üretmediği uzun süreli testle doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Doğrulama gecikmesi: (a) sabit, config'ten

`flowVerifyDelayMs` config'ten okunur. **Değer donanımda ölçülmelidir**;
boru dolma süresi sistem geometrisine bağlıdır ve tahmin edilemez. Varsayılan
bir başlangıç değeridir, doğrulanmış bir değer değildir — kabul kriterinde
açıkça işaretsiz bırakıldı.

## Karar 2 — Sensör arızası ile akış yokluğu: (b) + (c) birlikte

```text
Problem: "0 L/dk" hem kuru calisma hem kopuk sensor demek.

Selected: AYRI KOD URET, AMA HER IKI DURUMDA DA POMPAYI DURDUR.
  kalite != OK  → SAFETY_FLOW_VERIFY_FAILED  (koruma calismiyor)
  kalite OK,
  debi < esik   → SAFETY_DRY_RUN             (gercek kuru calisma)

Gerekce: "Koruma calismiyorsa korunan sey calistirilmaz."
         Donanim kaybi (pompa) > uretim kaybi (bir sulama turu).
         Iki kodun ayri olmasi teshis icin sart: biri tesisat sorunu,
         digeri kablo sorunudur ve mudahaleleri farklidir.
```

## Karar 3 — Arıza MANDALLI

Koşul düzelse bile kendiliğinden temizlenmez; operatör onayı gerekir
(`acknowledge()`, TASK-032 kurtarma yolundan çağrılır).

**Gerekçe:** Aralıklı kuru çalışma pompayı *yavaşça* öldürür. Mandal
olmasaydı sistem "çalış → kuru → dur → soğu → çalış" döngüsüne girer, her
turda biraz daha hasar verir ve **hiç kimse fark etmez**. Mandal, sorunu
görünür kılmanın tek yoludur.

## Karar 4 — `safety::evaluate()` içeriden `flow::evaluate()` çağırır

```text
Alternatif: app_core once flow::evaluate(), sonra safety::evaluate() cagirsin.
Risk:       Cagrilardan biri unutulursa kuru calisma korumasi SESSIZCE
            devre disi kalir. Bu, tespit edilmesi en zor ariza turudur.

Selected:   Guvenlik zinciri TEK CAGRIYLA calisir: `safety::evaluate()`.
            Ic sirasi modul tarafindan garanti edilir.
Dongusel bagimlilik yok: flow, safety::evaluate()'i cagirmaz; yalnizca
`setExternalInterlock()` ile bir bit bildirir.
```

## Karar 5 — Gecikme sayacı pompanın `lastOnAt`'inden okunur

Ayrı bir sayaç tutulmaz. `ActuatorManager` rölenin ne zaman açıldığını
zaten **kesin** olarak biliyor. Hızlı kapat/aç döngüsünde `lastOnAt` her
açılışta güncellendiği için gecikme kendiliğinden sıfırlanır — ayrı bir
sayaç tutulsaydı senkronizasyon hatası ihtimali doğardı.

## Karar 6 — Zaman kaynağı enjekte edilebilir

`evaluate(snap, now)` — `now` parametredir, modül `millis()` çağırmaz.
TASK-064 gecikme senaryolarını hızlandırılmış koşturabilir.

---

# STEP 3 — REVIEW RECORD

- [x] Gecikmeli akış kontrolü çalışıyor (`PRIMING` → `VERIFIED` / `FAILED`)
- [x] Akış eşiğin altındaysa mandal aktifleşiyor; pompa `SafetyMonitor`
      vetosuyla **aynı döngüde** durduruluyor (`apply()` adım 3)
- [x] Sensör arızası **ayrı kodla** raporlanıyor (`SAFETY_FLOW_VERIFY_FAILED`)
      ama pompa yine durduruluyor
- [x] Arıza mandallı; `evaluate()` mandal aktifken erken dönüyor —
      koşul düzelse bile kendiliğinden temizlenmiyor
- [x] Gecikme sayacı her açılışta sıfırlanıyor — ayrı sayaç yok, pompanın
      `lastOnAt` değeri kullanılıyor
- [x] Parametreler config'ten (`flowVerifyDelayMs`, `flowMinRate`)
- [x] Veto `SafetyMonitor` üzerinden — bu modül röleye **hiç dokunmuyor**
- [x] Zaman kaynağı enjekte edilebilir (`now` parametre; `millis()` çağrısı yok)
- [x] Derleme temiz
- [ ] **Doğrulama gecikmesi gerçek sistemde ÖLÇÜLMEDİ** — varsayılan bir
      başlangıç değeridir, doğrulanmış değer değildir
- [ ] **Gerçek kuru çalışma testi — donanım gerekiyor**

## Bulduğum hata: yanlış arıza kodu raporlanıyordu

`SafetyState.h` içindeki `reasonOf(ILK_DRY_RUN)` **sabit** olarak
`SAFETY_DRY_RUN` döndürüyor. Ancak bu kilit **iki ayrı nedenle** set
edilebiliyor: gerçek kuru çalışma veya akış sensörü arızası.

Somut sonucu: **kopuk bir akış sensörü kablosuna "kuru çalışma" denirdi.**
Operatör hazneye, borulara, pompaya bakar — sorun ise kablodadır. Tam da
TASK-031 Karar 2'de "iki kodun ayrı olması teşhis için şart" diye yazdığım
şeyin ihlali.

İki yerde düzeltildi:
- `reportedReason()` gerçek nedeni `flow::reason()`'dan alıyor
- `logEdges()` `ILK_DRY_RUN` bitini atlıyor — `flow` modülü zaten kendi
  kesin kodunu yükseltmiş durumda, ikinci kez yükseltmek kayda yanlış kod
  geçirirdi

## Kapsam kararı: `safety::evaluate()` içeriden çağırıyor

`app_core`'un iki ayrı çağrı yapması gerekmiyor. Bir çağrının unutulması
kuru çalışma korumasını **sessizce** devre dışı bırakırdı — tespit edilmesi
en zor arıza türü. Güvenlik zinciri tek çağrıyla çalışıyor. Döngüsel
bağımlılık yok: `flow`, `safety::evaluate()`'i çağırmıyor.

**TASK-031: TAMAMLANDI** (donanım kalibrasyonu ve kuru çalışma testi bekliyor).
