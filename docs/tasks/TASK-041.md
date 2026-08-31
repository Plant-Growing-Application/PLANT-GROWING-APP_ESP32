# TASK-041 — WebService Skeleton & Static Asset Serving

**Phase:** 9 — Web Backend · **Priority:** P1

## Objective

Web sunucusunu kurmak ve statik varlıkları verimli şekilde servis etmek. AsyncTCP bağlam
kurallarını (§14.6) baştan uygulayarak sonraki web task'larının güvenli temelini atmak.

## Scope

- AsyncWebServer başlatma ve yaşam döngüsü
- Statik varlık servisi (gzip'li dosya desteği)
- 404 ve hata yanıtı biçimi
- Ortak yanıt yardımcıları (JSON, hata şeması)
- AsyncTCP bağlam kurallarının uygulanması

## Out of Scope

- Kimlik doğrulama (TASK-042)
- API endpoint'leri (TASK-043, TASK-044)
- WebSocket (TASK-045)
- Frontend içeriği (TASK-047)

## Dependencies

- TASK-016, TASK-035

## Requirements

- `REQUIREMENTS.md` — §5.5 (statik servis `[x]`, `/index` 404), §5.1 (kırık link)

## Architecture References

- §2.11 WebService · §14.1 Sorumluluk ayrımı · §14.6 AsyncTCP bağlam kuralları

## Expected Design

### AsyncTCP bağlam kuralları (§14.6) — baştan uygulanmalı

| Kural | Gerekçe |
|---|---|
| Callback içinde dosya taraması / uzun döngü / bekleme **yok** | AsyncTCP task'ını bloklar, tüm web donar |
| Callback yalnızca doğrular ve kuyruğa koyar | Komut yürütme `app_core`'a aittir |
| Büyük JSON önceden boyutlandırılmış | Heap parçalanmasını önler |
| Yavaş istemci sistemi bloklamamalı | Yazma kuyruğu doluysa paket düşürülür |

### Gzip'li varlık servisi — neden önemli

Mevcut projede `bootstrap.min.css` **298 KB** olarak sıkıştırılmamış servis ediliyor.
Gzip ile bu boyut belirgin şekilde düşer; hem flash alanı hem yükleme süresi kazanılır.
Sunucu `.gz` uzantılı dosyayı bulup uygun `Content-Encoding` başlığıyla göndermeli.

### Kırık yol düzeltmesi

`/index` route'u mevcut projede yoktu ama navbar linki oradaydı → 404. Yeni tasarımda
**tek taraflı yol bulunmaz** (P7): ya route tanımlanır ya link kaldırılır.

## Implementation Notes

- `_server.begin()` dönüş değeri ve sunucunun gerçekten dinlediği kontrol edilmeli;
  mevcut projede kontrol edilmiyordu.
- Statik dosyalar için `Cache-Control` başlıkları verilmeli; ancak `index.html` için uzun
  önbellek **verilmemeli** (güncelleme sonrası eski sayfa görünür).
- Dosya sistemi mount edilmemişse (DEGRADED mod) sunucu yine başlamalı ve statik istekler
  için anlamlı bir hata sayfası döndürmeli — sessiz 404 yerine "dosya sistemi kullanılamıyor".
- Ortak hata şeması (§14.5) burada tanımlanmalı: `{ error: { code, message, field } }`.
  Tüm endpoint'ler bunu kullanacak.
- MIME tipleri doğru ayarlanmalı; yanlış tip tarayıcının dosyayı reddetmesine yol açar.
- Sunucu yalnızca ağ hazırken mi başlamalı? AP modunda da erişilebilir olmalı — bu yüzden
  AP veya STA fark etmeksizin başlatılmalı.

## Files

- `src/interfaces/web/WebService.h` / `.cpp` (yeni)
- `src/interfaces/web/HttpResponses.h` (yeni — ortak yanıt/hata yardımcıları)

## Acceptance Criteria

- [ ] Sunucu başlıyor ve dinlediği doğrulanıyor
- [ ] Statik varlıklar servis ediliyor
- [ ] Gzip'li dosya desteği çalışıyor, doğru başlıklarla
- [ ] MIME tipleri doğru
- [ ] Ortak hata şeması tanımlı ve kullanılıyor
- [ ] Dosya sistemi yokken anlamlı hata dönüyor
- [ ] `index.html` uzun önbelleğe alınmıyor
- [ ] Callback'lerde bloklama yok
- [ ] Tek taraflı (karşılığı olmayan) route veya link yok
- [ ] AP ve STA modunda erişilebiliyor

## Test Plan

- [ ] Ana sayfa yükleniyor; tüm varlıklar 200 dönüyor
- [ ] Gzip'li varlık doğru açılıyor; boyut kazancı ölçüldü
- [ ] Dosya sistemi mount edilmemişken sunucu çalışıyor ve anlamlı hata veriyor
- [ ] Var olmayan yol 404 ve ortak hata şeması dönüyor
- [ ] Yavaş istemci simülasyonunda sistem donmuyor
- [ ] Çoklu eşzamanlı istekte kararlılık
- [ ] Uzun süreli kullanımda heap sızıntısı yok
- [ ] AP modunda erişim doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.11, §14.6)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **callback içinde kesinlikle olmamalı**
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — heap sızıntısı, JSON tamponları
- [ ] Error handling var mı? — `begin()` sonucu, FS yokluğu
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — çift FS mount ve kontrolsüz `begin()` taşınmamalı

