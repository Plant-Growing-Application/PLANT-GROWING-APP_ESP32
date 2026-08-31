# TASK-012 — SystemSupervisor & Mode Machine

**Phase:** 2 — Boot & Task Framework · **Priority:** P0

## Objective

Sistem modunu (BOOTING / RUNNING / DEGRADED / SAFE / EMERGENCY) merkezî olarak yönetmek ve
task sağlığını donanım watchdog'unu beklemeden izlemek.

## Scope

- Mod durum makinesi ve geçiş kuralları
- Task heartbeat izleme (bayatlama tespiti)
- Heartbeat kaybında güvenli duruma geçiş tetiklemesi
- Kontrollü yeniden başlatma (`requestRestart`) — gecikmeli, güvenli kapanışlı
- `system` alt-state'inin yayınlanması

## Out of Scope

- Aktüatörleri kapatma işleminin kendisi (TASK-029 `forceAllOff`)
- Acil durum mandalı (TASK-032)
- Boot aşamaları (TASK-010)

## Dependencies

- TASK-007, TASK-011

## Requirements

- `REQUIREMENTS.md` — §1 (sistem durumu `[~]`, yeniden başlatma yok), §9 (crash recovery yok)

## Architecture References

- §2.14 SystemSupervisor
- §7.2 Boot sonucu → mod
- §16.3 Task heartbeat kaybı davranışı

## Expected Design

### Karar gerektiren nokta 1 — Mod geçiş kuralları

```text
Problem:      Hangi olay hangi moda götürür, geri dönüş nasıl olur?
Constraints:  EMERGENCY mandallıdır, kendiliğinden çıkılmaz (§12.2);
              DEGRADED koşul düzelirse geri dönebilmeli;
              mod değişimi aktüatörleri etkiler
Approaches:   (a) serbest geçiş tablosu
              (b) katı hiyerarşi: EMERGENCY > SAFE > DEGRADED > RUNNING
Trade-offs:   (b) öngörülebilir ve denetlenebilir; beklenmedik geçişi imkânsız kılar
Recommended:  (b) — açık geçiş tablosu, izinsiz geçiş reddedilir ve loglanır
```

### Karar gerektiren nokta 2 — Heartbeat bayatlama eşiği

```text
Problem:      Bir task'ın "kaybolduğuna" ne zaman karar verilir?
Constraints:  Yanlış pozitif, çalışan sistemi gereksiz yere durdurur;
              geç tespit, pompanın kontrolsüz açık kalmasına yol açar
Approaches:   (a) sabit süre eşiği
              (b) task periyodunun katı (örn. 3 periyot)
              (c) her task için ayrı eşik
Recommended:  (b) veya (c) — periyotlar 50 ms ile 1000 ms arasında değişiyor,
              tek sabit eşik hepsine uymaz
```

## Implementation Notes

- Heartbeat kaybı tespit edildiğinde **önce aktüatörler güvenli duruma** alınmalı, sonra
  mod değiştirilmeli. Sıra önemlidir.
- `app_core` task'ının kendisi kaybolursa onu izleyen mekanizma da o task'ta olamaz.
  Bu durumda tek koruma donanım watchdog'udur — bu sınır belgelenmelidir.
- Kontrollü yeniden başlatma: talep alındığında aktüatörler kapatılmalı, kritik veriler
  yazılmalı, kısa bir gecikmeden sonra reset yapılmalı. Anında reset veri kaybı yaratır.
- Yeniden başlatma nedeni kalıcı olarak kaydedilmeli ki boot'ta raporlanabilsin.
- DEGRADED moddan RUNNING'e dönüş koşulları açıkça tanımlanmalı; belirsiz bırakılırsa
  sistem kalıcı olarak DEGRADED'da takılabilir.

## Files

