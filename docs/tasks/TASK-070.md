# TASK-070 — Arayüz Sadeleştirme ve Kurulum Sihirbazı

## Problem

Arayüz altı sekmeyi **eşit ağırlıkta** gösteriyordu ve "Ayarlar" altında altı
dev bölüm vardı. Somut ölçüm (önceki sürüm):

| Dosya | Satır |
|---|---|
| `frontend/app.js` | 1663 (tek dosya) |
| `frontend/index.html` | 533 |
| `frontend/app.css` | 637 |

Karmaşıklığın kaynağı tasarım değil, **iç mimarinin arayüze sızmasıydı**:

- FreeRTOS stack watermark'ı "Panel" ile aynı seviyede duruyordu
- süreler **milisaniye** olarak giriliyordu (`900000` = 15 dk)
- `histerezis`, `flowVerifyDelayMs`, POSIX TZ dizesi doğrudan ekrandaydı
- kural düzenleyici, kullanıcının `onThreshold < offThreshold` ilişkisini
  kafadan çözmesini bekliyordu
- kurulumdan sonra kullanıcı MANUEL modda, **boş kural listesiyle** baş başa
  kalıyordu ve "şimdi ne yapacağım" sorusunun cevabı arayüzde yoktu

## Karar 1 — Silmek değil, KATMANLAMAK

Bu bölümlerin hepsi gerekli; hiçbiri ilk ekranda görülmesi gereken şey değil.

- **Basit mod (varsayılan):** `Bahçem` · `Kontrol` · `Ayarlar`
- **Uzman modu:** yukarıdakiler + `Grafikler` · `Ağ` · `Gelişmiş` · `Teşhis`

Uzman modundaki ekranlar önceki sürümle **birebir aynı yetkinliktedir**: kural
düzenleyici, aktüatör süre kısıtları, güvenlik eşikleri, sistem parametreleri,
görev sağlığı, olay günlüğü. Hiçbir işlev kaybolmadı.

`data-expert` işaretli düğümler moda göre gösterilir/gizlenir; tercih
`localStorage`'da tutulur ve depolama kapalıysa (gizli sekme) sessizce basit
moda düşer.

Uzman modu kapatılırken açık olan sekme uzman sekmesiyse ana sekmeye dönülür —
kullanıcı boş ekranda kalmasın diye.

## Karar 2 — Panelin üst yarısı "ne oluyor / ne yapmalıyım"

| Bileşen | İşi |
|---|---|
| **Durum bandı** | Tek cümle: bakılması gereken TEK yer |
| **Ürün kartı** | "Çilek · Meyve dönemi · 34. gün", hedef bantlar, dönem ilerlemesi |
| **Bugün ne yapmalıyım?** | Ölçümlerden TÜRETİLMİŞ, eyleme dönük cümleler |
| **Hızlı eylem** | "Şimdi Sula" / "Sulamayı Durdur" |
| **Ölçümler** | Trafik ışığı + düz cümle + hedef bandı işareti |

Ham sayı görmek isteyen kartlara bakabilir; üst yarı "şimdi ne yapayım"
sorusunu yanıtlar.

## Karar 3 — "İyi/kötü" kararı UYDURULMAZ

Önceki sürümde `SENSOR_META` içinde sabit `idealMin/idealMax` vardı ve
yetiştirilen bitkiyle **ilgisi yoktu**.

Artık bantlar `/api/crop` → `targets`'tan gelir. Ürün seçilmemişse band
**yoktur** ve arayüz sayıyı yorumsuz gösterip "Ürün seçilince hedef aralık
gösterilir" der. Uydurma bir ideal aralık, ilgisiz bir tavsiye demektir.

Bandın dışındayken "az dışında / çok dışında" ayrımı yapılır (band
genişliğinin dörtte biri): kullanıcı her küçük sapmada kırmızı uyarıya
boğulmasın.

Akış ve ışığın ürüne bağlı bandı yoktur; ikisi de donanıma bağlıdır.

## Karar 4 — Kurulum sihirbazı dört adım, sonuncusu ÖNİZLEME

```
1 Ürün  →  2 Dönem + dikim tarihi  →  3 Sulama yoğunluğu  →  4 Onay
```

Son adım `POST /api/crop/preview` çağırır ve cihazın hesapladığı planı
**kullanıcının dilinde** gösterir:

> Her 30 dk içinde 2 dk boyunca Su Pompası çalışır.
> Her gün 06:00 – 20:00 arasında Büyütme Işığı açık kalır.
> Su Sıcaklığı 18 °C altına düşünce Isıtıcı açılır, 19.5 °C olunca kapanır.

