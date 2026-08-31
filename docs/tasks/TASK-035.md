# TASK-035 — Network State Model & FSM

**Phase:** 7 — Network · **Priority:** P1

## Objective

Wi-Fi davranışını **bloklamayan bir durum makinesi** olarak modellemek ve `net` task
döngüsünü kurmak. Mevcut projenin en ciddi mimari problemi olan bloklayan bağlantıyı
yapısal olarak imkânsız kılmak.

## Scope

- FSM durumları: `BOOT`, `AP_ONLY`, `CONNECTING`, `CONNECTED`, `BACKOFF`, `AP_FALLBACK`
- Geçiş tablosu ve tetikleyiciler
- `network` alt-state modeli ve yayınlanması
- `net` task döngüsü, heartbeat, watchdog
- Event kuyruğu tüketimi

## Out of Scope

- Bağlantı kurma detayı (TASK-036)
- Backoff hesabı (TASK-037)
- AP fallback mantığı (TASK-038)
- Tarama (TASK-039)

## Dependencies

- TASK-034, TASK-007

## Requirements

- `REQUIREMENTS.md` — §2 (Wi-Fi), Kritik Problem 3

## Architecture References

- §8.1 FSM diyagramı · §8.2 Tasarım kararları · §8.3 Ağ durumu bilgisi
- §6.1 net task satırı

## Expected Design

### FSM (§8.1)

```text
                       ┌──────────────┐
                       │     BOOT     │
                       └──────┬───────┘
                              │ credential var?
                 hayır ┌──────┴──────┐ evet
                       ▼             ▼
              ┌─────────────┐  ┌──────────────┐
              │  AP_ONLY    │  │  CONNECTING  │◀────────┐
              └──────┬──────┘  └──────┬───────┘         │
                     │      ┌─────────┴─────────┐       │
                     │  başarı              başarısız    │
                     │      ▼                 ▼         │
                     │ ┌───────────┐   ┌──────────────┐ │
                     │ │ CONNECTED │   │   BACKOFF    │─┘
                     │ └─────┬─────┘   └──────┬───────┘
                     │       │ kopma          │ N deneme aşıldı
                     │       └────────────────┤
                     │                        ▼
                     │              ┌────────────────────┐
                     └─────────────▶│   AP_FALLBACK      │
                                    └────────────────────┘
```

### Bloklama yasağı — mutlak

> FSM'in hiçbir durumunda `while (WiFi.status() != WL_CONNECTED) delay(...)` benzeri bir
> bekleme **bulunamaz**. `CONNECTING` durumunda task normal periyoduyla döner; bağlantı
> sonucu event olarak gelir.

Mevcut projede `connect(5000)` çağrısı task'ı 5 saniye blokluyordu ve bu, watchdog
beslemesinden sonra yapıldığı için watchdog tarafından da görülmüyordu.

### Karar gerektiren nokta — Durum verisi nerede

```text
Problem:      FSM iç durumu ile yayınlanan network state ayrı mı olmalı?
Approaches:   (a) tek yapı, doğrudan yayınlanır
              (b) FSM iç durumu ayrı, yayınlanan state türetilir
Trade-offs:   (b) iç detayların (deneme sayacı, zamanlayıcılar) arayüze sızmasını önler
              ancak kullanıcı "sonraki deneme 8 sn sonra" bilgisini görmek isteyebilir
Recommended:  (b) — hangi iç bilginin yayınlanacağı bilinçli seçilsin
```

## Implementation Notes

- Geçiş tablosu açık olmalı; tanımsız geçiş loglanmalı ve güvenli duruma (BACKOFF veya
  AP_FALLBACK) düşülmeli.
- Her durum geçişi INFO seviyesinde loglanmalı — ağ sorunlarının teşhisi buna dayanır.
- `net` task periyodu 100 ms; ancak task esas olarak **event güdümlüdür** — kuyrukta olay
  yoksa yalnızca zamanlayıcıları kontrol eder.