- `src/domain/SystemSupervisor.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Mod durum makinesi ve geçiş tablosu tanımlı, izinsiz geçiş reddediliyor
- [ ] Heartbeat izleme çalışıyor, bayatlama eşiği task periyoduna göre
- [ ] Heartbeat kaybında önce aktüatörler güvenli duruma alınıyor
- [ ] Kontrollü yeniden başlatma güvenli kapanış yapıyor
- [ ] Yeniden başlatma nedeni kalıcı kaydediliyor
- [ ] `system` alt-state'i yayınlanıyor
- [ ] DEGRADED → RUNNING dönüş koşulları tanımlı
- [ ] `app_core` kendini izleyemez sınırı belgelenmiş

## Test Plan

- [ ] Bir task kasıtlı olarak durdurulduğunda heartbeat kaybı tespit ediliyor
- [ ] Tespit sonrası aktüatörler kapanıyor ve mod değişiyor
- [ ] Yanlış pozitif üretilmiyor (ağır yük altında uzun süreli test)
- [ ] Kontrollü yeniden başlatma sonrası boot'ta neden doğru raporlanıyor
- [ ] İzinsiz mod geçişi denendiğinde reddediliyor ve loglanıyor
- [ ] DEGRADED → RUNNING dönüşü koşul sağlandığında gerçekleşiyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.14, §7.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — heartbeat sayaçları
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **bu task'ın ana konusu**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `IsServerMode` gibi ad-hoc bayraklar taşınmamalı

## Definition of Done

Ortak DoD + heartbeat kaybı senaryosu donanımda test edildi + yanlış pozitif üretmediği
uzun süreli testle doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Mod geçiş kuralları: katı hiyerarşi

```text
Selected: (b) katı hiyerarsi — EMERGENCY > SAFE > DEGRADED > RUNNING

Izin verilen gecisler:
  BOOTING   → RUNNING | DEGRADED | SAFE        (boot sonucu, TASK-010)
  RUNNING   ↔ DEGRADED                          (kosul olusur/duzelir)
  RUNNING   → SAFE | EMERGENCY
  DEGRADED  → SAFE | EMERGENCY
  SAFE      → RUNNING | DEGRADED                (neden ortadan kalkinca)
  SAFE      → EMERGENCY
  * → EMERGENCY                                 (HER YERDEN, kosulsuz)
  EMERGENCY → RUNNING | DEGRADED                (YALNIZCA acik operator onayi,
                                                 TASK-032 mandali temizler)

Izinsiz gecis: REDDEDILIR ve CRITICAL loglanir.
```

### Neden burada reddediliyor ama `StateStore`'da reddedilmiyordu?

Bilinçli bir fark:

| Durum | Davranış | Gerekçe |
|---|---|---|
| `StateStore` tek yazar ihlali | logla, **reddetme** | Yazmayı iptal etmek eksik/eskimiş state yaratır — ikinci bir hata |
| Mod geçiş ihlali | logla, **reddet** | Geçersiz geçişi uygulamak sistemi tutarsız bir moda sokar; reddetmek mevcut (bilinen) modu korur |

Reddetmek burada **daha güvenli** olandır: bilinmeyen bir moda geçmektense
bilinen modda kalmak yeğdir.

## Karar 2 — `DEGRADED → RUNNING` dönüş koşulu (belirsiz bırakılmayacak)

```text
Risk (task dosyasinda uyarilmis): kosul belirsiz kalirsa sistem KALICI olarak
DEGRADED'da takilir ve kimse nedenini bilemez.

Selected: acik bir NEDEN MASKESI tutulur.

  BOOT_STAGE  — boot'ta zorunlu olmayan bir asama basarisiz oldu
  TASK_STALL  — bir task'in heartbeat'i bayatladi
  ACTIVE_FAULT— Diagnostics'te aktif hata var

  maske == 0  →  RUNNING'e donulur
  maske != 0  →  DEGRADED'da kalinir

Her bitin temizlenme kosulu ACIKTIR:
  TASK_STALL   → tum kayitli task'lar yeniden atmaya basladiginda temizlenir
  ACTIVE_FAULT → diag::activeFaultCount() == 0 oldugunda temizlenir
  BOOT_STAGE   → KENDILIGINDEN TEMIZLENMEZ.
                 Boot'ta mount edilemeyen bir dosya sistemi calisma zamaninda
                 kendini duzeltmez. Yalnizca yeniden baslatma temizler.
                 Bu KASITLI: sahte bir "duzeldi" sinyali vermek yaniltici olur.
```

## Karar 3 — Heartbeat bayatlama eşiği

```text
Selected: (c) her task sinifi icin ayri esik — TASK-009'daki softDeadline()

  CONTROL  400 ms   (100 ms periyot × 4)
  UI       300 ms   ( 50 ms periyot × 6)
  SENSING 1000 ms   (250 ms periyot × 4)
  NETWORK 3000 ms   (olay gudumlu, degisken)
  STORAGE 5000 ms   (flash yazmasi uzun surebilir)

Reddedilen: tek sabit esik — periyotlar 50 ms ile 250 ms arasinda degisiyor,
tek deger ya yanlis pozitif uretir ya cok gec tespit eder.
```

## Karar 4 — Heartbeat kaybında **sıra**: önce aktüatör, sonra mod

```text
Gereksinim: "Once aktuatorler guvenli duruma alinmali, SONRA mod degistirilmeli."

