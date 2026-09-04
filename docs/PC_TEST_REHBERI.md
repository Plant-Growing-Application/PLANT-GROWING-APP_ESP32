# PC'de Test Rehberi — ESP32 Olmadan Her Şeyi Denemek

> Amaç: telefona ve cihaza bağlı kalmadan, arayüzün ve mantığın tamamını
> bilgisayarda çalıştırmak.
>
> **Bu rehber bir SİMÜLATÖR anlatıyor.** Burada çalışan bir şey, firmware'in
> donanımda doğru çalıştığını **kanıtlamaz** — arayüzün doğru çizildiğini ve
> API sözleşmesinin tuttuğunu kanıtlar. Röle polaritesi, sensör kablolaması ve
> gerçek zamanlama yalnızca cihazda doğrulanabilir.

---

## 0. Beş dakikada başla

Bilgisayarda **tek komut**:

```bash
python tools/mock_device.py
```

Tarayıcıda aç: **http://127.0.0.1:8099**
Parola: **ne yazarsan yaz** (sahte cihaz her parolayı kabul eder).

Bu kadar. Aşağısı ayrıntı.

---

## 1. Gereken tek şey: Python

Python zaten PlatformIO ile birlikte kurulu. Ayrı bir şey yüklemene gerek yok.

**Windows'ta PlatformIO'nun Python'u:**

```bash
export PATH="$HOME/.platformio/python3:$PATH"
python --version
```

Kendi Python'un varsa o da olur (3.8+). Sahte cihaz **hiçbir harici paket
kullanmaz** — yalnızca standart kütüphane. `pip install` yok.

---

## 2. Sahte cihazı çalıştırma

```bash
python tools/mock_device.py
```

Çıktı:

```
  SALIXUS sahte cihaz  ->  http://127.0.0.1:8099
  Parola: herhangi bir sey  |  Sanal saat: 60x
  Ctrl+C ile durdurun
```

### Seçenekler

| Seçenek | Ne işe yarar |
|---|---|
| `--port 9000` | Başka bir bağlantı noktası (8099 doluysa) |
| `--speed 60` | Sanal saat hızı (varsayılan 60×) |

**Sanal saat neden var:** gerçek bir sulama çevrimi 30 dakikadır. 60× hızda
30 saniyede geçer, yani ekranda gerçekten bir şey olduğunu görebilirsin.

```bash
python tools/mock_device.py --speed 200   # cok hizli - cevrimleri izlemek icin
python tools/mock_device.py --speed 1     # gercek zaman
```

> **Dikkat:** hız arttıkça `minRunMs` gibi kısa kısıtlar tetiklenmez
> (5 saniyelik asgari süre, 90× hızda 55 milisaniyeye denk gelir). Gerçek
> cihazda normal işler.

**Durdurmak için:** terminalde `Ctrl+C`.

---

## 3. Arayüzü değiştirip anında görme

Arayüz kaynağı `frontend/` altında:

```
frontend/index.html      sayfa iskeleti
frontend/app.css         stiller
frontend/js/10-core.js   sözlükler, bağlantı, komut yolu
frontend/js/20-crop.js   ürün profili, sihirbaz, tavsiyeler
frontend/js/30-views.js  panel, kontrol, basit ayarlar
frontend/js/40-expert.js grafik, teşhis, kural düzenleyici
frontend/js/50-init.js   oturum ve başlatma
```

**Sahte cihaz dosyaları her istekte diskten okur.** Yani:

1. Dosyayı düzenle, kaydet
2. Tarayıcıda **F5**
3. Değişiklik ekranda

Sunucuyu yeniden başlatmana **gerek yok**. `build_assets.py` çalıştırmana da
gerek yok — o yalnızca cihaza yüklemek için gerekli.

> **Not:** sahte cihaz JS modüllerini ada göre sıralayıp birleştirir, tıpkı
> `build_assets.py` gibi. Yani sıraya bağlı bir hata varsa burada da görürsün.

---

## 4. Adım adım: her şeyi deneme senaryosu

Sırayla yaparsan sistemin tamamını görmüş olursun.

### 4.1 Giriş

- Parola alanına bir şey yaz → **Giriş Yap**
- Sağ üstte **Canlı** yazmalı (yeşil nokta). Yazmıyorsa sunucu kapalıdır.

### 4.2 Panel (Bahçem)

İlk açılışta **"Henüz bir ürün seçmediniz"** görürsün. Bu doğru: cihaz kutudan
çıktığında hiçbir şey yetiştirmiyor sayılır.

Bak:
- **Durum bandı** (en üst) — tek cümlelik genel durum
- **Ölçümler** — 8 kart. Çoğu "Bu sensör takılı değil" diyor, bu da doğru

### 4.3 Sensörleri açma

**Ayarlar → Takılı sensörler**

