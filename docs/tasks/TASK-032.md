# TASK-032 — Emergency Stop Latch & Recovery

**Phase:** 6 — Safety & Actuator System · **Priority:** **P0**

## Objective

Kritik ihlallerde mandallı (latching) bir acil durum mekanizması kurmak: tüm aktüatörler
kapanır, kilitlenir ve **yalnızca açık operatör onayıyla** normale dönülür.

## Scope

- Acil durum mandalı (set / sorgula / temizle)
- Tetikleyicilerin bağlanması (§12.3)
- Mandal aktifken tüm aktüatörlerin kilitlenmesi
- Neden bilgisinin saklanması ve gösterilmesi
- Operatör onaylı kurtarma akışı
- Mandalın kalıcı olup olmayacağı kararı

## Out of Scope

- Kilitlerin hesaplanması (TASK-030)
- Web/OLED arayüzü (TASK-043, TASK-052)
- Sistem modu makinesi (TASK-012 — burada yalnızca tetiklenir)

## Dependencies

- TASK-030, TASK-012

## Requirements

- `REQUIREMENTS.md` — §11-Critical (safe state), §9 (safe state yok)

## Architecture References

- §12.1 Katman 3 · §12.2 Mandallama ilkesi · §12.3 Tetikleyiciler
- §2.14 SystemSupervisor (EMERGENCY modu)

## Expected Design

### Mandallama ilkesi (§12.2) — neden kendiliğinden temizlenmez

> Kritik ihlal kendiliğinden temizlenmez. Koşul düzelse bile operatör onayı gerekir.

Gerekçe: aralıklı bir arıza (gevşek kablo, ara ara boşalan hazne) kendiliğinden temizlenen
bir sistemde **sessizce tekrarlanır** ve fark edilmez. Pompa her seferinde birkaç saniye
kuru çalışır ve haftalar içinde ölür. Mandal, insanın olaydan haberdar olmasını zorunlu kılar.

### Tetikleyiciler (§12.3)

| Tetikleyici | Kaynak |
|---|---|
| Kuru çalışma (akış doğrulama başarısız) | TASK-031 |
| Kritik su seviyesi | TASK-026 / TASK-030 |
| maxRunTime tekrarlı aşımı | TASK-029 |
| Güvenlik sensörü arızası | TASK-027 / TASK-030 |
| Task heartbeat kaybı | TASK-012 |
| Operatör komutu (web / OLED) | TASK-008 |

### Karar gerektiren nokta — Mandal kalıcı mı olmalı

```text
Problem:      Acil durum mandallıyken güç kesilip geri gelirse ne olmalı?
Constraints:  Boot'ta aktüatörler zaten kapalı başlar (§19);
              ancak arızanın nedeni hâlâ mevcut olabilir;
              kalıcı mandal, kullanıcının müdahale edemediği durumda sistemi
              kilitli bırakabilir
Approaches:   (a) mandal yalnızca RAM'de — reset ile temizlenir
              (b) mandal NVS'te kalıcı — reset ile temizlenmez
              (c) kalıcı ama boot'ta "önceki oturumda acil durum vardı" uyarısı,
                  koşullar sağlıklıysa normal başlangıç
Trade-offs:   (a) kullanıcı sorunu reset atarak "çözebilir" — tehlikeli
              (b) en güvenli ama saha müdahalesi gerektirir
Recommended:  (b) veya (c) — güvenlik lehine karar verilmeli;
              (a) kabul edilemez
```

## Implementation Notes

- Mandal aktifken `SafetyMonitor.permits()` **tüm** aktüatörler için ret döndürmeli.
- Temizleme işlemi açık bir operatör eylemi olmalı: web'de onay gerektiren bir istek
  (`POST /api/system/emergency-clear`) veya OLED'de bilinçli bir onay adımı.
  Yanlışlıkla temizlenebilecek bir tasarım (tek tuş) kabul edilemez.
- Temizleme öncesi **koşulların düzeldiği kontrol edilmeli**: hâlâ su seviyesi kritikse
  temizleme reddedilmeli. Aksi halde operatör onay verir, pompa açılır, aynı arıza tekrarlar.
- Neden bilgisi (tetikleyici + zaman + ilgili sensör değerleri) saklanmalı; kullanıcı
  ne olduğunu görebilmeli.
- Acil durum komutunun kuyruk doluyken bile ulaşması gerekir (TASK-008 garantili yol).
- Mandal aktifleşirken CRITICAL loglanmalı ve kalıcı kaydedilmeli.

## Files

- `src/domain/EmergencyStop.h` / `.cpp` (yeni)
- `src/domain/SafetyMonitor.cpp` (güncelleme)
- `src/domain/SystemSupervisor.cpp` (güncelleme — EMERGENCY modu)