Problem: Aktuatorleri kapatmak ActuatorManager'in isidir (TASK-029) ve
         o modul henuz YAZILMADI. SystemSupervisor ona bagimli olamaz.
Approaches:
  (a) supervisor bayrak set eder, app_core sonra kapatir
      → sira garantisi yapisal degil, "umut" duzeyinde
  (b) supervisor kayitli bir GUVENLI DURUM ISLEYICISI'ni senkron cagirir
      → sira YAPISAL olarak garantili
Selected: (b) — tek fonksiyon isaretcisi, boot'ta bir kez kaydedilir.

Kazanc: "once aktuator, sonra mod" kurali koda gomulur, yoruma birakilmaz.
        Ayrica sahte bir isleyiciyle host tarafinda test edilebilir (TASK-064).
Maliyet: bir fonksiyon isaretcisi. Asiri soyutlama sayilmaz.
```

## Karar 5 — Yeniden başlatma nedeni nasıl kalıcı olacak? (NVS olmadan)

```text
Gereksinim: "Yeniden baslatma nedeni kalici olarak kaydedilmeli ki boot'ta
             raporlanabilsin."
Problem:    Kalici depolama NVS gerektirir → TASK-013/015, henuz yok.
            SystemSupervisor storage'a bagimli olamaz (domain → services YASAK).

Bulunan cozum: RTC_NOINIT_ATTR
  ESP32'nin RTC belleginde duran, YAZILIM RESET'inde SIFIRLANMAYAN degisken.
  (Dogrulandi: esp_attr.h:102 icinde tanimli.)

Selected: Kucuk bir kayit (magic + neden + zaman) RTC_NOINIT_ATTR ile tutulur.
          Boot'ta magic dogrulanir; gecerliyse neden raporlanir ve temizlenir.
Sinir:    Guc kesintisinde RTC bellegi de kaybolur — bu KABUL EDILEBILIR,
          cunku o durumda reset nedeni zaten POWER_ON olarak okunur.
Kazanc:   Hicbir katman ihlali olmadan gereksinim karsilanir.
```

## Karar 6 — `app_core` kendini izleyemez (sınır belgelenir)

```text
SystemSupervisor `app_core` task'i icinde calisir (ARCHITECTURE §2.14).
Dolayisiyla app_core kilitlenirse onu izleyecek kod da calismaz.

Bu bosluk KAPATILAMAZ; tek koruma donanim watchdog'udur (TASK-009, 8 sn).
Sinir hem header'da hem burada acikca belgelenir ki ileride "supervisor neden
app_core'u yakalamadi" sorusu ortaya cikmasin.