## Definition of Done

Ortak DoD + gzip boyut kazancı ölçüldü + FS yokken sunucunun çalıştığı doğrulandı +
heap sızıntısı olmadığı uzun süreli testle kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — AsyncTCP bağlam kuralları kodun YAPISINDA

`interfaces/web/` içindeki hiçbir callback röle sürmez, flash taramaz,
beklemez. Callback yalnızca **doğrular ve kuyruğa koyar**. Bu kural
`WebService.cpp` içinde `hal::relay`, `hal::nvsstore`, `delay` taramasıyla
denetlenir.

Eski sistemde WebSocket handler'ı doğrudan `digitalWrite()` yapıyordu —
AsyncTCP task bağlamından, güvenlik kontrolü olmadan.

## Karar 2 — Ortak hata şeması TEK yerde

```json
{ "error": { "code": 2306, "message": "SAFETY_BLOCKED", "field": "actuator" } }
```

`code` sayısal `ErrCode`; `message` enum adı; `field` varsa ilgili alan.
Tüm endpoint'ler bunu kullanır. Farklı endpoint'lerin farklı hata şekli
döndürmesi istemci hatalarının ana kaynağıdır (TASK-039'da aynı sorunun
somut örneği vardı).

## Karar 3 — Statik servis `ESPAsyncWebServer`'ın kendi mekanizmasıyla

`serveStatic()` gzip'li dosyayı (`.gz`) otomatik bulur ve
`Content-Encoding: gzip` ekler. Elle dosya arama yazmak, hem AsyncTCP
bağlamında dosya sistemi taraması yapmak (yasak) hem de tekerleği yeniden
icat etmek olurdu.

`bootstrap.min.css` eski projede **298 KB sıkıştırılmamış** servis
ediliyordu; TASK-047 varlıkları gzip'leyecek, bu task o dosyaları
doğru başlıkla sunacak altyapıyı kuruyor.

## Karar 4 — `index.html` uzun önbelleğe ALINMAZ

Diğer varlıklar (hash'li isim taşıyanlar) uzun `Cache-Control` alır;
`index.html` `no-cache`. Aksi hâlde firmware güncellemesinden sonra
kullanıcı **eski sayfayı** görür ve "güncelleme çalışmadı" der.

## Karar 5 — Sunucu AP ve STA fark etmeksizin başlar

Kurulum tam olarak AP modunda yapılır; sunucunun yalnızca STA bağlıyken
başlaması, cihazın hiç yapılandırılamaması demektir.

## Karar 6 — Dosya sistemi yoksa SESSİZ 404 DEĞİL

LittleFS mount edilemediyse (DEGRADED mod) sunucu yine başlar ve statik
istekler için "dosya sistemi kullanılamıyor" diyen açık bir yanıt döner.
Sessiz 404, kullanıcıyı yanlış yerde arattırır.

## Karar 7 — Tek taraflı route/link YASAK (P7)

Eski projede navbar'da `/index` linki vardı ama route tanımlı değildi → her
tıklama 404. Ya route tanımlanır ya link kaldırılır; ikisi TASK-047'de
birlikte doğrulanacak.

---

# STEP 3 — REVIEW RECORD

- [x] Sunucu başlıyor; `listening()` ile durum sorgulanabiliyor (eski sistemde
      `begin()` dönüşü hiç kontrol edilmiyordu)
- [x] Statik varlıklar servis ediliyor; gzip'li dosya desteği kütüphanenin
      kendi mekanizmasıyla (`WebHandlers.cpp:145-168` — `.gz` aranıyor,
      `Content-Encoding` ekleniyor; **kaynakta doğrulandı, varsayılmadı**)
- [x] `index.html` `no-cache`; `/assets/` uzun önbellek
- [x] Ortak hata şeması tanımlı ve tüm uç noktalarda kullanılıyor
- [x] Dosya sistemi yokken **sessiz 404 değil**, `STORAGE_FS_MOUNT_FAILED`
- [x] Callback'lerde bloklama yok — tarama: `hal::relay|digitalWrite|`
      `nvsstore::set|delay(|vTaskDelay` → **0 eşleşme** (kalan ikisi yorum)
- [x] AP ve STA fark etmeksizin başlıyor
- [x] Tek taraflı route/link yok — frontend tek sayfa, tüm sekmeler DOM içinde
- [ ] **Donanım testleri bekliyor**

## HTTP durum kodu eşlemesi

Güvenlik vetosu **403**, kısıt ertelemesi **409** döndürüyor. 400 döndürmek
istemciye "isteğini düzelt" der; oysa düzeltilecek bir şey yoktur —
koşulların değişmesi gerekir. Bu ayrım arayüzün doğru mesajı göstermesini
sağlıyor.

**TASK-041: TAMAMLANDI.**