## Acceptance Criteria

- [ ] Mandal set edilebiliyor, sorgulanabiliyor, onayla temizlenebiliyor
- [ ] Mandal aktifken tüm aktüatörler kilitli
- [ ] Kalıcılık kararı verildi ve gerekçelendirildi
- [ ] Temizleme açık operatör eylemi gerektiriyor; kaza ile temizlenemiyor
- [ ] Temizleme öncesi koşullar kontrol ediliyor; düzelmemişse reddediliyor
- [ ] Neden bilgisi saklanıyor ve okunabiliyor
- [ ] Tüm tetikleyiciler bağlı
- [ ] Operatör acil durdurma komutu kuyruk doluyken de ulaşıyor
- [ ] Mandal aktifleşirken CRITICAL loglanıyor

## Test Plan

- [ ] Her tetikleyici tek tek uyarılıp mandalın aktifleştiği doğrulandı
- [ ] Mandal aktifken hiçbir aktüatör açılamıyor (manuel, otomatik, web, OLED)
- [ ] Koşul düzelse bile mandal kendiliğinden temizlenmiyor
- [ ] Koşul düzelmeden temizleme talebi reddediliyor
- [ ] Onaylı temizleme sonrası sistem normale dönüyor
- [ ] Güç kesme sonrası kalıcılık kararına uygun davranıyor
- [ ] Neden bilgisi web ve OLED'de doğru gösteriliyor
- [ ] Operatör acil durdurma komutu yük altında da işleniyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§12.1 Katman 3, §12.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — acil durum yolu hızlı mı
- [ ] Shared state güvenli mi? — mandal durumu
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **bu task'ın özü**
- [ ] ESP32 resource kullanımı uygun mu? — kalıcı mandal flash yazması
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + tüm tetikleyiciler donanımda test edildi + mandalın kaza ile temizlenemediği
doğrulandı + kalıcılık davranışı güç kesme testiyle kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Mandal KALICI (yaklaşım b), (c) reddedildi

```text
(a) Yalnizca RAM  → REDDEDILDI (plan da kabul edilemez diyor):
    kullanici sorunu "reset atarak cozer". Ariza durur gibi gorunur.

(c) Kalici ama boot'ta kosullar saglikliysa normal baslangic → REDDEDILDI.
    Gerekce: KURU CALISMA icin (c), (a)'ya CÖKER. Guc kesildikten sonra
    pompa zaten kapalidir; akis dogal olarak sifirdir ve "kosul" degerlendirilemez.
    Su seviyesi de bu arada duzelmis olabilir. Sonuc: mandal her guc
    dongusunde temizlenir — yani hic mandal yoktur.

(b) NVS'te kalici, YALNIZCA acik operator onayiyla temizlenir → SECILDI.
```

**Bilinen bedeli:** operatör sahada değilse sistem kilitli kalır. Bu bilinçli
bir takas: kilitli bir sistem, sessizce ölen bir pompadan iyidir.

## Karar 2 — Yazma sırası: ÖNCE güvenli hâl, SONRA kalıcı kayıt

```text
NVS yazmasi bloklayan bir flash islemidir (~10-20 ms). Acil durum yolu
bloklamamalidir (P3).

SIRA:
  1. RAM mandali set et        (mikrosaniyeler)
  2. forceAllOff()             (roleler GUVENLI — is burada biter)
  3. CRITICAL logla
  4. NVS'e yaz                 (bloklar, ama artik her sey guvenli)

Adim 4 basarisiz olsa bile 1-3 gecerlidir. Kalicilik bir IYILESTIRMEDIR;
guvenligin kendisi RAM mandalinda ve kapali rolelerdedir.
```

## Karar 3 — Temizleme: iki koşullu

```text
1. ACIK OPERATOR EYLEMI  — `clear()` yalnizca komut yolundan cagrilir.
   Tek tusla temizlenemez; web'de onay gerektiren istek, OLED'de bilincli
   onay adimi (TASK-043 / TASK-052 uygular).

2. KOSULLARIN DUZELMIS OLMASI — `clear()` mevcut kilit maskesini kontrol eder.
   Acil durum disinda BASKA bir kilit hala aktifse temizleme REDDEDILIR.
   Aksi halde operator onay verir, pompa acilir, ayni ariza tekrarlar.
```

## Karar 4 — Neden bilgisi: kod + zaman + boot sayısı

`FixedString` ile serbest metin **saklanmaz** (Diagnostics deseniyle
tutarlı): 12 baytlık kayıt yerine kilobaytlarca metin tutmak gömülü bir
sistemde savunulamaz. Kod → metin çevirisi arayüz katmanının işidir.