Mevcut kural varsa **kaçının silineceği** ayrıca uyarı olarak yazılır. Sistem
MANUEL moddaysa "program kurulacak ama hemen çalışmayacak" notu gösterilir —
kullanıcı kuralları görüp çalıştığını sanmasın.

Onaya kadar config'e **hiçbir şey yazılmaz**.

## Karar 5 — "Bağlı cihazlar" tek anlaşılır soru

Profilin hangi kuralları üretebileceğini belirleyen tek şey: hangi röle fiilen
kablolu. Ayarlar ekranında üç onay kutusu (ışık, ısıtıcı, dozaj pompası)
`actuators[].enabled` yazar.

Su ve hava pompası listede **yok**: onlar sistemin temelidir ve kapatmak
sulamayı tamamen durdurur — o karar Gelişmiş ekranına aittir.

Cihaz reddederse kutu **eski hâline döndürülür**: arayüz kaydedilmemiş bir
durumu kaydedilmiş gibi gösteremez (ARCHITECTURE P5).

## Karar 6 — JS modüllere bölündü, çıktı yine TEK dosya

```
frontend/js/10-core.js    sözlükler, depo, WS, komut yolu, REST
frontend/js/20-crop.js    hedef bantlar, tavsiyeler, sihirbaz
frontend/js/30-views.js   yönlendirme, panel, kontrol, basit ayarlar
frontend/js/40-expert.js  grafik, teşhis, kural düzenleyici, kısıtlar
frontend/js/50-init.js    oturum, giriş, başlatma
```

`tools/build_assets.py` bunları **ada göre** birleştirip tek `app.js.gz`
üretir. Ayrı dosya olarak sunulsaydı:

- tarayıcı 5 ayrı istek atar, her biri LittleFS'ten okunur — AP modunda gözle
  görülür gecikme
- ayrı gzip akışları toplamda daha büyük çıkar (sözlük paylaşılmaz)
- `index.html`'de script sırası elle tutulur; biri unutulunca hata ancak
  tarayıcıda "undefined is not a function" olarak görünür

`'use strict'` paketin başına **tek kez** konur.

## Ölçüm

| | Önce | Sonra |
|---|---|---|
| Varsayılan sekme sayısı | 6 | **3** |
| Toplam gzip varlık | 30,4 KB | 44,8 KB |
| LittleFS kullanımı | %3 | **%4** (896 KB bölüm) |
| En büyük JS dosyası | 1663 satır | 5 modül, en büyüğü ~700 satır |

Boyut arttı çünkü sihirbaz, ürün kartları, tavsiye motoru ve iki katmanlı
arayüz eklendi. 64 KB'lık uyarı eşiğinin altında.

## Doğrulama

Firmware olmadan doğrulamak için bir **sahte cihaz sunucusu** yazıldı
(scratchpad, depoya girmez): REST uç noktalarını firmware'in döndürdüğü
şemayla taklit eder. Tarayıcıda uçtan uca koşturuldu:

- [x] giriş → panel → ürün seçilmemiş durumu
- [x] bağlı cihaz işaretleme (3 röle)
- [x] sihirbaz 4 adım → önizleme → uygulama
- [x] yoğunluk ölçekleme (120 sn → 168 sn, ×1.4)
- [x] hedef bantlarla sensör kartları (8 sensör, 5 bantlı)
- [x] tavsiye motoru (EC düşük, pH yüksek, su soğuk, seviye düşük, arızalı sensör)
- [x] kontrol kartlarında engel nedeni ("Su seviyesi yetersiz — depoya su ekleyin")
- [x] kural elle düzenleme → `ÖZEL` + hedef bantların korunması
- [x] uzman modu → 7 sekme, kural düzenleyici, grafik, teşhis
- [x] mobil (375 px) — yatay taşma yok
- [x] konsolda JS hatası yok

## Düzeltilen iki kusur (tarayıcı testinde bulundu)

1. **`Yok lx`** — takılı olmayan sensörde birim gösteriliyordu; değer yokken
   birim de gizlenir.
2. **`Özel (strawberry temelli)`** — `CUSTOM` iken ürün adı ham anahtar olarak
   görünüyordu; katalog gerektiğinde yükleniyor. (Hedef bantların `CUSTOM`'da
   kaybolması TASK-068 Karar 4'te düzeltildi.)

## Definition of Done

- [x] Basit modda 3 sekme, uzman modunda 7
- [x] Uzman ekranlarında işlev kaybı yok
- [x] Sihirbaz onaysız yazma yapmıyor
- [x] Varlık üretimi tek `app.js.gz` veriyor
- [x] Tarayıcıda uçtan uca doğrulandı (sahte cihaz)
- [ ] **Gerçek cihazda doğrulanmadı** — WebSocket yolu (canlı telemetri, ack)
      yalnızca enjekte edilmiş durumla test edildi