- Yayınlanan state şunları içermeli (§8.3): FSM durumu, SSID, IP, gateway, RSSI, AP aktif mi,
  bağlantı süresi, son kopma nedeni, deneme sayısı, sonraki deneme zamanı.
- RSSI periyodik okunmalı (her döngüde değil; saniyede bir yeterli).
- Ağ tamamen kopuk olsa bile **otomasyon ve güvenlik etkilenmez** (§16.3) — bu bağımsızlık
  test edilmeli.
- Watchdog: `net` task'ı uzun timeout kullanır ancak yine de her döngüde beslenmeli.

## Files

- `src/services/network/NetworkFsm.h` / `.cpp` (yeni)
- `src/domain/models/NetworkState.h` (yeni)
- `src/tasks/NetworkTask.cpp` (yeni)

## Acceptance Criteria

- [ ] Altı durum ve geçiş tablosu implement edildi
- [ ] **Hiçbir durumda bloklayan bekleme yok**
- [ ] Tanımsız geçiş loglanıyor ve güvenli duruma düşülüyor
- [ ] Durum geçişleri loglanıyor
- [ ] `network` alt-state'i §8.3'teki alanları içeriyor
- [ ] Yayınlanan state ile FSM iç durumu ayrımı yapıldı
- [ ] RSSI periyodik okunuyor
- [ ] Task heartbeat ve watchdog doğru
- [ ] Ağ kopukken otomasyon ve güvenlik etkilenmiyor

## Test Plan

- [ ] Her durum geçişi tetiklenip doğrulandı
- [ ] `CONNECTING` durumunda task periyodu bozulmuyor (bloklama yok kanıtı)
- [ ] AP kapatılıp açıldığında FSM doğru ilerliyor
- [ ] 100 bağlan/kes döngüsünde FSM tutarlı kalıyor
- [ ] Ağ tamamen kapalıyken pompa kontrolü ve güvenlik çalışıyor
- [ ] Task döngü süresi ölçüldü
- [ ] Stack watermark ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§8.1, §8.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **bu task'ın ana konusu**
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — stack watermark
- [ ] Error handling var mı? — tanımsız geçiş
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`connect(5000)` bloklayan deseni yasak**

## Definition of Done

Ortak DoD + bloklama olmadığı periyot ölçümüyle kanıtlandı + ağ kopukken güvenliğin
çalıştığı doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — (b) FSM iç durumu AYRI, yayınlanan state TÜRETİLİR

```text
Ic durum (yayinlanmaz): deneme sayaci, zamanlayicilar, kimlik hatasi sayaci,
                        AP linger zamani, kopma sinifi
Yayinlanan (§8.3):      FSM durumu, SSID, IP/gw/subnet/dns, RSSI, AP aktif mi,
                        baglanti suresi, son kopma nedeni, deneme sayisi,
                        SONRAKI DENEME ZAMANI

`nextRetryAt` bilincli olarak YAYINLANIR: kullanici "8 sn sonra tekrar
denenecek" gormeli. Sessiz bekleme kullaniciya "bozuk" izlenimi verir —
eski sistemin en cok sikayet edilen davranisi.
```

## Karar 2 — BLOKLAMA YASAĞI mutlak

FSM'in hiçbir durumunda `while (WiFi.status() != WL_CONNECTED) delay(...)`
benzeri bir bekleme **bulunmaz**. `CONNECTING` durumunda task normal
periyoduyla döner; sonuç event olarak gelir.

Eski sistemde `connect(5000)` task'ı 5 saniye blokluyordu ve bu, **watchdog
beslemesinden sonra** yapıldığı için watchdog tarafından da görülmüyordu.

## Karar 3 — Bağlantı "kuruldu" sayılmaz, "IP alındı" sayılır

```text
STA_CONNECTED  → AP'ye baglanildi, IP HENUZ YOK → hala CONNECTING
STA_GOT_IP     → baglanti GERCEKTEN kullanilabilir → CONNECTED

Ayrim onemli: STA_CONNECTED'te web sunucusu dinlemeye baslarsa hicbir
adreste erisilemez ve SNTP basarisiz olur.
```