## Karar 5 — `AIR_PUMP` de kesiliyor — endişemi kaydediyorum

Kabul kriteri "mandal aktifken **tüm** aktüatörler kilitli" diyor ve
uygulanan budur. Ancak hidroponik bir sistemde havalandırmanın günlerce
durması kök çürümesine yol açar; acil durumun nedeni su pompasıyla
ilgiliyken havalandırmayı da kesmek ikinci bir zarar üretebilir.

Spesifikasyona uyuyorum, kararı değiştirmiyorum — **ISSUE-017** olarak
kaydedildi.

## Karar 6 — Garantili acil durum yolu zaten var

`core::cmdq::postEmergencyStop()` / `takeEmergencyStop()` (TASK-008) kuyruğu
tamamen atlayan atomik bir yoldur. Kuyruk doluyken bile acil durdurma ulaşır.
Bu task o yolu **tüketir**, yeniden icat etmez.

---

# STEP 3 — REVIEW RECORD

- [x] Mandal set edilebiliyor, sorgulanabiliyor, onayla temizlenebiliyor
- [x] Mandal aktifken **tüm** aktüatörler kilitli
      (`masksFor()` → `ILK_EMERGENCY_LATCHED` her aktüatörde;
      `static_assert`larla derleme zamanında kilitli)
- [x] Kalıcılık kararı: **NVS**, gerekçesi kayıtlı (Karar 1)
- [x] Kalıcı kayıt `NS_CONFIG`'ten AYRI bir namespace'te (`NS_SYSTEM`) —
      fabrika ayarlarına dönüş bir güvenlik mandalını sessizce temizleyemez
- [x] Temizleme açık operatör eylemi gerektiriyor (`acknowledge()` yalnızca
      komut yolundan çağrılır; arayüzdeki onay adımı TASK-043/052'nin işi)
- [x] Temizleme öncesi **canlı** koşullar kontrol ediliyor; su seviyesi
      uygun değilse reddediliyor
- [x] Neden bilgisi saklanıyor (kod + kaynak + uptime + boot sayısı)
- [x] Mandal aktifleşirken CRITICAL loglanıyor ve kalıcı kaydediliyor
- [x] Yeniden girişte ilk neden korunuyor — sonraki tetikleyiciler asıl
      nedeni gizlemiyor
- [x] Acil durum yolu bloklamıyor: RAM mandalı → röleler güvenli → log →
      **en son** NVS yazması
- [x] Derleme temiz
- [ ] **Tetikleyicilerin bağlanması TASK-033'te tamamlanacak** — bu task
      mekanizmayı kuruyor, döngüye bağlama app_core'un işi
- [ ] **Donanım testleri (güç kesme dahil) — donanım gerekiyor**

## Bulduğum tuzak: kurtarma kendini engelliyordu

`emergency::clear(blockingMask)` koşullar düzelmemişse reddediyor. Ancak
`blockingMask` içinde `ILK_DRY_RUN` de var ve **kuru çalışma mandalı
yalnızca `flow::acknowledge()` ile temizleniyor.**

Sonuç: kuru çalışma nedeniyle mandallanmış bir sistemde acil durum onayı
**sonsuza dek reddedilirdi.** Operatör hazneyi doldurur, onaylar, reddedilir;
tekrar dener, yine reddedilir. Kurtarma yolu yok.

Kök neden: iki bağımsız mandal ve iki ayrı temizleme yolu. Düzeltme —
`safety::acknowledge()` **tek onay noktası** oldu:

```text
1. YALNIZCA canli fiziksel kosullari kontrol et (su seviyesi)
2. flow mandalini temizle
3. emergency mandalini + asim sayaclarini temizle
4. kilitleri yeniden hesapla
```

Adım 1'in yalnızca canlı koşullara bakması kritik: mandalların ve sayaçların
kontrol edilmesi tam da yukarıdaki kendini-engelleyen kilidi üretirdi.

## Endişemi kaydediyorum: ISSUE-017

Kabul kriteri "mandal aktifken **tüm** aktüatörler kilitli" diyor ve
uygulanan budur. Ancak hidroponik bir sistemde havalandırmanın günlerce
durması kök çürümesine yol açar; acil durumun nedeni su pompasıyla
ilgiliyken havalandırmayı da kesmek **ikinci bir zarar** üretebilir.

Spesifikasyona uydum, kararı değiştirmedim. `docs/ISSUES.md`'ye kaydedildi.

**TASK-032: TAMAMLANDI** (döngüye bağlama TASK-033, donanım testleri bekliyor).
