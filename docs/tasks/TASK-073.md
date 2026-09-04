# TASK-073 — Y1, Y2, O1: Denetimin Yüksek Öncelikli Bulguları

Denetimde ertelenen üç bulgu kapatıldı. Üçü de veri göçü, yeni API yüzeyi veya
ekran yerleşimi gerektirdiği için ayrı bir task'a bırakılmıştı.

---

## Y1 / ISSUE-034 — Geçmiş kaydı yanlış sensöre etiketleniyordu

### Neydi

`HistoryStore::append()` slotları **yayın sırasına** göre dolduruyordu:

```
0 level · 1 flow · 2 waterTemp · 3 ph · 4 ec · 5 airTemp
```

`HistoryApi` ise kendi içinde **elle yazılmış** — üstelik `slotName` ve
`slotId` içinde İKİ KEZ tekrarlanmış — farklı bir sırayı okuyordu:

```
0 waterTemp · 1 flow · 2 ph · 3 ec · 4 level · 5 humidity
```

Ölçek katsayıları da kimliğe bağlı olduğu için değerler yalnızca yanlış
etiketlenmiyor, **yanlış hesaplanıyordu**:

| Slot | Yazılan | Okunan etiket | Grafikte |
|---|---|---|---|
| 0 | seviye (2) | Su Sıcaklığı ÷10 | **0.2 °C** |
| 2 | su sıcaklığı 19.4 | pH ÷100 | **1.94 pH** |
| 3 | pH 6.05 | EC ÷100 | **6.05 EC** |
| 4 | EC 1.35 | Seviye ×1 | **135** |

`qualityMask` bitleri de aynı kaymayı taşıyordu. Grafikler sekmesi bugüne
kadar hep yanlış veri göstermiş.

İronik olarak `HistoryApi`'nin kendi yorumu doğruyu söylüyordu —
*"`Record.values[]` sırası snapshot'taki sıradır"* — ama kod onu uygulamıyordu.

### Karar 1 — Sıra TEK BİR TABLODA

`HistoryStore.h::SLOT_ORDER` artık tek doğruluk kaynağı. Yazıcı sensörü
**kimliğine göre** slotuna koyar (`slotOf()`), okuyucu aynı tabloyu kullanır.
İkisinin ayrışması imkânsız.

Sıra `SensorId` enum sırasından **bağımsızdır**: geçmiş dosyası uzun ömürlüdür
ve enum'a yeni bir sensör eklenmesi eski kayıtların anlamını değiştirmemelidir.
Yeni sensör tablonun **sonuna** eklenir.

Dört `static_assert` tabloyu koruyor: uzunluk, tekrar yokluğu, geçerli kimlik,
`MAX_SENSORS` ile eşleşme.

### Karar 2 — 8 slot, kayıt 24 → 28 bayt

Altı slotla kalıp iki sensörü dışarıda bırakmak, kullanıcının yeni taktığı
ışık ve hava sıcaklığı sensörlerini grafikte göremeyeceği anlamına gelirdi.

`int16` ölçekleri genişletildi:
- ortam sıcaklığı ×10 (0,1 °C çözünürlük)
- **ışık ×0.1 (dekalüks)** — 0–120 000 lüks `int16`'ya sığmaz; onda birine
  indirilince 12 000 olur. Grafik için 10 lüks çözünürlük fazlasıyla yeterli;
  kırpılsaydı güneşli her gün 32767'de düzleşirdi.

### Karar 3 — Dosya boyutu KORUNDU, süre kısaldı

Kayıt büyüdü ama dosya 476 KB'da tutuldu (`RECORD_COUNT` 20480 → 17408).
LittleFS bölümü 896 KB ve kalan alan aşınma dengeleme, meta veri ve web
varlıkları için gerekli; 560 KB'a çıkmak o payı yerdi.

**Bedeli açıkça kabul ediliyor: 60 sn periyotla 14,2 gün → 12,1 gün.**

### Karar 4 — Eski dosya SİLİNİR, taşınmaz

`/hist.bin` → `/hist2.bin`. Ad değişikliği bir geçersiz kılma aracıdır.

Sürüm 1 verisi **zaten bozuktu** — taşımanın anlamı yok. Dosya başlığı eklemek
yerine ad değiştirildi: halka ofset matematiği dosyanın 0'dan başlamasına
dayanıyor ve araya başlık koymak `offsetOf`, `findValidCount` ve
`findRotation`'ın tamamını etkilerdi.

Eski dosya boot'ta silinir ve INFO olarak loglanır. Sessizce bırakılsaydı
480 KB'lık ölü bir dosya yeni halkayla birlikte bölümü doldururdu.

---

## Y2 / ISSUE-035 — Sensörlerin yarısı hiç açılamıyordu

### Neydi

pH, EC, ortam nemi, hava sıcaklığı ve ışık `enabled = 0` doğuyordu — doğru bir
güvenli varsayılan. Ama:

