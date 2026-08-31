# TASK-029 — ActuatorManager & Arbitration

**Phase:** 6 — Safety & Actuator System · **Priority:** **P0**

## Objective

Aktüatörlerin **tek sahibi** olmak. Üç farklı kaynaktan (güvenlik, manuel, otomasyon)
gelen talepleri tahkim etmek, kısıtları uygulamak ve fiziksel çıkışa **tek kapıdan** yazmak.

## Scope

- Talep kabul arayüzü (`request(id, desiredState, source)`)
- Tahkim: `SAFETY` > `MANUAL` > `AUTOMATION`
- Kısıt uygulaması: minRunTime, maxRunTime, cooldown
- Fiziksel çıkışa yazma (yalnızca buradan)
- `forceAllOff(reason)` — acil durum yolu
- `actuators` alt-state'inin yayınlanması

## Out of Scope

- Güvenlik kilitlerinin hesaplanması (TASK-030) — burada yalnızca **sorgulanır**
- Otomasyon kuralları (TASK-057)
- Manuel override süresi (TASK-057)

## Dependencies

- TASK-017, TASK-028

## Requirements

- `REQUIREMENTS.md` — §4.1/4.2 (otomatik kontrol yok, güvenlik koşulları yok), Kritik Problem 2

## Architecture References

- §2.6 ActuatorManager · §10.3 Tahkim · §10.4 Komut sonucu
- §3.3 Komut akışı · §12.1 Güvenlik zinciri

## Expected Design

### Tek kapı kuralı — pazarlıksız

> Röle GPIO'suna **yalnızca `ActuatorManager`** yazar ve **yalnızca `app_core` task'ından**
> çağrılır. Web, UI, otomasyon veya başka bir servis doğrudan `RelayOutput`'a erişemez.

Mevcut projede WebSocket handler'ı doğrudan `digitalWrite(pin, ...)` yapıyordu — bu, AsyncTCP
task bağlamından güvenlik kontrolü olmadan pompa sürmek demektir. Yeni tasarımda bu yol
yapısal olarak kapalıdır.

### Tahkim sırası (§10.3)

```text
   1. SAFETY      ── veto / zorla kapat        → her zaman kazanır
   2. MANUAL      ── operatör override         → süreli
   3. AUTOMATION  ── kural motoru              → varsayılan
```

### Karar gerektiren nokta — maxRunTime aşımı davranışı

```text
Problem:      Pompa maxRunTime'ı aştı. Ne yapılmalı?
Constraints:  Sürekli tekrar eden aşım gerçek bir arızaya işaret eder;
              tek seferlik aşım normal bir uzun sulama olabilir
Approaches:   (a) sessizce kapat
              (b) kapat + WARNING
              (c) kapat + WARNING; N kez tekrarlarsa acil duruma geç
Trade-offs:   (a) arızayı gizler; (c) tekrarlayan sorunu yakalar
Recommended:  (c) — ARCHITECTURE §12.3 "maksimum çalışma süresi tekrarlı aşımı"
              acil durum tetikleyicisidir
```

## Implementation Notes

- Her açma talebinde `SafetyMonitor.permits()` **mutlaka** sorgulanmalı; önbelleğe alınmış
  eski izin kullanılmamalı. Güvenlik durumu döngüler arasında değişebilir.
- `forceAllOff()` acil durum yolundan çağrılır: hızlı, bloklamayan, kısıt tanımayan olmalı.
  `minRunTime` acil durumda **uygulanmaz**.
- Yayınlanan durum **gerçek pin durumu** olmalı, talep edilen değil. Talep ile gerçek
  arasındaki fark bir hata göstergesidir ve raporlanmalıdır.
- Her reddedilen talep nedeniyle birlikte loglanmalı (§12.2 gözlemlenebilirlik). Sessiz
  engelleme kullanıcıyı "sistem bozuk" sanmaya iter.
- Kısıt kontrolü ile fiziksel yazma arasında başka bir işlem olmamalı.
- Boot'ta aktüatör durumu **geri yüklenmez** (§19); her boot rölesiz başlar.

## Files

