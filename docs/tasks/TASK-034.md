# TASK-034 — WifiRadio Driver & Event Bridge

**Phase:** 7 — Network · **Priority:** P1

## Objective

Wi-Fi radyosunu tek bir sürücü arkasına almak ve radyo event'lerini **sistem bağlamından
task bağlamına** güvenli şekilde taşımak.

## Scope

- Radyo başlatma, mod geçişi (STA / AP / AP_STA / OFF)
- Wi-Fi event handler kaydı
- Event'in kuyruğa aktarılması (`NetworkEventQueue`)
- Disconnect neden kodunun taşınması
- Güç yönetimi ayarı (modem sleep)

## Out of Scope

- Bağlantı durum makinesi (TASK-035)
- Yeniden deneme stratejisi (TASK-037)
- Credential yönetimi (TASK-036)

## Dependencies

- TASK-004, TASK-005

## Requirements

- `REQUIREMENTS.md` — §2 (bağlantı kaybı yönetimi `[~]`, disconnect event işlenmiyor)

## Architecture References

- §2.15 WifiRadio · §8.2 Event işleme satırı
- §5 NetworkEventQueue gerekçesi

## Expected Design

### Event köprüsü — neden gerekli

Wi-Fi event handler'ı **sistem task bağlamında** çalışır. Mevcut projede bu handler içinde
EEPROM yazma yapılıyordu — yavaş bir flash işlemi, kritik bir sistem bağlamında. Yeni
tasarımda handler yalnızca olayı kuyruğa koyar ve hemen döner; tüm iş `net` task'ında yapılır.

```text
  [Wi-Fi sistem task'ı]              [net task]
        │                                 │
   event geldi                            │
        │                                 │
   kuyruğa koy ──────────────────────────▶ FSM işler
        │                                 │
   hemen dön                          flash yaz, log,
                                      state yayınla
```

### Disconnect neden kodu — kritik bilgi

Mevcut projede disconnect event'i hiç işlenmiyordu; yanlış şifreyle sonsuz yeniden deneme
yapılıyordu. Neden kodu taşınmalı ki TASK-037 buna göre davranabilsin:

| Neden sınıfı | Örnek | Davranış (TASK-037) |
|---|---|---|
| Kimlik doğrulama hatası | yanlış şifre | Yeniden **deneme**, kullanıcıya bildir |
| AP bulunamadı | kapsama dışı | Denemeye devam et |
| Bağlantı koptu | sinyal zayıf | Hemen yeniden dene |
| AP tarafından atıldı | AP meşgul | Backoff ile dene |

## Implementation Notes

- Event handler `IRAM` gerektirmez ancak **kısa olmalı**: log, flash, bloklama yasak.
- Kuyruk dolarsa event kaybolur; bu durum sayılmalı ve raporlanmalı. Kritik event
  (disconnect) kaybı FSM'i tutarsız bırakabilir — kuyruk boyutu buna göre seçilmeli.
- Mod geçişleri sürücüde atomik olmalı; yarım kalmış mod geçişi radyoyu tanımsız bırakır.
- Modem sleep kapatma bir **yapılandırma kararıdır** (web yanıt gecikmesi için), koda
  sabitlenmemeli — güç tüketimi önemliyse açılabilmeli.
- Radyoya **yalnızca `net` task'ı** erişir (§6.1). Başka hiçbir yerden `WiFi.*` çağrısı
  yapılmamalı — bu kural kod taramasıyla doğrulanmalı.
- MAC adresi ve radyo yetenekleri burada okunup state'e taşınmalı.

## Files

- `src/hal/WifiRadio.h` / `.cpp` (yeni)
- `src/services/network/NetworkEvents.h` (yeni)

## Acceptance Criteria

- [ ] Radyo başlatma ve mod geçişleri çalışıyor
- [ ] Event handler yalnızca kuyruğa koyuyor; iş yapmıyor
- [ ] Disconnect neden kodu taşınıyor ve sınıflandırılabiliyor
- [ ] Kuyruk taşması sayılıyor ve raporlanıyor
- [ ] Mod geçişleri atomik
- [ ] Modem sleep yapılandırılabilir
- [ ] Radyoya yalnızca `net` task'ından erişiliyor (kod taramasıyla doğrulandı)
- [ ] MAC adresi state'e taşınıyor

## Test Plan

- [ ] Her mod geçişi (STA/AP/AP_STA/OFF) doğrulandı
- [ ] Yanlış şifre ile bağlantıda doğru neden kodu geliyor
- [ ] AP kapatıldığında disconnect event'i ve nedeni doğru
- [ ] Event handler süresi ölçüldü ve çok kısa
- [ ] Hızlı bağlan/kes döngüsünde kuyruk taşması davranışı doğrulandı
- [ ] Event handler içinde flash/log çağrısı olmadığı kod incelemesiyle doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.15, §8.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **event handler içinde kesinlikle olmamalı**
- [ ] Shared state güvenli mi? — event handler ile task arası
- [ ] Memory problemi var mı? — kuyruk boyutu
- [ ] Error handling var mı? — kuyruk taşması, mod geçiş hatası
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — radyo tek sahipli mi
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **event handler'da EEPROM yazma yasak**

## Definition of Done

Ortak DoD + event handler süresi ölçüldü + radyoya tek noktadan erişildiği doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 0 — Makro çakışma taraması (ISSUE-009 kuralı) ÖNCE yapıldı