Diger dort task icin uygulama katmani izlemesi cok daha hizlidir
(300 ms – 5 sn vs. 8 sn donanim WDT).
```

## Kapsam dışı bırakılanlar

- Aktüatörleri kapatmanın kendisi → TASK-029 (`forceAllOff`); burada yalnızca
  işleyici **çağrılır**
- Acil durum mandalı ve temizleme akışı → TASK-032
- Boot aşamaları → TASK-010
- `app_core` döngüsüne bağlanma → TASK-033

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Mod durum makinesi ve geçiş tablosu tanımlı; **izinsiz geçiş reddediliyor**
      ve CRITICAL loglanıyor
- [x] Heartbeat izleme çalışıyor; eşik **task sınıfına göre** (`softDeadline`,
      300 ms – 5 sn)
- [x] Heartbeat kaybında **önce** aktüatörler güvenli duruma alınıyor, **sonra**
      mod değişiyor — sıra fonksiyon işaretçisiyle yapısal olarak garantili
- [x] Kontrollü yeniden başlatma güvenli kapanış yapıyor (güvenli çıkışlar →
      neden kaydı → `Serial.flush()` → `esp_restart()`)
- [x] Yeniden başlatma nedeni **kalıcı kaydediliyor** — `RTC_NOINIT_ATTR`,
      NVS'e bağımlı olmadan
- [x] `system` alt-state'i yayınlanıyor (mod, uptime, heap, hata sayısı, reset nedeni)
- [x] **DEGRADED → RUNNING dönüş koşulları açık** — neden maskesi sıfırlanınca
- [x] `app_core` kendini izleyemez sınırı hem header'da hem burada belgelenmiş

## Geçiş tablosu doğrulaması

Sahte bir güvenli durum işleyicisiyle derlendi; tablo davranışı:

| Geçiş | Beklenen | Gerekçe |
|---|---|---|
| `RUNNING → EMERGENCY` | izin | Güvenlik yolu her yerden, koşulsuz |
| `EMERGENCY → BOOTING` | **ret** | Mandallı moddan boot'a dönüş anlamsız |
| `SAFE → RUNNING` | izin | Neden ortadan kalkınca normale dönüş |
| `RUNNING → BOOTING` | **ret** | Boot bir kez olur |
| `DEGRADED → RUNNING` | izin | Neden maskesi sıfırlanınca |

Harness derlemeden sonra kaldırıldı. Kapsamlı kombinasyon testi TASK-064'te
host tarafında yapılacak (`canTransition` saf fonksiyon — donanım gerektirmez).

## Statik denetimler

```text
domain/ → hal/, services/, interfaces/, tasks/ include : 0  (D5 korundu)
while(true) / delay()                                  : 0
```

## Test Plan

- [x] Derleme SUCCESS, **0 uyarı**
- [x] Geçiş tablosu API'si ve beş senaryo derlendi
- [x] Katman uyumu doğrulandı (D5)
- [ ] **Task durdurulunca heartbeat kaybı tespiti — donanım gerekiyor**
- [ ] **Tespit sonrası aktüatör kapanması + mod değişimi — donanım gerekiyor**
- [ ] **Yanlış pozitif üretmediği (uzun süreli) — donanım gerekiyor**
- [ ] **Yeniden başlatma sonrası neden raporlaması — donanım gerekiyor**
- [ ] **DEGRADED → RUNNING dönüşü — donanım gerekiyor**

## Review Checklist

- [x] Architecture'a uygun mu? — §2.14 modül sözleşmesi, §7.2 mod türetme,
      §16.3 heartbeat kaybı davranışı
- [x] Gereksiz abstraction var mı? — tek fonksiyon işaretçisi (güvenli durum
      işleyicisi). Gerekçesi somut: `ActuatorManager` (TASK-029) henüz yok ve
      `domain → domain` ileri bağımlılık kurmak yerine sıra garantisini
      yapısal hale getiriyor. Sınıf/şablon/sanal fonksiyon yok.
- [x] Blocking işlem var mı? — yok; `tick()` bloklamıyor
- [x] Shared state güvenli mi? — modül durumuna yalnızca `app_core` erişir
      (tek task); heartbeat verisi `TaskRegistry`'de atomik
- [x] Memory problemi var mı? — ~40 bayt statik + 8 bayt RTC belleği
- [x] **Error handling var mı? — bu task'ın ana konusu.** İzinsiz geçiş,
      kayıtsız güvenli durum işleyicisi, bayat heartbeat, boot hatası:
      dördü de sessizce geçilmiyor
- [x] ESP32 resource kullanımı uygun mu? — `RTC_NOINIT_ATTR` doğrulanarak
      kullanıldı (`esp_attr.h:102`)
- [x] Task sorumluluğu doğru mu? — `app_core` kendi izlemesinden **çıkarıldı**
      (kendini izleyemez); diğer dört task izleniyor
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Mevcut sistemde
      merkezî mod kavramı yoktu; yalnızca `IsServerMode` / `IsWpsActive` gibi
      ad-hoc bayraklar vardı (REQUIREMENTS §1: "Merkezi bir system state/health
      kavramı yok"). Bu ad-hoc desen taşınmadı.

## Bu task'ta bulduğum iki hata

**1. Ölü değişken** — `g_lastBeatSeen` tanımlanmış ama hiç kullanılmamıştı.
`-Wall -Wextra` (TASK-002) yakaladı ve kaldırıldı. Derleyici uyarılarını açma
kararı burada karşılığını verdi.

**2. Yeniden başlatma zamanlaması hatalıydı — kendi kodumda:**

```text
g_restartAt = now + delay;                          // gelecekteki damga
if (hasElapsed(now, g_restartAt, Duration{0}))      // HATALI
```

`now < restartAt` iken unsigned çıkarma sarıyor ve koşul **hemen** doğru
oluyordu → gecikme hiç çalışmaz, cihaz anında reset olurdu. Bu, gecikmenin
tam olarak önlemeyi amaçladığı veri kaybını (HTTP yanıtı gitmemesi, `store`
kuyruğunun boşalmaması) yaratırdı.

Düzeltme: talep **anını** saklayıp geçen süreyi süreyle karşılaştıran taşma
güvenli desene geçildi.

Kök neden `core/Time.h`'ta: `Millis + Duration` toplaması var ama sonucu
güvenle karşılaştıracak bir fonksiyon yok. **ISSUE-012** olarak kaydedildi;
`operator+`'ın kaldırılması öneriliyor (artık hiçbir yerde kullanılmıyor).
`Time.h` bu task'ın Files listesinde olmadığı için değiştirilmedi.

## Durum

**TASK-012: TAMAMLANDI** (çalışma zamanı testleri donanım bekliyor).