- `src/domain/ActuatorManager.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Röle GPIO'suna yalnızca bu modül yazıyor
- [ ] Tahkim sırası doğru uygulanıyor
- [ ] Her açma talebinde güvenlik izni taze olarak sorgulanıyor
- [ ] Üç kısıt da doğru uygulanıyor
- [ ] maxRunTime aşımı davranışı seçildi ve uygulandı
- [ ] `forceAllOff()` kısıt tanımıyor ve bloklamıyor
- [ ] Yayınlanan durum gerçek pin durumu
- [ ] Talep/gerçek uyuşmazlığı tespit ediliyor
- [ ] Her reddedilen talep neden koduyla loglanıyor
- [ ] Boot'ta durum geri yüklenmiyor

## Test Plan

- [ ] Güvenlik vetosu varken açma talebi reddediliyor (`REJECTED_SAFETY`)
- [ ] `minRunTime` dolmadan kapatma ertelenıyor
- [ ] `cooldown` dolmadan açma ertelenıyor
- [ ] `maxRunTime` aşımında zorla kapanıyor ve loglanıyor
- [ ] Tekrarlı aşımda acil duruma geçiliyor
- [ ] `forceAllOff()` her koşulda röleleri kapatıyor (minRunTime dahil)
- [ ] MANUAL komut AUTOMATION kararını geçersiz kılıyor
- [ ] SAFETY her ikisini de geçersiz kılıyor
- [ ] Talep/gerçek uyuşmazlığı yapay olarak üretilip tespit edildiği doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.6, §10.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — `forceAllOff()` yolu
- [ ] Shared state güvenli mi? — **tek sahip/tek task kuralı**
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — reddedilen talepler loglanıyor mu
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — yalnızca `app_core`'dan çağrılıyor mu
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **WS handler'dan doğrudan GPIO sürme yasak**

## Definition of Done

Ortak DoD + tüm kısıt ve tahkim senaryoları donanımda test edildi + röleye başka hiçbir
yerden erişilmediği kod taramasıyla doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Tek kapı, YAPISAL olarak

```text
Eski sistem: WebSocket handler'i DOGRUDAN digitalWrite(pin, ...) yapiyordu.
             Yani AsyncTCP task baglamindan, guvenlik kontrolu OLMADAN,
             pompa suruluyordu (REQUIREMENTS Kritik Problem 2).

Yeni: role GPIO'suna yalnizca `hal::relay::set()` yazar; `hal::relay::set()`
      cagrisi da yalnizca `ActuatorManager::apply()` icinde bulunur.
      Bu, TARAMA ile denetlenebilir bir kural — inceleme adiminda dogrulanir.
```

## Karar 2 — `request()` NİYET kaydeder, `apply()` uygular

```text
Problem:  Kisit nedeniyle ertelenen bir talep ne olacak?
          Ornek: operator KAPAT dedi, minRunMs dolmamis.
Approaches: (a) talebi DUSUR, DEFERRED don
            (b) niyeti MANDALLA, her donguda yeniden dene
Trade-off: (a) operatorun KAPAT komutu KAYBOLUR; pompa maxRunMs'e kadar
               calismaya devam eder. Kullanici "kapatma calismiyor" der.
Selected:  (b)
```

## Karar 3 — Erteleme mandallanır, GÜVENLİK REDDİ MANDALLANMAZ

**Bu bir güvenlik kararıdır, tutarlılık kararı değildir.**

```text
DEFERRED_COOLDOWN / DEFERRED_MIN_RUNTIME → niyet mandallanir
    Gerekce: zamanlama kisiti. Kisa, deterministik, ongorulebilir.
             Operator "birazdan olacak" bekler.

REJECTED_SAFETY → niyet MANDALLANMAZ, talep DUSURULUR
    Gerekce: Operator hazne bosken ACMA'ya basti. Mandallansaydi, 20 dakika
             sonra hazne dolunca POMPA KENDILIGINDEN CALISIRDI — kimse
             basinda degilken. Bu tam olarak "surpriz aktuator hareketi"dir
             ve ARCHITECTURE P6'ya aykiridir.
             Guvenlik engeli kalkinca yeni bir talep GEREKIR.
```

`apply()` her döngüde güvenlik iznini **taze** sorar; mandallanmış bir AÇ
niyeti, uygulanmadan önce güvenlik engeline takılırsa niyet **temizlenir**.

## Karar 4 — Güvenlik izni fonksiyon işaretçisiyle sorulur

```text
`SafetyPermitFn` — TASK-012'deki `SafeStateHandler` deseninin aynisi.

Neden dogrudan SafetyMonitor cagrilmiyor (ikisi de domain/, ihlal degil):
  1. ONBELLEK YASAGI yapisal hale gelir — manager'in saklayacagi bir
     guvenlik nesnesi yok, her seferinde SORMAK zorunda.
  2. Host testi: TASK-064 sahte bir izin fonksiyonuyla tum tahkim ve
     kisit senaryolarini donanimsiz kosturabilir.
```

## Karar 5 — `maxRunMs` aşımı: (c) kapat + WARNING + sayaç

Seçilen yaklaşım ARCHITECTURE §12.3 ile uyumlu. Tek seferlik aşım normal bir
uzun sulama olabilir; **tekrarlayan** aşım sistemik arızadır. Manager sayar,
SafetyMonitor eşiği (`maxRuntimeViolations`) izler ve acil duruma geçirir
(TASK-028 Karar 3).

## Karar 6 — Yayınlanan durum GERÇEK pin durumu

```text
`hal::relay::isOn()` pin register'ini okur. Yayinlanan `ActuatorStatus.isOn`
bu degerdir — TALEP EDILEN degil.

