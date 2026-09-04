# Ürün Profilleri — Parametre Tablosu ve Kaynakları

> Kaynak kod: `src/core/CropProfile.cpp` · Model: `src/core/CropProfile.h`
> İlgili task'lar: TASK-067 (çekirdek), TASK-068 (uygulama), TASK-069 (API)

---

## 1. Bu belgenin amacı

`CropProfile.cpp` içindeki tablo **tarımsal iddialar** taşır: "çilek meyve
döneminde EC 1.8–2.2 mS/cm ister" bir yazılım kararı değildir. Bu değerler
kaynaklarıyla birlikte burada listelenir ki:

- gözden geçiren biri sayının nereden geldiğini görebilsin,
- kullanıcıya "bunlar uydurulmuş" izlenimi verilmesin,
- bir değer değiştirilecekse hangi kaynağa karşı değiştirildiği bilinsin.

> **BUNLAR BAŞLANGIÇ DEĞERİDİR.** Kaynaklar arasında %20–30 sapma olağandır;
> çeşit, iklim, sistem tipi (NFT / damlama / derin su) ve su kalitesi hedefi
> kaydırır. Kullanıcı üretilen kuralların tamamını Gelişmiş ekranından elle
> değiştirebilir; değiştirdiğinde profil `ÖZEL`'e düşer ve tablo devreye
> girmez.

---

## 2. Neden dönem ayrımı var

Tek bir "ortalama" değer iki dönemde birden yanlıştır. En somut örnek:

> Çileği domates/biber EC'sinde (2.5–3.5 mS/cm) çalıştırmak yaprak büyümesini
> tetikler ve **meyve tutumunu düşürür**. Çilek daha hafif ve **kararlı** bir
> çözelti ister; çiçeklenmeyle birlikte potasyum ağırlığı artar.

Aynı şey ürün içinde de geçerlidir: fide dönemindeki bir bitki, meyve
dönemindeki bitkinin çözeltisini kaldıramaz.

Bu yüzden model **ürün × dönem** matrisidir, ürün başına tek satır değil.

---

## 3. Katalog

Zorluk: ★ = kolay (geniş tolerans, hızlı hasat) · ★★ = orta · ★★★ = zor
(yoğun ışık, ağır besleme, kararlı koşul gerektirir).

### 3.1 Çilek — `strawberry` · ★★ · 4 dönem

| Dönem | pH | EC (mS/cm) | Su °C | Hava °C | Nem % | Işık | Süre |
|---|---|---|---|---|---|---|---|
| Fide | 5.5–6.2 | 0.8–1.2 | 18–22 | 18–24 | 65–80 | 10 sa | 21 g |
| Gelişme | 5.5–6.2 | 1.2–1.4 | 18–22 | 18–24 | 65–80 | 12 sa | 30 g |
| Çiçeklenme | 5.5–6.2 | 1.6–2.0 | 18–22 | 18–24 | 60–75 | 14 sa | 21 g |
| **Meyve** | 5.5–6.2 | **1.8–2.2** | 18–22 | 18–24 | 60–75 | 14 sa | — |

İlk hasat 90–120 gün; bitki 1–3 yıl üretir.

### 3.2 Domates — `tomato` · ★★★ · 4 dönem

| Dönem | pH | EC | Su °C | Hava °C | Nem % | Işık | Süre |
|---|---|---|---|---|---|---|---|
| Fide | 5.5–6.3 | 1.0–1.5 | 20–24 | 21–27 | 60–75 | 14 sa | 21 g |
| Gelişme | 5.5–6.3 | 1.8–2.5 | 20–24 | 21–27 | 60–70 | 16 sa | 30 g |
| Çiçeklenme | 5.5–6.3 | 2.2–3.0 | 20–24 | 21–27 | 60–70 | 16 sa | 25 g |
| Meyve | 5.5–6.3 | 2.5–3.5 | 20–24 | 21–27 | 55–70 | 16 sa | — |

Askı/ip desteği ve yüksek ışık şarttır.

### 3.3 Biber — `pepper` · ★★★ · 4 dönem