pH, EC, nem, hava sıcaklığı, ışık — beşini de işaretle.

Panele dön: kartlar artık **gerçek değer** gösteriyor. Değerler yavaşça
değişiyor çünkü sahte cihaz fiziksel bir benzetim çalıştırıyor.

> Gerçek cihazda da aynı: sensör işaretlenmeden okunmaz. Bu bölüm olmadan
> sensörleri açmanın hiçbir yolu yoktu (ISSUE-035).

### 4.4 Bağlı cihazları işaretleme

**Ayarlar → Bağlı cihazlar**

Büyütme ışığı, ısıtıcı, besin pompası — üçünü de işaretle.

Bu, "bu röle fiilen kablolu" demektir. İşaretlemezsen ürün profili o cihaz
için **kural üretmez** — çalışmayacak bir kural bırakmamak için.

### 4.5 Ürün seçme (meyve modu)

**Ayarlar → Ürün Seç** → sihirbaz açılır:

1. **Ürün** — Çilek seç
2. **Dönem** — Meyve seç, dikim tarihini bırak
3. **Sulama** — Normal
4. **Onay** — burada dur ve **oku**

Son ekran ne yazılacağını Türkçe anlatır:

```
Her 30 dk içinde 2 dk boyunca Su Pompası çalışır.
Her gün 06:00 – 20:00 arasında Büyütme Işığı açık kalır.
Su Sıcaklığı 18 °C altına düşünce Isıtıcı açılır, 19.5 °C olunca kapanır.
Besin Yoğunluğu (EC) 1.8 mS/cm altına düşünce Besin Pompası açılır…
```

**Uygula**'ya bas.

Panelde artık ürün kartı var: hedef bantlar (pH 5.5–6.2, EC 1.8–2.2…) ve
ölçüm kartlarında "İyi" / "Hedefin altında" yorumları.

### 4.6 Otomatik çalışmayı açma

**Ayarlar → Otomatik çalışma** → anahtarı aç → onayla.

**Bahçem**'e dön ve bekle. 60× hızda birkaç saniye içinde röleler kendiliğinden
dönmeye başlar. **Kontrol** sekmesinden canlı izleyebilirsin.

> Profil uygulamak tek başına sulama başlatmaz. Kurallar hazır olur ama motor
> MANUEL modda hiçbirini değerlendirmez. Bu bilinçli bir güvenlik kapısıdır.

### 4.7 Komut yolunu görme

**Kontrol** sekmesi → herhangi bir cihazda **Çalıştır**:

- Kart önce **BEKLİYOR** yazar
- Sonra **ÇALIŞIYOR**'a döner

Arada geçen süre gerçek: arayüz kendi kendine durum değiştirmez, cihazın
onayını bekler. Su pompasını çalıştırınca **Su Akışı** ölçümünün yükseldiğini
göreceksin — benzetim gerçekten tepki veriyor.

### 4.8 Acil durdurma ve temizleme

**Kontrol → ACİL DURDUR**

- Üstte kırmızı bant çıkar
- Tüm cihazlar kesilir
- Kilit listesi görünür: "Acil durum kilidi mandallanmış"

**Acil Durumu Temizle** → temizlenir.

**Şimdi ilginç kısım:** depo boşalınca (benzetimde birkaç dakika sürer, hızlı
modda saniyeler) temizleme **reddedilir** ve nedeni yazılır:

```
Temizlenemez — önce şunu düzeltin: Su seviyesi yetersiz — depoya su ekleyin.
```

> Bu davranış doğrudur: koşullar düzelmeden temizlemek, aynı arızayla pompayı
> yeniden çalıştırmak olurdu. Eskiden düğme sessizce hiçbir şey yapmıyordu
> (TASK-074).

### 4.9 Uzman modu

**Ayarlar → Uzman modu** anahtarı → 3 sekme **7 sekmeye** çıkar.

| Sekme | Ne var |
|---|---|
| **Grafikler** | Geçmiş ölçümler, 8 sensör seçilebilir |
| **Ağ** | Bağlantı durumu, IP, sinyal |
| **Gelişmiş** | Güvenlik eşikleri · **Sensör kalibrasyonu** · Kural düzenleyici · Cihaz süre kısıtları · Sistem parametreleri |
| **Teşhis** | Görev sağlığı, aktif hatalar, olay günlüğü |

**Kural düzenleyicide** bir kuralı değiştirip kaydet: ürün profili **ÖZEL**'e
düşer ve panelde "Özel (Çilek temelli)" yazar. Hedef bantlar korunur — bantlar
bitkiyi anlatır, kuralları değil.

### 4.10 Telefon görünümünü PC'de test etme

Tarayıcıda **F12** → cihaz araç çubuğu (Ctrl+Shift+M) → iPhone/Android seç.

Arayüz 375 px genişlikte yatay kaydırma olmadan çalışır.