- `PUT /api/config/sensors` **uç noktası yoktu**
- `GET /api/config` yanıtında **`sensors` bölümü yoktu**
- `ConfigService::updateSensor()` yazılmış ama **hiçbir yerden çağrılmıyordu**

Kullanıcı AHT20 ve BH1750'yi taksa bile arayüzde sonsuza kadar "takılı değil"
yazacaktı. pH kalibrasyonu (ARCHITECTURE §9.3) da aynı boşluktaydı.

### Karar 1 — Üç katmanlı çözüm

| Katman | Eklenen |
|---|---|
| API | `PUT /api/config/sensors` + `GET /api/config`'e `sensors` bölümü |
| Arayüz (basit) | "Takılı sensörler" — beş isteğe bağlı sensör için onay kutusu |
| Arayüz (uzman) | "Sensör Kalibrasyonu" — ölçek, kayma, geçerli aralık, filtre, sıçrama sınırı |

### Karar 2 — Sürücü ÇALIŞMA ANINDA başlatılır

İkinci katman: `sensorsvc::begin()` yalnızca boot'ta koşuyordu. Uç nokta
eklense bile açılan bir sensörün sürücüsü başlatılmaz, `sample()` her turda
`FAULT` dönerdi — kullanıcı sensörü açar, "okunamıyor" görür ve yeniden
başlatması gerektiğini **hiçbir yerden öğrenemezdi**.

`sensorsRevision()` sayacı eklendi (`rulesRevision()` ile aynı desen).
`SensorService::tick()` sayaç değiştiğinde etkinleştirilmiş ama hazır olmayan
sensörlerin sürücüsünü **`io_sense` bağlamında** başlatır — donanıma dokunan
tek task orası.

Başarısız denemeler **10 saniyede bir**e sınırlı: takılı olmayan bir çipi her
250 ms'de yoklamak I2C hattını OLED ile gereksiz paylaştırır ve olay günlüğünü
aynı hatayla doldururdu.

### Karar 3 — Güvenlik sensörleri kapatılamaz

`requireLevelSensor` açıkken seviye sensörünü kapatmak, pompa kilidini
**sessizce** devre dışı bırakırdı. `validateAll` bunu zaten yakalıyordu ama
orası ancak `persist()` sırasında çalışır; `updateSensor()` şimdi doğrudan
reddediyor — geçersiz durum RAM'e hiç girmiyor.

Arayüzde su seviyesi ve su akışı "Takılı sensörler" listesinde **yok**;
uzman kartlarında da onay kutusu yerine gerekçe yazıyor.

### Karar 4 — `validRange` ikisi birden verilmeli

Yalnızca `min` güncellenip `max` eskisinde kalırsa ters bir aralık oluşabilir
ve sensör her okumada `OUT_OF_RANGE` damgalanırdı. Uç nokta ikisini birlikte
ister.

### Karar 5 — `/api/config` akıtılır

Sensör bölümü eklenince yanıt ~2,1 KB'a çıktı ve 2 KB'lık sabit tamponu
aşıyordu: `writeConfigJson` 0 döner, kullanıcı "istek çok büyük" hatası alır ve
**tüm ayarlar ekranı çalışmazdı**.

Tamponu büyütmek yerine `AsyncResponseStream`'e geçildi (katalog ve geçmiş için
verilen kararın aynısı). `writeConfigJson(char*, size_t)` yerine
`fillConfigJson(JsonDocument&)`. **2 KB kalıcı `.bss` geri kazanıldı.**

---

## O1 / ISSUE-036 — OLED yeni donanımı görmüyordu

### Neydi

```cpp
constexpr uint8_t UI_SENSORS = 6;   // 8 sensör var
constexpr uint8_t UI_ACTS    = 2;   // 5 aktüatör var
```

Taşma yoktu (döngüler sınırlıydı) ama:
- nem ve ışık OLED'den **sessizce düşüyordu**
- `ViewModelBuilder` su/hava pompası dışındakileri filtreliyordu; büyütme
  ışığı, ısıtıcı ve dozaj pompası **ne görülebiliyor ne kontrol edilebiliyordu**

Filtre `AUX_1`/`AUX_2` eşlenmemişken doğruydu; TASK-066 ile beşinin de pini var.

### Karar 1 — Model tam kapasiteye çıkarıldı

`UI_SENSORS = MAX_SENSORS`, `UI_ACTS = MAX_ACTUATORS`. Sabit sayı yerine
kaynağa bağlanınca, bir sensör daha eklendiğinde OLED kendiliğinden büyüyor.

### Karar 2 — Kayan pencere

128×64 ekran gövdede 5 satır alıyor; sensör ekranı 8, kontrol ekranı 6 öğe
taşıyor. `windowStart()` imleci **her zaman görünür** tutar: seçili öğe
pencerenin altına taşarsa pencere onunla kayar. `cursor` doğrudan başlangıç
olarak kullanılsaydı listenin sonunda boş satırlar çizilirdi.

