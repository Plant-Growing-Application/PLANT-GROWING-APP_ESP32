# TASK-069 — Ürün Profili API'si

## Uç noktalar

```
GET  /api/crops         — katalog (ürünler, dönemler, hedef bantlar)
GET  /api/crop          — aktif seçim + o dönemin hedefleri
POST /api/crop/preview  — bir seçimin NE DEĞİŞTİRECEĞİ — HİÇBİR ŞEY YAZMAZ
PUT  /api/crop          — seçimi uygular
```

Hepsi `requireAuth()`'tan geçer; gövdeler `ConfigValidation` üzerinden
doğrulanır ve hata **alan adıyla** döner.

## Karar 1 — Katalog `/api/config` içinde değil

Katalog sabittir ve ~5 KB JSON tutar. Her config okumasında taşınması
gereksizdir; ayrıca `CONFIG_JSON_MAX` (2 KB) tamponunu tek başına aşardı.
Kurallar için verilen kararın (`writeRulesJson`) aynısı.

İstemci onu **bir kez** çeker ve saklar.

## Karar 2 — Önizleme ve uygulama AYNI yanıt şemasını döner

Arayüz onay ekranını ve sonuç ekranını tek bir kodla çizer; ikisi sessizce
ayrışamaz. `applied` alanı hangisinin olduğunu söyler.

Yanıt şu üçünü ayrı ayrı taşır:

| Alan | Neden ayrı |
|---|---|
| `ruleCount` | kaç kural yazılacak |
| `replacedCount` | kaç mevcut kuralın **üzerine** yazılacak |
| `automationMode` | kurallar etkin ama motor kapalı olabilir |

Son ikisi olmadan arayüz "3 kural yazılacak" der ve kullanıcı mevcut
kurallarının silineceğini fark etmez, ya da kuralları görüp çalıştığını sanar.

## Karar 3 — Tek paylaşılan JSON tamponu

Dört ayrı `static` tampon 12 KB `.bss` yiyordu ve bu doğrudan boş heap'ten
düşer — `LOW_HEAP_BYTES` (32 KB) kısılma eşiğine yaklaşmak istemiyoruz.

HTTP işleyicileri tek bağlamda (AsyncTCP) sırayla koşar ve yanıt aynı çağrı
içinde gönderilir; ikinci bir okuyucu yoktur. Bu, `ConfigApi.cpp`'deki
`ruleField()` gerekçesinin aynısıdır. Tek tampon: 6 KB, ölçülen RAM tasarrufu
6 KB (%25,3 → önceki %26,7).

İkinci fayda: katalog büyüdüğünde hangi yanıtın taşacağını aramak gerekmez.

## Karar 4 — Yanıt taşarsa uygulama BAŞARILI raporlanır

`PUT /api/crop` kuralları yazdıktan sonra yanıt tampona sığmazsa `sendOk()`
dönülür. İstemciye hata dönmek, **uygulanmış** bir değişikliği uygulanmamış
göstermek olurdu ve kullanıcı işlemi tekrarlardı.

## Karar 5 — Aktif ürün canlı state'e eklendi

`writeStateJson` artık kompakt bir `crop` nesnesi taşır: `{key, stage, day}`.
Hedef bantlar ve katalog **burada değil** — paket saniyede bir yayınlanıyor ve
değişmeyen 5 KB'lık katalogu her turda taşımak telemetri hızını düşürmekten
başka işe yaramazdı.

Panelin "Çilek · 34. gün · Meyve dönemi" satırı için bu üç alan yeterlidir ve
ayrı bir istek gerektirmez. İstemci `key`/`stage` değiştiğinde `/api/crop`'u
yeniden çeker — otomatik dönem ilerlemesinden sonra bayat bantlarla "iyi/kötü"
demesin diye.

`STATE_JSON_MAX` 2048 → 2560 yükseltildi: sensör sayısı 5→8, aktüatör 4→5 ve
ürün bilgisi eklendi (ölçülen tipik paket ~1,6 KB). 2 KB'da bırakılsaydı tam
dolu bir sistemde `serializeJson` taşar, `writeStateJson` 0 döner ve arayüzde
bu **"bağlantı koptu"** olarak görünürdü.

## Karar 6 — `plantedAt` metin değil sayı

Saat dilimi dönüşümü sunum katmanının (tarayıcı) işidir; cihazın yerel saatiyle
tarayıcınınki farklı olabilir. Epoch saniye taşınır.

## Dokunulan dosyalar

```
src/interfaces/web/api/CropApi.cpp   YENI
src/interfaces/web/ApiRoutes.h       registerCrop()
src/interfaces/web/api/NetworkApi.cpp  registerAll()
src/interfaces/web/StateJson.cpp     canli state'e crop
src/interfaces/web/WsProtocol.h      STATE_JSON_MAX 2048 → 2560
```

## Definition of Done

- [x] `pio run` temiz
- [x] Dört uç nokta sahte cihaz sunucusuyla tarayıcıda uçtan uca doğrulandı
- [x] Önizleme config'e dokunmuyor
- [x] Yanıt şemaları önizleme/uygulama arasında aynı