---

## 5. Sahte cihaz ne yapıyor, ne yapmıyor

### Yapıyor

| | |
|---|---|
| **Canlı telemetri** | WebSocket üzerinden, gerçek protokolle |
| **Komut → onay → durum** | Gerçek yol: kuyruk, ack, gerçek röle durumu |
| **Fiziksel benzetim** | Pompa çalışınca debi oluşur ve depo boşalır; ısıtıcı suyu ısıtır; bitki EC'yi düşürür; dozaj yükseltir; ışık gün içi salınır |
| **Güvenlik zinciri** | Seviye düşünce pompa **ve ısıtıcı** kilitlenir; acil durum mandallanır |
| **Kural motoru** | Çevrim, pencere ve eşik kuralları gerçekten röleleri sürer |
| **Ürün profili** | Katalog, önizleme, uygulama, ÖZEL'e düşme |
| **Sensör yönetimi** | Açma/kapama, kalibrasyon, güvenlik sensörü koruması |

### Yapmıyor

| | |
|---|---|
| **Gerçek donanım** | Röle polaritesi, I2C adresleri, ADC gürültüsü |
| **Gerçek zamanlama** | Task periyotları, stack kullanımı, heap kararlılığı |
| **Tam güvenlik zinciri** | Basitleştirilmiş; akış doğrulama ve süre aşımı sayaçları yok |
| **NVS/flash** | Ayarlar bellekte tutulur, sunucu kapanınca sıfırlanır |

---

## 6. Firmware'i PC'de derleme

Cihaz olmadan da derleyip hataları görebilirsin:

```bash
export PATH="$HOME/.platformio/penv/Scripts:$PATH"
pio run
```

Beklenen çıktı:

```
RAM:   [==        ]  23.9% (used 78272 bytes from 327680 bytes)
Flash: [=======   ]  66.9% (used 1052529 bytes from 1572864 bytes)
SUCCESS
```

**0 uyarı** görmelisin (`-Wall -Wextra` açık). Uyarı çıkarsa bir şey bozulmuş.

### Cihaza yükleme (cihaz elindeyken)

```bash
pio run -t upload          # firmware
python tools/build_assets.py
pio run -t uploadfs        # web arayuzu
pio device monitor         # seri port
```

> `build_assets.py` çalıştırmayı unutursan cihazda **eski arayüz** kalır ve
> hiçbir hata mesajı almazsın.

---

## 7. Host testleri (şu an koşmuyor)

```bash
pio test -e native
```

Bu komut **şu an çalışmaz**: `native` ortamı bilgisayarın C++ derleyicisini
ister ve bu makinede yok (ISSUE-033).

Kurmak için (Windows):

```bash
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
```

Kurulumdan sonra yeni bir terminal aç ve komutu tekrar çalıştır. 46 test var:
zaman taşması, encoder yön değişimi, ürün profili üretimi, geçmiş slot sırası,
güvenlik kilidi matrisi.

---

## 8. Sık karşılaşılan durumlar

| Belirti | Sebep / çözüm |
|---|---|
| Tarayıcıda sayfa açılmıyor | Sunucu çalışmıyor. Terminale bak, `python tools/mock_device.py` |
| `Address already in use` | Bağlantı noktası dolu → `--port 9000` |
| Sağ üstte "Kopuk" yazıyor | Sunucu kapandı veya çöktü. Terminaldeki hatayı oku |
| Değişiklik ekranda görünmüyor | **Ctrl+F5** (önbelleği atlayarak yenile) |
| Sensörler "takılı değil" | Normal. Ayarlar → Takılı sensörler'den aç |
| Röleler kendiliğinden çalışmıyor | Otomatik çalışma kapalı. Ayarlar → Otomatik çalışma |
| "Temizlenemez — su seviyesi…" | Doğru davranış. Benzetimde depo boşalmış, dolmasını bekle |
| Türkçe karakterler bozuk | Terminal kodlaması. Arayüzü etkilemez |

---

## 9. Bir sonraki adım: gerçek cihaz

PC'de her şey çalışıyorsa cihazda yapılacaklar:

1. **Röle polaritesini ölç** (ISSUE-003) — 5 rölenin de aktif seviyesi
2. **I2C hattını tara** — AHT20 (0x38), BH1750 (0x23) yanıt veriyor mu
3. **İlk açılış** — seri porttan boot raporunu oku
4. `GET /api/diagnostics` → `minStack` ile stack boyutlarını düzelt
5. `docs/HARDWARE_TEST_PROCEDURE.md` §2 → **M4 güvenlik kapısı**
6. M4 geçtikten **sonra** ürün profilini uygula ve OTOMATİK'e al

Malzeme listesi ve kablolama: `docs/HARDWARE.md`
Ürün parametreleri ve kaynakları: `docs/CROP_PROFILES.md`
