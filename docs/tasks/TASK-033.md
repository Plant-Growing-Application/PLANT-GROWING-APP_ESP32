# TASK-033 — app_core Task Loop

**Phase:** 6 — Safety & Actuator System · **Priority:** **P0**

## Objective

Sistemin karar merkezini kurmak: komutları tüketen, güvenliği değerlendiren ve aktüatörleri
süren tek task. `ARCHITECTURE.md` §11.1'deki değerlendirme döngüsünün sırası bu task'ta
uygulanır ve bu sıra değiştirilemez.

## Scope

- `app_core` task döngüsü (100 ms, en yüksek öncelik, Core 1)
- Snapshot alma → komut tüketme → güvenlik → (otomasyon) → aktüatör uygulama → yayınlama
- Komut işleme ve sonuç üretimi
- Heartbeat ve watchdog entegrasyonu
- Döngü süresi izleme

## Out of Scope

- Otomasyon motoru (TASK-057 — bu task'ta yalnızca **çağrı yeri ayrılır**)
- Güvenlik kilit hesabı (TASK-030)
- Aktüatör kısıtları (TASK-029)

## Dependencies

- TASK-029, TASK-030, TASK-031, TASK-032, TASK-008

## Requirements

- `REQUIREMENTS.md` — §10 (task sistemi), Kritik Problem 1 ve 2

## Architecture References

- §11.1 Değerlendirme döngüsü (adım sırası)
- §6.1 app_core satırı · §6.3 Öncelik gerekçesi

## Expected Design

### Döngü sırası — değiştirilemez

```text
  1. snapshot al
  2. komut kuyruğunu boşalt (sınırlı sayıda)
  3. SafetyMonitor.evaluate()        ◀── HER ZAMAN ÖNCE
  4. mod == AUTO ? AutomationEngine.evaluate() : mevcut durumu koru
  5. komutları uygula (override / mod değişimi / config)
  6. ActuatorManager.apply()
  7. StateStore.publish*() · heartbeat · watchdog besle
```

**Adım 3 asla adım 4'ten sonra gelmez.** Otomasyon güvenlik değerlendirmesi yapılmamış bir
state üzerinde karar veremez.

### Karar gerektiren nokta — Komut tüketme sınırı

```text
Problem:      Bir döngüde kaç komut işlenmeli?
Constraints:  Tümünü işlemek döngü süresini öngörülemez yapar;
              az işlemek komut gecikmesi yaratır;
              acil durdurma asla beklememeli
Approaches:   (a) kuyruğu tamamen boşalt
              (b) döngü başına en fazla N komut
              (c) süre bütçesi dolana kadar işle
Trade-offs:   (a) güvenlik döngüsünün periyodunu bozabilir
Recommended:  (b) — N küçük tutulmalı; acil durdurma komutu sınırdan muaf
```

## Implementation Notes

- Bu task **en yüksek önceliğe** sahiptir (4). Mevcut projede en yüksek öncelik loglama
  task'ındaydı; bu ters çevrilmiştir.
- Döngü içinde **hiçbir bloklama olmamalı**: flash yazma, ağ işlemi, uzun hesaplama yok.
  Kalıcılaştırma gerekiyorsa `store` task'ına kuyruklanır.
- Döngü süresi ölçülmeli ve izlenmeli; 100 ms periyodun belirgin altında kalmalı.
  Aşım durumunda WARNING loglanmalı — güvenlik döngüsünün gecikmesi ciddi bir göstergedir.
- Otomasyon çağrısı için **yer ayrılmalı** ancak TASK-057'ye kadar boş kalmalı. Bu aşamada
  sistem yalnızca manuel komutlarla ve güvenlik kilitleriyle çalışır — ve bu **kasıtlıdır**:
  M4 kapısı, otomasyon eklenmeden önce güvenliğin kanıtlanmasını gerektirir.
- Watchdog beslemesi döngünün en sonunda; adım 1–6 tamamlanmadan beslenmez.
- Snapshot bir kez alınmalı ve tüm döngü boyunca aynı görüntü kullanılmalı; ortada yeniden
  okumak tutarsız karar üretir.

## Files

- `src/tasks/AppCoreTask.cpp` (yeni)
- `src/domain/AppCore.h` / `.cpp` (yeni — döngü mantığı, test edilebilir)

## Acceptance Criteria

- [ ] Döngü sırası §11.1 ile birebir aynı
- [ ] Güvenlik her zaman otomasyondan önce değerlendiriliyor
- [ ] Komut tüketme sınırı var; acil durdurma sınırdan muaf
- [ ] Döngüde bloklama yok
- [ ] Snapshot döngü başında bir kez alınıyor
- [ ] Döngü süresi ölçülüyor; aşımda WARNING
- [ ] Otomasyon çağrısı için yer ayrıldı, şimdilik boş
- [ ] Heartbeat ve watchdog doğru sırada
- [ ] Döngü mantığı host'ta test edilebilir

## Test Plan

- [ ] Döngü süresi ölçüldü; 100 ms'in belirgin altında
- [ ] Yük altında (web trafiği + sensör okuma) periyot korunuyor
- [ ] Komut seli altında döngü süresi patlamıyor
- [ ] Acil durdurma komutu yük altında bir döngü içinde işleniyor
- [ ] Güvenlik değerlendirmesinin otomasyondan önce çalıştığı doğrulandı
- [ ] Stack watermark ölçüldü
- [ ] 24 saatlik çalışmada heartbeat kesintisiz

## Review Checklist

- [ ] Architecture'a uygun mu? (§11.1, §6.1)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **döngüde kesinlikle olmamalı**
- [ ] Shared state güvenli mi? — tek snapshot kullanımı
- [ ] Memory problemi var mı? — stack, snapshot boyutu
- [ ] Error handling var mı? — döngü aşımı raporlanıyor mu
- [ ] ESP32 resource kullanımı uygun mu? — çekirdek ve öncelik doğru mu
- [ ] Task sorumluluğu doğru mu? — **bu task'ın özü**
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — eski task yapısı ve öncelikleri taşınmamalı

## Definition of Done

Ortak DoD + döngü süresi yük altında ölçüldü + **M4 kilometre taşı doğrulaması:
pompa yalnızca güvenlik izniyle çalışıyor**.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Komut tüketme sınırı: (b) döngü başına en fazla N

```text
Selected: N = 4, acil durdurma SINIRDAN MUAF.
Reddedilen: (a) kuyrugu tamamen bosalt → 16 komutluk bir sel, guvenlik
            dongusunun periyodunu bozar. Guvenlik dongusunun gecikmesi
            komut gecikmesinden ciddidir.
Reddedilen: (c) sure butcesi → olcum maliyeti ve ongorulemezlik.

N=4 gerekcesi: kuyruk 16 slot; 4 komut/dongu × 10 dongu/sn = 40 komut/sn.
Bir operatorun uretebilecegi hizin cok ustunde. Kuyruk dolarsa `post()`
zaten BUSY dondurur ve cagiran bilgilendirilir — komut sessizce kaybolmaz.
```

## Karar 2 — Acil durdurma döngünün EN BAŞINDA, snapshot'tan bile önce

```text
Sira: takeEmergencyStop() → (varsa) emergency::trigger() → sonra normal dongu.

Gerekce: Acil durdurma, o donguye ait bir snapshot'a veya guvenlik
         degerlendirmesine IHTIYAC DUYMAZ. "Her seyi kapat" kararinin girdisi
         yoktur. Snapshot'i beklemek, kapatmayi bir kac yuz mikrosaniye
         geciktirmekten baska bir sey yapmaz.
```

## Karar 3 — Otomasyon için yer ayrıldı, BOŞ bırakıldı

ARCHITECTURE §11.1 adım 4. Bu aşamada sistem **yalnızca** manuel komutlarla
ve güvenlik kilitleriyle çalışır. Bu **kasıtlıdır**: M4 kapısı, otomasyon
eklenmeden önce güvenliğin kanıtlanmasını gerektirir. P7 gereği boş bir
`AutomationEngine` sınıfı **yazılmadı** — yalnızca çağrı yeri yorumla
işaretlendi.

## Karar 4 — Döngü mantığı `domain/AppCore`, task sarmalayıcı ince

```text
`tasks/AppCoreTask.cpp`   → yalnizca TaskRunner + millis() + sonsuz dongu
`domain/AppCore.cpp`      → TUM karar mantigi, `now` parametreli

Kazanc: TASK-064 tum donguyu FreeRTOS olmadan, sahte zamanla kosturabilir.
        Guvenlik dongusunun test edilebilir olmasi pazarliksizdir.
```

## Karar 5 — Snapshot bir kez, döngü boyunca aynı görüntü

312 bayt yığında (stack) tutulur; `STACK_APP_CORE = 4096` bayt için sorun
değil. Ortada yeniden okumak, güvenliğin bir görüntüye, aktüatörün başka bir
görüntüye göre karar vermesi demektir — **tutarsız karar** üretir.

## Karar 6 — Döngü aşımı WARNING, ama log seli yok

`TaskRunner` zaten aşım sayıyor. `AppCore` ek olarak kendi iş süresini ölçer
ve **eşiği ilk aştığında** loglar; her döngüde loglamaz.

## Karar 7 — Bilinen istisna: NVS yazması döngü içinde

`emergency::trigger()` mandalı NVS'e yazar (~10–20 ms bloklama). "Döngüde
bloklama yok" kuralının bilinçli ve **tek** istisnası budur:

- yalnızca acil duruma **geçiş anında** olur, döngü başına değil
- yazma sırasında röleler **zaten güvenli**
- `app_core` yumuşak son tarihi 300 ms; 20 ms içinde kalır

Alternatif (`store` task'ına kuyruklamak) mandalın kalıcılığını başka bir
task'ın sağlığına bağımlı kılardı — acil durumda kabul edilemez.

---

# STEP 3 — REVIEW RECORD

- [x] Döngü sırası §11.1 ile birebir aynı (adım 0 eklendi: acil durdurma
      snapshot'tan önce — §11.1'i genişletir, ihlal etmez)
- [x] Güvenlik **her zaman** otomasyondan önce — otomasyon yeri adım 3'ün
      **altında** ve şu an boş
- [x] Komut tüketme sınırı var (`MAX_COMMANDS_PER_CYCLE = 4`); acil durdurma
      sınırdan muaf ve kuyruğu tamamen atlıyor
- [x] Snapshot döngü başında **bir kez** alınıyor, tüm döngü aynı görüntüyü
      kullanıyor
- [x] Bayat komutlar (>3 sn) atılıyor ve kaydediliyor
- [x] Döngü süresi `esp_timer_get_time()` ile ölçülüyor; bütçe aşımında
      WARNING (**bir kez** — log seli yok)
- [x] Otomasyon çağrısı için yer ayrıldı; P7 gereği **boş bir motor sınıfı
      yazılmadı**
- [x] Heartbeat ve watchdog `TaskRunner` tarafından doğru sırada
- [x] Döngü mantığı `domain/AppCore` içinde, FreeRTOS'suz, `now` parametreli
      → host'ta test edilebilir
- [x] Röleye giden tek kapı korundu — tarama: `relay::set|allSafe` yalnızca
      `ActuatorManager.cpp:52` ve `:275`
- [x] `domain/` → `interfaces/|tasks/` **0 ihlal**
- [x] `domain/` içinde heap tahsisi **0**
- [x] Derleme temiz (`-Wall -Wextra`)
- [ ] **Döngü süresi, yük altı davranışı, stack watermark — donanım gerekiyor**

## Güvenlik zincirinin bağlandığı yer

`appcore::begin()` bu batch'in tüm parçalarını birleştiriyor:

```text
safety::begin()      → kilit hesaplayici
flow::begin()        → kuru calisma korumasi
emergency::begin()   → kalici mandali NVS'ten geri okur
actuators::begin(cfg, &safety::permits)   ◀── VETO BAGLANTISI
supervisor::setSafeStateHandler(&onSafeState)  ◀── TASK-012 baglantisi
```

`actuators::begin()` `permit == nullptr` ise reddediyor: **güvenlik izni
bağlanmadan aktüatör yöneticisi başlatılamaz.** Bu, unutulması mümkün
olmayan bir bağlantı.

## Bilinen istisna: NVS yazması döngü içinde (3 nokta)

Tarama üç NVS çağrısı buldu:

| Yer | Ne zaman | Değerlendirme |
|---|---|---|
| `emergency::begin()` boot sayacı | task başlangıcı, döngü dışı | sorun yok |
| `emergency::trigger()` mandal yazma | acil duruma **geçiş anında** | kabul edildi |
| `emergency::clear()` anahtar silme | operatör onayı anında | kabul edildi |

Üçü de tek seferlik geçişlerde, döngü başına değil. Yazma sırasında röleler
zaten güvenli. `app_core` yumuşak son tarihi 300 ms, işlem ~10–20 ms.

Alternatif — `store` task'ına kuyruklamak — mandalın kalıcılığını başka bir
task'ın sağlığına bağımlı kılardı. Acil durumda kabul edilemez.

## Bayat komut döngüsü hakkında bir not

`while (n < 4 && receive(...))` içinde bayat komut `continue` ile atlanıyor;
yani bir döngüde 4'ten fazla komut **alınabilir** (hepsi bayatsa 16'ya
kadar). Bu bilinçli: bayat komutlar iş üretmez, yalnızca kuyruktan
temizlenir. Üst sınır kuyruk boyutu (16) ile zaten sabittir.

## M4 kilometre taşı — durum

| Gereklilik | Durum |
|---|---|
| Röleye tek kapıdan erişim | ✅ tarama ile kanıtlandı |
| Güvenlik vetosu her açma yolunda | ✅ yapısal (`permit == nullptr` reddedilir) |
| Çalışan aktüatörün izlenmesi | ✅ `apply()` adım 3 |
| Kuru çalışma koruması | ✅ mandallı |
| Acil durum mandalı, kalıcı | ✅ NVS |
| Operatör onaylı kurtarma | ✅ tek nokta, canlı koşul kontrollü |
| **"Pompa yalnızca güvenlik izniyle çalışıyor" — DONANIMDA** | ❌ **doğrulanmadı** |

**M4 KAPANMADI.** Yazılım tarafı hazır; kapının açılması donanım
doğrulamasına bağlı. IMPLEMENTATION_PLAN'e göre "M4 doğrulanmadan PHASE 12
(otomasyon) başlatılmaz" — bu kısıt geçerliliğini koruyor.

**TASK-033: TAMAMLANDI** (donanım doğrulaması bekliyor).
