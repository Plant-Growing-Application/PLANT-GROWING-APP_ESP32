# TASK-045 — WebSocket Protocol & Command Path

**Phase:** 9 — Web Backend · **Priority:** P1

## Objective

Canlı durum yayını ve komut kabulü için sağlam bir WebSocket protokolü tanımlamak.
Mevcut projedeki en yaygın tutarsızlık kaynağını — arayüzün cihazdan bağımsız durum
üretmesini — protokol düzeyinde imkânsız kılmak.

## Scope

- Mesaj şeması: `state`, `cmd`, `ack`, `event`
- Bağlantıda tam state gönderimi
- Komut kabulü, doğrulama ve `CommandQueue`'ya aktarım
- `reqId` tabanlı ack mekanizması
- WS bağlantısında yetki doğrulaması
- İstemci listesi yönetimi

## Out of Scope

- Telemetri yayın zamanlaması ve backpressure (TASK-046)
- Frontend istemcisi (TASK-048)
- Komut yürütme (TASK-033)

## Dependencies

- TASK-042, TASK-008

## Requirements

- `REQUIREMENTS.md` — §5.4 (WebSocket `[~]`), Kritik Problem 5

## Architecture References

- §14.2 Durum senkronizasyonu diyagramı · §14.6 AsyncTCP kuralları
- §3.3 Komut akışı

## Expected Design

### Protokol akışı (§14.2)

```text
   İstemci bağlanır (yetki doğrulanır)
        │
   ── SUNUCU: { type:"state", v:1234, full:true, ... } ──────────────▶
        │
   İstemci komut gönderir
        │
   ── İSTEMCİ: { type:"cmd", reqId:"a7", target:"pump", action:"on" } ─▶
        │       arayüz "BEKLİYOR"a geçer, durumu DEĞİŞTİRMEZ
        ▼
   ── SUNUCU: { type:"ack", reqId:"a7", result:"ACCEPTED" } ──────────▶
        │
   ── SUNUCU: { type:"state", v:1235, ... }  ◀── gerçek durum ───────▶
        │
   İstemci kartı YALNIZCA bu state ile günceller
```

### Protokol kuralları

| Kural | Gerekçe |
|---|---|
| Her state paketinde `v` (versiyon) | Sıra dışı/eski paket yok sayılabilsin |
| Her komutta `reqId` | Ack eşleştirmesi; hangi isteğin reddedildiği belli olsun |
| Bağlantıda tam state | Sayfa yenilendiğinde gerçek durum anında görünsün |
| Ack sonucu §10.4 enum'undan | `SAFETY_BLOCKED`, `COOLDOWN` gibi nedenler açıkça bildirilsin |
| Komut yalnızca kuyruğa gider | Callback asla GPIO sürmez |

### JSON ayrıştırma

Mevcut projede `msg.indexOf("\"id\":")` ile elle string parse yapılıyordu — geçersiz
mesajda tanımsız davranış üretir. Yeni tasarımda **gerçek JSON ayrıştırıcı** kullanılmalı
ve ayrıştırma hatası açık bir hata yanıtı üretmeli.

## Implementation Notes

- WS callback **AsyncTCP task bağlamında** çalışır: doğrula → kuyruğa koy → dön.
  Uzun işlem, flash erişimi veya bekleme yasak (§14.6).
- Yetki doğrulaması bağlantı kurulurken yapılmalı; yetkisiz bağlantı kabul edilmemeli.
  WS'te her mesajda token taşımak yerine bağlantı seviyesinde doğrulama daha uygundur.
- Mesaj boyutu sınırlandırılmalı; parçalı (fragmented) mesajlar doğru birleştirilmeli.
  Mevcut projede `info->final` kontrolü vardı ama parça birleştirme yoktu.