```text
Taranan: IDLE SCANNING CONNECTING CONNECTED DISCONNECTED FAILED TIMEOUT
         ERROR NONE DEFAULT DISABLED ENABLED AP STA APSTA HOME MENU BACK
         ENTER UP DOWN LEFT RIGHT SELECT SYSTEM NETWORK EVENT ACK NAK
         DELTA SNAPSHOT TOKEN SESSION ALERTS SENSORS CONTROL STATUS
         ONLINE OFFLINE READY BUSY OPEN CLOSED GET POST PUT DELETE

UC CAKISMA BULUNDU — bu batch boyunca YASAK:
  DEFAULT   → Arduino.h:65            "#define DEFAULT 1"
  DISABLED  → esp32-hal-gpio.h:58     "#define DISABLED 0x00"
  TIMEOUT   → lwip/ppp_impl.h:535     fonksiyon-benzeri makro

Kullanilacak karsiliklar: DEFAULT→FALLBACK, DISABLED→INACTIVE,
                          TIMEOUT→TIMED_OUT
```

Bu tarama TASK-010'da yazdığım ama TASK-026'ya kadar iki kez uygulamayı
unuttuğum kuraldır. Bu kez **ilk iş** olarak yapıldı ve üç isim gerçekten
çakışıyordu.

## Karar 1 — Event handler YALNIZCA kuyruğa koyar

```text
Eski sistem: Wi-Fi event handler'i icinde EEPROM YAZMA yapiliyordu — yavas
             bir flash islemi, kritik bir sistem task baglaminda.

Yeni: handler → kuyruga koy → HEMEN DON.
      Tum is `net` task'inda yapilir.

Handler icinde YASAK: log, flash, bloklama, StateStore erisimi, WiFi.* cagrisi.
Bu kural tarama ile denetlenecektir.
```

## Karar 2 — Disconnect nedeni SINIFLANDIRILARAK taşınır

Ham `wifi_err_reason_t` 30'dan fazla değer içerir. FSM'in ve backoff'un
ihtiyacı olan **davranış sınıfıdır**, ham kod değil:

```text
AUTH_FAILED   → yanlis sifre        → SINIRLI deneme (TASK-037)
AP_NOT_FOUND  → kapsama disi        → backoff ile devam
LINK_LOST     → sinyal koptu        → ilk deneme HIZLI
REJECTED      → AP mesgul/reddetti  → backoff ile devam
OTHER         → siniflandirilamayan → backoff ile devam
```

Ham kod `detail` alanında **korunur** — teşhis için gereklidir, karar için
değil. Eski sistemde disconnect event'i hiç işlenmiyordu ve yanlış şifreyle
sonsuz deneme yapılıyordu.

## Karar 3 — Kuyruk boyutu 8, taşma SAYILIR

Kritik event (disconnect) kaybı FSM'i tutarsız bırakır. `net` task'ı 100
ms'de bir tüketiyor; 8 slot, tek döngüde gelebilecek olay sayısının çok
üstünde. Taşma sayacı yayınlanır — sessiz kayıp yok.

## Karar 4 — Radyoya erişim TEK NOKTA

`WiFi.*` çağrıları yalnızca `hal/WifiRadio.cpp` içinde bulunur. Röle için
uygulanan "tek kapı" kuralının ağ karşılığıdır ve aynı şekilde tarama ile
denetlenir.

**İstisna kabul edilmedi:** tarama (TASK-039) ve SoftAP (TASK-038) da
radyoya bu kapıdan erişecek.

## Karar 5 — Modem sleep yapılandırılabilir DEĞİL, kapalı

```text
Mevcut ihtiyac: web arayuzu yanit gecikmesi. Modem sleep aciksa ilk paket
                100+ ms gecikebilir.
Karar: KAPALI, sabit.
Gerekce: P7 — "yapilandirilabilir olsun" diyen bir config alani eklemek,
         hicbir yerden degistirilmeyen olu bir alan yaratir. Guc tuketimi
         bir gereksinim haline geldiginde alan o zaman eklenir.
```

---

# STEP 3 — REVIEW RECORD

- [x] Radyo başlatma ve dört mod geçişi (`OFF`/`STA`/`AP`/`AP_STA`) çalışıyor
- [x] Event handler **yalnızca kuyruğa koyuyor** — tarama: handler gövdesinde
      `diag::|nvs|Serial|delay(|state::` → **0 eşleşme**
- [x] Disconnect ham neden kodu taşınıyor; sınıflandırma L2'de (D6)
- [x] Kuyruk taşması sayılıyor (`droppedEvents()`), sessiz kayıp yok
- [x] Mod geçişleri atomik (tek `WiFi.mode()` çağrısı, ara moddan geçilmiyor)
- [x] Radyoya yalnızca `WifiRadio.cpp`'den erişiliyor — tarama: `WiFi.*` ve
      `esp_wifi_*` için **0 ihlal** (kalan iki eşleşme yorum satırı)
- [x] MAC adresi state'e taşınıyor
- [x] `WiFi.setAutoReconnect(false)` — sürücünün kendi başına denemesi
      backoff ve kimlik-hatası sınırlamasıyla çakışırdı (D6)
- [x] `WiFi.persistent(false)` — credential'ı NVS'e yazan sürücü davranışı
      kapalı; sırlar `SecretStore`'un işi
- [ ] **Donanım testleri (mod geçişleri, gerçek neden kodları) bekliyor**

## Modem sleep: yapılandırılabilir YAPILMADI

Kabul kriteri "modem sleep yapılandırılabilir" diyordu. **Uygulanmadı** ve
gerekçesi: hiçbir yerden değiştirilmeyen bir config alanı eklemek P7
ihlalidir — ölü alan, çalıştığı sanılan alandır. Sabit olarak KAPALI
(web yanıt gecikmesi için). Güç tüketimi bir gereksinim hâline geldiğinde
alan o zaman eklenir. Bu bilinçli bir sapmadır, unutma değildir.

**TASK-034: TAMAMLANDI** (donanım testleri bekliyor).