Sensör ekranında takılı olmayan satırlar önce **sıkıştırılır**: "yok" yazan bir
sensör pencerede yer kaplamamalı.

### Karar 3 — ACİL DURDUR her zaman son öğe

Kaydırmayla yeri değişmez. Kritik bir kontrolün konumunun listeye göre
oynaması kabul edilemez.

### Karar 4 — Kaydırma göstergesi olarak İMLEÇ

Sayısal bir "3-7/8" göstergesi denendi ve **durum çubuğuna sığmadı**: çubuğun
sağını mod yazısı kullanıyor ("CALISIYOR" ~54 px, sağa yaslı) ve gösterge
üzerine biniyordu. Gövdede de yer yok — değerler sağa yaslı.

İmleç zaten her satırda `>` ile çiziliyor ve kullanıcı çevirdikçe hareket
ediyor; listenin devam ettiğini bu yeterince anlatıyor. 128 px'lik bir ekranda
fazladan metin çakışma riskine değmez.

### Karar 5 — `itemCount` ile `isActionable` AYRILDI

`SENSORS` ekranında imleç var (kaydırma için) ama **basılacak eylem yok**.
Ayrılmasaydı basmak boş bir onay durumuna girer, "Onaylamak icin bas" hiçbir
yerde görünmez ve ikinci basış hiçbir şey yapmazdı — kullanıcı cihazı takılmış
sanardı.

### Karar 6 — İmleç → aktüatör eşlemesi türetilir

Eskiden iki aktüatör `actionFor()` içinde elle yazılıydı. Artık
`cursor < MAX_ACTUATORS` ise imleç doğrudan aktüatör indeksidir. Yeni bir röle
eklendiğinde zincirin güncellenmesi unutulsaydı hata sahada "düğme hiçbir şey
yapmıyor" olarak görünürdü.

---

## Doğrulama

Sahte cihaz sunucusuyla tarayıcıda uçtan uca:

- [x] **Y1** — 8 alan doğru sırada; her değer kendi birimine makul
      (`waterTemp=13.6 °C · ph=6.29 · ec=0.05 · level=2 · humidity=74 · airTemp=18.2 · light=44`).
      Sekiz grafik çipinin hepsi bir alana çözümleniyor.
- [x] **Y2** — beş sensör başlangıçta kapalı ve panelde "takılı değil";
      arayüzden açılınca **ölçüm gelmeye başlıyor**. pH kalibrasyonu
      kaydediliyor. Ters aralık ve sıfır ölçek istemci tarafında reddediliyor.
      Seviye sensöründe onay kutusu yok.
- [x] **O1** — derleniyor; kayan pencere ve gezinme mantığı gözden geçirildi.
- [x] `pio run` temiz, **0 uyarı** · Flash %66,9 · RAM %23,9

Host testleri (koşulamadı — gcc yok): slot tablosunun benzersizliği, her
sensörün tam bir slotu olması, bilinmeyen kimliğin reddi.

## Dokunulan dosyalar

```
src/services/HistoryStore.{h,cpp}      SLOT_ORDER, 8 slot, 28 bayt, dosya v2, eski dosya silinir
src/interfaces/web/api/HistoryApi.cpp  kendi tablosu silindi, SLOT_ORDER kullaniyor
src/services/ConfigService.{h,cpp}     updateSensor guclendirildi, sensorsRevision()
src/services/SensorService.cpp         calisma aninda surucu baslatma + yeniden deneme
src/interfaces/web/StateJson.{h,cpp}   sensors bolumu, fillConfigJson()
src/interfaces/web/api/ConfigApi.cpp   PUT /api/config/sensors, GET akitiliyor
src/interfaces/ui/ViewModels.h         UI_SENSORS/UI_ACTS tam kapasite, UI_VISIBLE_ROWS
src/interfaces/ui/ViewModelBuilder.cpp filtre kaldirildi, bes role etiketi
src/interfaces/ui/screens/Screens.cpp  kayan pencere (sensor + kontrol)
src/interfaces/ui/Navigation.cpp       itemCount, isActionable, imlec->aktuator
frontend/index.html                    Takili sensorler + Kalibrasyon + 8 grafik cipi
frontend/js/10-core.js                 OPTIONAL_SENSORS, SENSOR_DESC
frontend/js/30-views.js                renderSensorCard, toggleSensor
frontend/js/40-expert.js               renderSensorConfig, alan adi sozlugu
tools/mock_device.py                   sensor bolumu + uc nokta, HIST_FIELDS
test/test_domain/test_domain.cpp       3 yeni test
```

## Açık kalan

- `pio test -e native` hâlâ koşmuyor (host derleyicisi yok — ISSUE-033)
- Donanımda hiçbiri doğrulanmadı: yeni geçmiş dosyası, I2C sensör başlatma,
  OLED kaydırma