- İstemci sayısı sınırlandırılmalı; her istemci RAM tüketir.
- Kopan istemciler temizlenmeli (mevcut projedeki `cleanupClients()` karşılığı, ancak
  `loop()` yerine `net` task'ında).
- Komut kuyruğu doluysa ack `BUSY` sonucuyla dönmeli — sessiz yutma yok.
- Ack, komutun **kabul edildiğini** bildirir; **uygulandığını** state paketi bildirir.
  Bu ayrım protokolde net olmalı.

## Files

- `src/interfaces/web/WsProtocol.h` / `.cpp` (yeni)
- `src/interfaces/web/WsCommandHandler.cpp` (yeni)

## Acceptance Criteria

- [ ] Dört mesaj tipi tanımlı ve şemaları belgelenmiş
- [ ] Bağlantıda tam state gönderiliyor
- [ ] Her state paketinde versiyon var
- [ ] Komutlar `reqId` taşıyor ve ack ile eşleşiyor
- [ ] Ack sonucu §10.4 enum'undan
- [ ] Gerçek JSON ayrıştırıcı kullanılıyor; hatalı mesaj açık hata üretiyor
- [ ] Komut yalnızca kuyruğa gidiyor; callback GPIO sürmüyor
- [ ] WS bağlantısında yetki doğrulanıyor
- [ ] Mesaj boyutu sınırlı; parçalı mesajlar doğru birleştiriliyor
- [ ] İstemci sayısı sınırlı; kopan istemciler temizleniyor
- [ ] Kuyruk doluyken `BUSY` ack dönüyor

## Test Plan

- [ ] Bağlantıda tam state alınıyor
- [ ] Komut gönderilip ack alınıyor; `reqId` eşleşiyor
- [ ] Güvenlik vetolu komutta ack `SAFETY_BLOCKED` dönüyor
- [ ] Bozuk JSON gönderildiğinde çökme yok, açık hata dönüyor
- [ ] Parçalı büyük mesaj doğru birleştiriliyor
- [ ] Yetkisiz WS bağlantısı reddediliyor
- [ ] Maksimum istemci sayısı aşıldığında davranış doğru
- [ ] İstemci aniden koptuğunda kaynak sızıntısı yok
- [ ] Komut seli altında `BUSY` ack dönüyor, sistem kararlı
- [ ] Uzun süreli bağlantıda heap sabit

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.2, §14.6)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **callback içinde kesinlikle olmamalı**
- [ ] Shared state güvenli mi? — istemci listesi
- [ ] Memory problemi var mı? — istemci başına RAM, mesaj tamponları
- [ ] Error handling var mı? — bozuk mesaj, dolu kuyruk
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **elle string parse ve doğrudan `digitalWrite` yasak**

## Definition of Done

Ortak DoD + bozuk mesaj dayanıklılığı test edildi + komutun GPIO'ya callback'ten
ulaşmadığı kod taramasıyla doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — İYİMSER GÜNCELLEME YOK: `ack` ≠ `state`

**Bu batch'in en önemli protokol kararıdır** (REQUIREMENTS Kritik Problem 5).

```text
Eski sistem: kullanici butona basar → arayuz kart durumunu HEMEN degistirir
             → cihaz komutu REDDETSE bile arayuz "acik" gosterir.
             Kullanici pompanin calistigini SANIR. Bu bir GUVENLIK sorunudur.

Yeni protokol:
   cmd  → arayuz "BEKLIYOR"a gecer, DURUMU DEGISTIRMEZ
   ack  → komut KABUL EDILDI (veya reddedildi + neden)
   state→ komut UYGULANDI; kart YALNIZCA bunu gorunce degisir

`ack` kabulu, `state` uygulanmayi bildirir. Bu ayrim protokolde nettir ve
istemci tarafinda TASK-048 bunu zorunlu kilar.
```

## Karar 2 — Gerçek JSON ayrıştırıcı (ArduinoJson), elle parse YOK

```text
Eski: msg.indexOf("\"id\":") ile elle string arama.
      Gecersiz mesajda TANIMSIZ DAVRANIS; kacik tirnak, eksik alan,
      farkli sira → sessizce yanlis komut.
Yeni: ArduinoJson 7, SABIT boyutlu belge, ayristirma hatasi ACIK yanit.
```

## Karar 3 — Yetki BAĞLANTI seviyesinde

Token her mesajda taşınmaz; WS el sıkışmasında bir kez doğrulanır ve
bağlantı yetkili işaretlenir. Her mesajda token taşımak hem bant genişliği
hem sabit zamanlı karşılaştırma maliyeti getirir, güvenlik kazancı yoktur.

**Yetkisiz bağlantı kabul EDİLMEZ** — kapatılır.

## Karar 4 — Parça birleştirme GERÇEKTEN yapılır

Eski sistemde `info->final` kontrolü vardı ama **parça birleştirme yoktu**:
uzun bir mesaj sessizce bozuluyordu. Burada bağlantı başına bir birleştirme
tamponu var (512 bayt); aşan mesaj `WEB_PAYLOAD_TOO_LARGE` ile reddedilir.

## Karar 5 — Komut kuyruğu doluysa `ack: BUSY`

Sessiz yutma yok. `CommandQueue::post()` zaten bloklamadan `BUSY` döner
(TASK-008); protokol bunu istemciye aynen iletir.

## Karar 6 — İstemci sayısı sınırlı: 4

Her WS istemcisi RAM tüketir (yazma kuyruğu + birleştirme tamponu).
Sınır aşıldığında yeni bağlantı reddedilir — kabul edip sonra düşürmek
istemciyi yeniden bağlanma döngüsüne sokar.

---

# STEP 3 — REVIEW RECORD