| Dönem | pH | EC | Su °C | Hava °C | Nem % | Işık | Süre |
|---|---|---|---|---|---|---|---|
| Fide | 5.8–6.3 | 1.0–1.4 | 20–25 | 21–28 | 60–75 | 14 sa | 25 g |
| Gelişme | 5.8–6.3 | 1.8–2.2 | 20–25 | 21–28 | 60–75 | 14 sa | 35 g |
| Çiçeklenme | 5.8–6.3 | 2.0–2.5 | 20–25 | 21–28 | 60–75 | 16 sa | 25 g |
| Meyve | 5.8–6.3 | 2.5–3.0 | 20–25 | 21–28 | 55–70 | 16 sa | — |

### 3.4 Salatalık — `cucumber` · ★★ · 4 dönem

| Dönem | pH | EC | Su °C | Hava °C | Nem % | Işık | Süre |
|---|---|---|---|---|---|---|---|
| Fide | 5.5–6.0 | 1.0–1.4 | 20–24 | 22–28 | 65–80 | 12 sa | 14 g |
| Gelişme | 5.5–6.0 | 1.7–2.0 | 20–24 | 22–28 | 65–80 | 14 sa | 21 g |
| Çiçeklenme | 5.5–6.0 | 1.8–2.2 | 20–24 | 22–28 | 60–75 | 14 sa | 14 g |
| Meyve | 5.5–6.0 | 2.0–2.5 | 20–24 | 22–28 | 60–75 | 15 sa | — |

Hızlı büyür ve ağır besler; **bodur (bush) çeşit** önerilir.

### 3.5 Marul — `lettuce` · ★ · **2 dönem**

| Dönem | pH | EC | Su °C | Hava °C | Nem % | Işık | Süre |
|---|---|---|---|---|---|---|---|
| Fide | 5.5–6.5 | 0.6–0.9 | 18–22 | 15–22 | 50–70 | 12 sa | 14 g |
| Gelişme | 5.5–6.5 | 0.8–1.2 | 18–22 | 15–22 | 50–70 | 14 sa | — |

21–30 günde hasat. **Çiçeklenme dönemi YOKTUR** ve istenmez: sapa kalkan
marul acılaşır. `stageCount = 2`; arayüz "meyve dönemi" seçtirmez,
`validateCrop()` böyle bir seçimi reddeder.

### 3.6 Fesleğen — `basil` · ★ · **2 dönem**

| Dönem | pH | EC | Su °C | Hava °C | Nem % | Işık | Süre |
|---|---|---|---|---|---|---|---|
| Fide | 5.5–6.0 | 0.6–1.0 | 20–24 | 20–27 | 55–70 | 12 sa | 18 g |
| Gelişme | 5.5–6.0 | 1.0–1.6 | 20–24 | 20–27 | 55–70 | 14 sa | — |

28–35 günde hasat; başlangıç için en uygun ürün.

---

## 4. Sulama ve havalandırma çevrimleri

Kaynaklarda tek bir doğru yoktur: sulama sıklığı **sistem tipine**, ortam
sıcaklığına ve bitki sayısına bağlıdır ve cihaz bunların hiçbirini ölçemez.

Bu yüzden tabloda "orta yol" bir çevrim tutulur ve kullanıcıya tek bir
anlaşılır ayar verilir:

| Yoğunluk | Çarpan |
|---|---|
| Az (`sparse`) | ×0.7 |
| Normal | ×1.0 |
| Bol (`abundant`) | ×1.4 |

Sonuç `MAX_GENERATED_ON_S` (240 sn) ile **kırpılır**. Gerekçe: su pompasının
varsayılan `maxRunMs`'i 5 dakikadır; daha uzun bir çevrim `ActuatorManager`
tarafından her turda süre aşımıyla kesilir ve kullanıcı doğru görünen bir
kuralın neden çalışmadığını bulamaz.

Havalandırma **yoğunluktan etkilenmez**: "bol sula" diyen kullanıcı daha çok
su ister, daha çok hava değil.

---

## 5. Hedeflerin hangisi KURALA dönüşür

Bu ayrım kritiktir. Tablodaki her bant otomatik bir eylem üretmez:

| Hedef | Karşılığı | Neden |
|---|---|---|
| `ec` | **Kural** — EC alt sınırına eşik, besin pompası | Dozaj pompası var |
| `waterTemp` | **Kural** — alt sınıra eşik + 1.5 °C histerezis, ısıtıcı | Isıtıcı rölesi var |
| `lightMinutes` | **Kural** — 06:00'dan itibaren pencere, büyütme ışığı | Işık rölesi var |
| `irrigation*` | **Kural** — periyodik çevrim, su pompası | Pompa var |
| `aeration*` | **Kural** — periyodik çevrim, hava pompası | Pompa var |
| `ph` | **Yalnızca gösterim** | **pH dozaj donanımı YOK** |
| `airTemp` | **Yalnızca gösterim** | Ortam ısıtma/soğutma donanımı yok |
| `humidity` | **Yalnızca gösterim** | Nemlendirici/fan yok |

Gösterim-amaçlı bantlar arayüzde "iyi / hedefin altında / hedefin üstünde"
yorumunu ve "pH yüksek, düşürücü ekleyin" tavsiyesini üretir. Bunlar
kullanıcının **elle** yapacağı işlerdir.

**Kural üretilmesinin iki ön koşulu vardır** (`buildCropRules`):
1. hedef aktüatör config'te `enabled` (kullanıcı "bu röle kablolu" demiş),
2. eşik kuralları için ilgili sensör `enabled`.

İkisinden biri eksikse kural **hiç üretilmez** — çalışamayacak bir kuralı
listede bırakmak, kullanıcıyı "neden çalışmıyor" sorusuyla baş başa
bırakmaktır.

---

## 6. Dönem ilerlemesi

`durationDays` sütunları toplanarak dikimden bu yana geçen güne göre dönem
hesaplanır (`stageForDay`). Üç koruma vardır:

1. **Zaman geçersizken ilerleme DURUR.** Donanımsal RTC yok (ISSUE-005); güç
   kesintisi + ağ yokluğunda duvar saati geçersizdir. Geçersiz saatle gün
   saymak, meyve dönemindeki çileği fideye döndürüp EC hedefini yarıya
   indirirdi.
2. **Dönem GERİ ALINMAZ.** SNTP saati geriye çekerse daha küçük bir dönem
   hesaplanabilir; ileri gitmemek yalnızca gecikme yaratır, geri gitmek
   bitkiyi aç bırakır.
3. **Kullanıcı kuralları elle düzenlerse otomatik ilerleme kapanır.** Aksi
   hâlde bir sonraki dönem geçişi kullanıcının düzenlemelerini sessizce
   silerdi.

---

## 7. Kaynaklar

Aşağıdaki kaynakların **ortak aralığı** alınmıştır; hiçbiri tek başına
kanonik değildir.

- HydroHowTo — *List of pH & EC Levels for 65+ Hydroponic Vegetables & Herbs*
  <https://hydrohowto.com/ph-ec-hydroponic-vegetable/>
- Urban Harvest Lab — *Hydroponic EC chart by crop*
  <https://urbanharvestlab.com/blog/hydroponics/hydroponic-ec-chart-by-crop/>
- HydroGreenSpace — *Hydroponic Strawberry Nutrient Solution: EC, pH & NPK*
  <https://www.hydrogreenspace.com/hydroponic-strawberry-nutrient-solution/>
- ISHS *Acta Horticulturae* 1315_78 — *Effect of different nutrient solution EC
  during growth stages on fruit and vegetative characteristics of strawberry in
  hydroponic system* <https://www.actahort.org/books/1315/1315_78.htm>
- Growee — *Best Hydroponic Vegetables & Crops for Beginners*
  <https://getgrowee.com/best-plants-for-hydroponic-farming/>
- SmartHydroLab — *Best Plants for Hydroponics: 15 Easy Crops for Beginners*
  <https://smarthydrolab.com/best-hydroponic-plants/>

---

## 8. Yeni ürün eklemek

1. `src/core/CropProfile.cpp` → `kCatalog` dizisine bir satır.
2. `CropId` enum'una yeni bir değer (**mevcut değerleri DEĞİŞTİRME** — config'te
   saklanıyor ve API'de taşınıyor).
3. `CROP_CATALOG_COUNT` sayısını artır.
4. Bu belgeye tabloyu ve kaynağı ekle.
5. `pio test -e native` — `test_every_crop_and_stage_produces_valid_rules`
   yeni ürünü otomatik kapsar.

Firmware'de **başka hiçbir yere dokunulmaz**: kural motoru, güvenlik zinciri ve
aktüatör kısıtları ürünleri bilmez.