Talep != gercek ise: ACTUATOR_STATE_MISMATCH yukseltilir.
Neden onemli: arayuzde "pompa calisiyor" yazarken pompanin durmus olmasi,
operatoru yanlis bir guven duygusuna sokar. §2.6.
```

## Karar 7 — `forceAllOff()` kısıt TANIMAZ

`minRunMs` acil durumda uygulanmaz. Kısa çevrim aşınması, taşan bir hazneden
veya kuru çalışan bir pompadan **ucuzdur**. Yol bloklamaz, tahsis yapmaz,
tek işi `hal::relay::allSafe()` çağırıp niyetleri temizlemektir.

## Karar 8 — Boot'ta durum GERİ YÜKLENMEZ (ARCHITECTURE §19)

Her boot röleler kapalı başlar. Beklenmedik bir reset sonrası pompanın
kendiliğinden yeniden başlaması, tam da reset'e yol açan arıza sürerken
olur. Sessiz yeniden başlatma yerine sessiz duruş tercih edilir.

## Karar 9 — Çağıran task doğrulaması (çalışma zamanı)

`begin()` çağıran task handle'ını saklar; `apply()` farklı bir task'tan
çağrılırsa CRITICAL loglanır. `StateStore`'daki tek-yazar doğrulamasıyla
aynı desen: **reddetmez, raporlar** — güvenlik yolunu bir denetimin
kilitlemesi ihtimalinden kaçınılır.

---

# STEP 3 — REVIEW RECORD

- [x] Röle GPIO'suna **yalnızca bu modül** yazıyor — tarama ile doğrulandı:
      `hal::relay::set|allSafe` çağrısı yalnızca `ActuatorManager.cpp:52` ve
      `:255`. `src/hal/` dışında `digitalWrite` **0 tane** (yalnızca eski
      deseni anlatan yorumlar).
- [x] Tahkim sırası `sourceOutranks()` ile uygulanıyor; sıra
      `static_assert`larla derleme zamanında kilitli
- [x] Her açma talebinde güvenlik izni **taze** sorgulanıyor — `request()`
      içinde bir kez, `apply()` içinde tekrar. Manager'ın önbelleğe alacağı
      bir güvenlik nesnesi yok (fonksiyon işaretçisi deseni)
- [x] Üç kısıt da uygulanıyor (`canTurnOn` / `canTurnOff` / `maxRunExceeded`)
- [x] `maxRunMs` aşımı: (c) kapat + WARNING + sayaç
- [x] `forceAllOff()` kısıt tanımıyor, bloklamıyor, tahsis yapmıyor; **`g_ready`
      kontrolüne bile bağlı değil** — güvenlik yolu başlatma durumuna bağlı olamaz
- [x] Yayınlanan `isOn` **gerçek pin durumu** (`hal::relay::isOn()` → `digitalRead`)
- [x] Talep/gerçek uyuşmazlığı `ACTUATOR_STATE_MISMATCH` olarak yükseltiliyor
- [x] Her reddedilen/ertelenen talep neden koduyla loglanıyor
- [x] Boot'ta durum geri yüklenmiyor (`begin()` tüm runtime'ı sıfırlar)
- [x] Heap kullanımı yok; tüm durum statik (4 × 32 bayt)
- [x] Derleme temiz — `-Wall -Wextra` ile uyarı yok
- [ ] **Kısıt ve tahkim senaryolarının donanımda testi — donanım gerekiyor**

## Tasarımın yakaladığı iki tuzak

**1. Ertelenen KAPAT komutu kayboluyordu.** İlk taslakta `request()` kısıt
ihlalinde erken dönüyordu; niyet kaydedilmiyordu. Somut sonucu: operatör
KAPAT'a basar, `minRunMs` dolmamıştır, komut **düşer** ve pompa `maxRunMs`
dolana kadar çalışmaya devam eder. Niyet mandallama deseniyle giderildi.

**2. Mandallamanın güvenlik tarafında ters etkisi.** Aynı deseni güvenlik
reddine de uygulasaydım: hazne boşken verilen AÇ komutu mandallanır, hazne
20 dakika sonra dolduğunda **pompa kimse başında değilken kendiliğinden
çalışırdı**. Güvenlik reddi bilinçli olarak mandallanmıyor; ayrıca
`apply()` mandallanmış bir AÇ niyetini güvenlik engeline takıldığında
**temizliyor**.

Bu ikisi zıt yönlü kararlardır ve ikisinin de gerekçesi kodda yazılıdır.

## Kayıt: ISSUE-016 (kapsam dışı)

`ActuatorManager` canlı `Config` referansını tutuyor; `ConfigService`
güncellemeleri `net`/`store` bağlamından gelebilir. Tek alan okumaları ESP32'de
hizalı 32-bit olduğu için yırtılmaz, ancak alanlar arası tutarsız bir ara
görüntü mümkündür (bir döngü boyunca eski `minRunMs` + yeni `maxRunMs`).
Etkisi bir döngüyle sınırlı ve tehlikesiz. Yapılandırma eşzamanlılığı bu
task'ın kapsamı değil — `docs/ISSUES.md`'ye kaydedildi.

**TASK-029: TAMAMLANDI** (donanım testleri bekliyor).