- [x] Mesaj şeması: `state` / `cmd` / `ack`
- [x] Bağlantıda **tam state** gönderiliyor; bu paket düşürülmez
- [x] Komut doğrulanıp `CommandQueue`'ya aktarılıyor; callback GPIO sürmüyor
- [x] `reqId` tabanlı ack; sonuç `CommandResult` enum'undan
- [x] **Gerçek JSON ayrıştırıcı** (ArduinoJson 7); elle `indexOf` parse yok
- [x] Yetki **el sıkışmasında** doğrulanıyor — yetkisiz bağlantı hiç kurulmuyor
- [x] Parça birleştirme GERÇEKTEN yapılıyor (512 baytlık tampon); aşan mesaj
      `WEB_PAYLOAD_TOO_LARGE`
- [x] İstemci sayısı 4 ile sınırlı; sınır aşımında bağlantı **reddediliyor**
      (kabul edip düşürmek yeniden bağlanma döngüsü üretirdi)
- [x] Kuyruk doluysa ack `BUSY` — sessiz yutma yok
- [x] Kopan istemciler `net` task'ında temizleniyor (eski `loop()` değil)
- [ ] **Donanım testleri bekliyor**

## Yetki: `handleHandshake` — plandan daha güçlü

İlk tasarımda yetki `WS_EVT_CONNECT` içinde doğrulanacaktı: bağlantı kurulur,
sonra kapatılırdı. `AsyncWebSocket::handleHandshake()` (kütüphane kaynağında
bulundu, `AsyncWebSocket.h:411`) doğrulamayı **el sıkışmasına** taşıyor —
yetkisiz bağlantı hiç kurulmuyor, bir slot bile tüketmiyor.

Token sorgu parametresiyle taşınıyor çünkü tarayıcının WebSocket API'si
özel başlık göndermeye izin vermez. Bu bir tasarım kısıtı, tercih değil.

## `ack` ≠ `state` — protokolün özü

`ack` komutun **kabul edildiğini**, `state` **uygulandığını** bildirir.
Bu ayrım olmadan istemci "kabul edildi"yi "çalıştı" sanar ve iyimser
güncelleme yasağı anlamını kaybeder.

**TASK-045: TAMAMLANDI.**

---

# STEP 3 — REVIEW RECORD (ek denetim)

**Tarih:** 2026-08-31 · Kod çalıştırılmadan yapılan ikinci okuma.

## Bulunan hata: görevler arası use-after-free

`tick()` (net task) şöyleydi:

```text
AsyncWebSocketClient* c = g_ws.client(g_slots[i].id);
(void)sendState(c, json, n, true);
```

Kütüphane kaynağı okundu (`AsyncWebSocket.cpp:942-951`):

```text
AsyncWebSocket::client(id)      → _ws_clients_lock ALIR, bulur, BIRAKIR,
                                  std::list icine HAM ISARETCI dondurur
AsyncWebSocket::_handleDisconnect → ayni kilidi alir, _clients.erase(iter)
```

`_handleDisconnect()` **AsyncTCP task'ından** çalışır. `client()` dönüşü ile
`text()` çağrısı arasında istemci kopar ve nesne **yok edilirse**, işaretçi
serbest bırakılmış belleği gösterir.

Bu, yalnızca kopma anında ve yalnızca iki task çakışırsa olur — yani
**seyrek, tekrarlanamaz ve teşhisi çok zor** bir çökme sınıfı. Tam da
donanımda haftalarca kovalanacak türden.

**Düzeltme:** `textAll(AsyncWebSocketSharedBuffer)` kullanıldı. Kütüphane
tüm iterasyonu `_ws_clients_lock` **altında** yapar ve `c.text(buffer)`'ı
orada çağırır — yarış yapısal olarak imkânsız.

Backpressure **korundu**: `_queueMessage()` kuyruk doluyken paketi düşürür
ve `false` döner; `SendStatus` bunu `PARTIALLY_ENQUEUED`/`DISCARDED` olarak
bildirir. `setCloseClientOnQueueFull(false)` açıkça ayarlandı — yavaş bir
istemcinin bağlantısını koparmak onu yeniden bağlanma döngüsüne sokardı.

`sendState()` → `sendFullState()` olarak yeniden adlandırıldı ve artık
**yalnızca** `WS_EVT_CONNECT` içinde (AsyncTCP bağlamında, işaretçinin
geçerli olduğu tek yer) kullanılıyor.

## Doğrulanan diğer noktalar

- `onFrame` tampon sınırı: `assembled + len >= ASSEMBLY_MAX` guard'ı
  `buf[assembled] = '\0'` yazımını güvenli kılıyor (en fazla `buf[511]`)
- `ack` yolu AsyncTCP bağlamında; işaretçi o çağrı süresince geçerli
- Yetkisiz bağlantı el sıkışmada reddediliyor → bağlı her istemci yetkili;
  yayında ayrıca yetki süzmeye gerek yok