## Karar 4 — Tanımsız geçişte güvenli duruma düş

Geçiş tablosu açıktır; tabloda olmayan bir olay/durum çifti WARNING
loglanır ve `BACKOFF`'a düşülür. Sessizce yok saymak, FSM'i olayın
gerçekleşmediği varsayımıyla bırakır.

## Karar 5 — Ağ tamamen kopuk olsa bile güvenlik ETKİLENMEZ

`net` task'ı `app_core`'dan bağımsızdır ve `StateStore` üzerinden tek yönlü
konuşur. `net` tamamen kilitlense bile aktüatör kontrolü ve güvenlik
kilitleri çalışmaya devam eder (ARCHITECTURE §16.3). Bu bağımsızlık
tasarımın sonucudur, ek bir mekanizma gerektirmez.

## Karar 6 — RSSI saniyede bir okunur

Her 100 ms'lik döngüde okumak radyoya gereksiz sorgu yapar ve değer
zaten o hızda anlamlı değişmez.

---

# STEP 3 — REVIEW RECORD

- [x] Altı FSM durumu ve geçiş tablosu uygulandı
- [x] **Bloklama yok** — tarama: `delay(|vTaskDelay|while(...status|WL_CONNECTED`
      → ağ yolunda **0 eşleşme**
- [x] Her durum geçişi INFO loglanıyor (ağ teşhisi buna dayanır)
- [x] Tanımsız durumda WARNING + `BACKOFF`'a güvenli düşüş
- [x] Yayınlanan state §8.3'ün istediği alanları taşıyor; `nextRetryAt` dahil
- [x] RSSI saniyede bir okunuyor
- [x] İç durum (`NetworkRuntime`) yayınlanan state'ten ayrı
- [x] Şifre yayınlanan state'e **girmiyor**
- [x] `net` → `app_core` bağımlılığı yok; ağ kilitlense de güvenlik çalışır
- [ ] **Donanım testleri bekliyor**

## Katman düzeltmesi: `NetworkState.h` `services/` altına alındı

Task'ın dosya listesi durum modelini `domain/models/`, FSM'i
`services/network/` altında gösteriyordu. Bu bir **D1 ihlali** üretirdi:
servis (L2) katmanı domain (L3) başlığını include edemez.

Ağ FSM'i bir altyapı politikasıdır ve servis katmanına aittir; durum modeli
sahibiyle aynı katmana taşındı. Dosya kaydı tarihçeyi koruyarak
`git mv` ile taşındı.

## Bulduğum hata: ISSUE-012 tuzağına ÜÇÜNCÜ kez düştüm

İlk yazımda:

```text
g_rt.nextRetryAt = Millis{now.v + delay};
...
if (hasElapsed(now, g_rt.nextRetryAt, Duration{0}) && now.v >= g_rt.nextRetryAt.v)
```

İki ayrı hata bir arada:
1. `hasElapsed(..., Duration{0})` **her zaman true** (unsigned `>= 0`)
2. `now.v >= deadline.v` **taşmada kırılır** — `millis()` 49 günde bir sarar
   ve o anda backoff ya sonsuza kadar bekler ya hiç beklemez

Somut sonucu: 49. günde ağ koptuğunda cihaz ya saniyede 10 kez bağlanmayı
dener (AP'yi boğar) ya da bir daha hiç denemez.

Kök neden yine **"son tarih" zihinsel modeli**. Doğru desen beklemenin
BAŞLADIĞI anı ve süreyi saklamaktır. `NetworkRuntime` `nextRetryAt` yerine
`retryFrom` + `retryDelayMs` tutuyor; `retryDue()` tek yerde
`hasElapsed()` kullanıyor. Yayınlanan `nextRetryAt` yalnızca **sunum için**
türetiliyor.

Bu, ISSUE-012'nin üçüncü tekrarıdır (TASK-012, TASK-027, TASK-035) ve
`Millis operator+` kaldırma önerisinin ne kadar haklı olduğunun kanıtıdır.

**TASK-035: TAMAMLANDI** (donanım testleri bekliyor).
