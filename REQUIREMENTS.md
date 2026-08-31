# Topraksız Tarım Sistemi

## Proje Gereksinimleri

> **Doküman amacı:** Bu dosya, mevcut ESP32 tabanlı topraksız tarım (hidroponik) projesinin
> **kod tabanındaki gerçek durumunu** belgeler. Yeni mimari tasarımı, refactor önerisi veya
> hedef özellik listesi içermez. Buradaki her madde `src/`, `include/`, `data/` ve
> `platformio.ini` içeriğinden doğrulanmıştır.
>
> **Durum işaretleri**
> - `[x]` Tamamlanmış / çalışır durumda
> - `[~]` Kısmen tamamlanmış (kod var ama eksik, bağlı değil veya kısıtlı)
> - `[ ]` Eksik / kodda mevcut değil
>
> **Analiz tarihi:** 2026-08-30 · **Branch:** `V.0.1.1` · **Son commit:** `a2807e3`

---

## Donanım ve Proje Yapısı (Referans)

| Konu | Değer |
|---|---|
| Kart | `esp32dev` (ESP32, Arduino framework) |
| Ekran | SSD1306 128x64 OLED, I2C `0x3C`, `Wire.begin(21, 22)` |
| Dosya sistemi | LittleFS (`board_build.filesystem = littlefs`) |
| Partition | `no_ota` (OTA yok) |
| Kütüphaneler | Adafruit SSD1306/GFX/SH110X, ESPAsyncWebServer, AsyncTCP, ArduinoJson, sqlite3 (yerel `lib/`) |
| Monitor / Upload | 115200 baud, COM3 |

**Pin haritası (`include/Define.h`)**

| Pin | Sabit | Kullanım |
|---|---|---|
| 33 | `PIN_ENCODER_A` | Rotary encoder A (interrupt) |
| 32 | `PIN_ENCODER_B` | Rotary encoder B (interrupt) |
| 25 | `PIN_ENCODER_PUSH` | Encoder butonu (sayfa içine gir/çık) |
| 26 | `PIN_CONFIRM_BUTTON` | `pinMode` yapılıyor, **hiç okunmuyor** |
| 27 | `PIN_BACK_BUTTON` | WPS tetikleme butonu |
| 23 | `WIFI_LED` | Wi-Fi durum LED'i |
| 16 | `RELAY1` | Röle 1 (web arayüzünde "Su Pompası") |
| 17 | `RELAY2` | Röle 2 (web arayüzünde "Oksijen") |
| 34 | `PIN_WATER_FLOW` | Akış sensörü (interrupt, FALLING) |
| 35 | `PIN_WATER_TEMPRATURE` | Su sıcaklığı (analog / NTC) |

**Kaynak dosyalar:** `main.cpp`, `GrowPlant.cpp`, `MyWifi.cpp`, `WebServer.cpp`,
`DisplayProtocol.cpp`, `MyEeproom.cpp`, `RealTimeClock.cpp`, `Sensor.cpp`,
`SpeedSensor.cpp`, `TempratureSensor.cpp`, `SqlManager.cpp`

---

## 1. Sistem

- [x] **ESP32 açılışı** — `setup()` içinde Serial, I2C, EEPROM, LittleFS, OLED, pinler ve task'lar sırayla başlatılıyor.
- [x] **Configuration yüklenmesi** — `MyEeprom.GetSettings()` ile EEPROM'dan `Settings` yapısı okunuyor; magic (`0x5A5A5A5A`) geçersizse `memset` ile sıfırlanıyor.
- [~] **Sensörlerin başlatılması** — Sadece akış sensörü `SetupSpeedSensor()` ile başlatılıyor. Sıcaklık sensörü için yalnızca `pinMode(..., INPUT)` var, ayrı bir init yok. Başka sensör başlatılmıyor.
- [x] **Wi-Fi başlatılması** — `MywiFi.connect(4000)`; başarısızsa SoftAP moduna düşülüyor.
- [x] **Web Server başlatılması** — `webServer.begin()` ile route'lar ve WebSocket kaydediliyor.
- [x] **Display başlatılması** — `oled.begin()` başarısızsa **sonsuz `while(true)` döngüsüne** giriliyor (sistem tamamen kilitleniyor).
- [~] **Sistem durumunun belirlenmesi** — Yalnızca `IsServerMode` / `IsWpsActive` bayrakları var. Merkezi bir "system state / health" kavramı yok.
- [ ] **Sistem yeniden başlatma** — `ESP.restart()` çağrısı kodda hiç yok; web veya display üzerinden reboot yok.
- [~] **Başlangıç / initialization süreci** — Tüm init `setup()` içinde düz sıralı yazılmış. Init sonuçları saklanmıyor, bir "boot report" yok. LittleFS iki kez mount ediliyor (`setup()` + `WebServerManager::begin()`).
- [~] **Hata durumunda davranış** — LittleFS mount hatasında `setup()` `return` ile erken çıkıyor → **task'lar hiç oluşturulmuyor, sistem yarı ölü kalıyor**. OLED hatasında sistem kilitleniyor. Diğer hatalar sadece `Serial.println` ile raporlanıyor.

---

## 2. Wi-Fi / Network

- [x] **Wi-Fi bağlantısı** — `MyWiFi::connect(timeoutMs)`, EEPROM'daki SSID/şifre ile STA bağlantısı.
- [x] **Wi-Fi reconnect** — `Task_WiFiMonitor` her 1 sn'de bağlantı yoksa `connect(5000)` deniyor (Server modda değilse).
- [x] **SSID tarama** — `GET /scan`, `WiFi.scanNetworks(true, false)` ile asenkron tarama.
- [~] **SSID listeleme** — Backend listeyi JSON döndürüyor, frontend `<select>`'e dolduruyor. Ancak tarama devam ederken backend `202 {"status":"scanning"}` dönüyor, frontend bunu dizi sanıp `forEach` hatası veriyor → **ilk "TARA" tıklaması her zaman "Ağ taraması yapılamadı!" uyarısı veriyor**, tekrar tıklamak gerekiyor.
- [x] **SSID seçme** — `wifi.html` içinde `<select name="ssid">`.
- [x] **Şifre girme** — `wifi.html` içinde password input + göster/gizle butonu.
- [x] **Web üzerinden Wi-Fi ayarlama** — `POST /saveWiFi` → EEPROM'a yaz → `wifiShouldReconnect = true` → `Task_WiFiMonitor` içinde `ConnectFromWeb()` bağlantıyı kuruyor.
- [x] **DHCP** — Varsayılan mod (`_useDHCP = true`).
- [~] **Static IP** — `setStaticIP()` / `applyConfig()` fonksiyonları yazılmış ama **hiçbir yerden çağrılmıyor**; web arayüzünde veya display'de static IP girişi yok. `_useDHCP`, `IsServerMode` bayrağından türetiliyor (mantıksal olarak ilgisiz iki kavram).
- [x] **IP adresi görüntüleme** — OLED'de INTRO ve WIFI sayfalarında (`ShowIP()`).
- [~] **MAC adresi görüntüleme** — `ShowMac()` yalnızca INTRO sayfasında ve yalnızca `SendWifiInfo()` çağrıldığında çiziliyor; `SendWifiInfo()` periyodik çağrılmıyor.
- [x] **Wi-Fi bağlantı durumu** — `isConnected()`, `WiFi.status()`, LED blink, OLED "MODE: SERVER/CLIENT".
- [~] **Wi-Fi sinyal gücü** — Sadece tarama sonuçlarında `RSSI` var. Bağlı ağın sinyal gücü hiçbir yerde okunmuyor/gösterilmiyor.
- [~] **Wi-Fi bağlantı kaybı yönetimi** — Yalnızca `Task_WiFiMonitor` içindeki yeniden bağlanma denemesi. Disconnect event'i (`ARDUINO_EVENT_WIFI_STA_DISCONNECTED`) işlenmiyor. Bağlantı kalıcı koparsa AP moduna otomatik geçiş yok (bu sadece açılışta bir kez yapılıyor).
- [x] **Wi-Fi yeniden bağlanma** — Yukarıdaki monitor task'ı üzerinden.
- [x] **Wi-Fi ayarlarının kaydedilmesi** — EEPROM `Settings.SSID` / `Settings.Password`.
- [~] **Wi-Fi ayarlarının silinmesi / resetlenmesi** — `MyEEPROM::ResetEeprom()` yazılmış ama **hiçbir yerden çağrılmıyor**; kullanıcıya açık bir arayüz yok.
- [x] **Access Point / SoftAP** — `setWiFiMode()` içinde `WiFi.softAP("ESP32_SERVER", "12345678")`. Bilgiler OLED WIFI sayfasında gösteriliyor.
- [ ] **Captive Portal** — Yok. DNS server / yönlendirme kodu bulunmuyor.
- [~] **WPS** — `StartWps()` (PBC) + `ARDUINO_EVENT_WPS_ER_SUCCESS/FAILED/TIMEOUT` event'leri var, başarıda credential EEPROM'a yazılıyor. Ancak tetikleme yalnızca OLED WPS sayfasındayken `PIN_BACK_BUTTON`'a basılıyken çalışıyor ve "2 saniye" gibi gerçekçi olmayan bir timeout ile hata gösteriliyor. `include/Wps.H` / `src/Wps.cpp` commit `e6b30a1` ile silinmiş, `Define.h`'de include satırı yorumlu.
- [ ] **mDNS / hostname** — Yok.

---

## 3. Sensörler

Kodda **fiziksel olarak okunan yalnızca 2 sensör** vardır. Web arayüzünde 4 kart görünmesi,
ikisinin sabit HTML değeri olmasından kaynaklanır.

### 3.1 Su Sıcaklığı Sensörü (NTC — GPIO 35)

- [x] **Sensör bağlantısı** — `PIN_WATER_TEMPRATURE 35`, `pinMode(INPUT)`
- [x] **Sensör okuma** — `TempratureSensorClass::WaterTemprature()`, `analogRead()`
- [~] **Değerin işlenmesi** — Steinhart/Beta formülü uygulanıyor ancak sonuç `int`'e yuvarlanıyor (ondalık kayıp). Kalibrasyon sabiti yok, seri direnç değeri formülde yok.
- [x] **Web'de gösterilmesi** — `GET /api/sensors` → `temp-val` alanı, 600 ms'de bir güncelleniyor
- [x] **Display'de gösterilmesi** — SENSORS sayfası, 500 ms'de bir
- [ ] **Hata / bağlantı kopması yönetimi** — `sensorValue == 0` veya `4095` durumunda `log()` **matematiksel hata** üretir (sıfıra bölme / NaN); kontrol yok

### 3.2 Su Akış Sensörü (YF-S401 — GPIO 34)

- [x] **Sensör bağlantısı** — `PIN_WATER_FLOW 34`, `INPUT_PULLUP` + `attachInterrupt(FALLING)`
- [x] **Sensör okuma** — `SpeedSensorClass::GetWaterFlowRate()`, pulse sayımı
- [~] **Değerin işlenmesi** — `(pulses * 100) / 450` formülü **sabit zaman aralığı varsayıyor**, ancak fonksiyon farklı periyotlarda (600 ms task, 500 ms display) çağrılıyor → L/dk birimi güvenilir değil. Ayrıca her çağrıda sayaç sıfırlandığı için iki farklı çağıran birbirinin verisini tüketiyor.
- [x] **Web'de gösterilmesi** — `GET /api/sensors` → `flow-val`
- [x] **Display'de gösterilmesi** — SENSORS sayfası
- [ ] **Hata / bağlantı kopması yönetimi** — Yok (0 pulse ile "sensör yok" ayırt edilemiyor)
- [ ] **Toplam su hacmi (litre) sayacı** — Yok

### 3.3 pH Sensörü

- [ ] **Sensör bağlantısı** — Pin tanımı yok
- [ ] **Sensör okuma** — `TempratureSensorClass::PhSensor()` **sadece header'da bildirilmiş, implementasyonu yok**
- [ ] **Değerin işlenmesi** — Yok
- [~] **Web'de gösterilmesi** — `index.html` içinde `id="ph-val"` kartı var ama **sabit "6.5" yazıyor**, hiçbir JS bu alanı güncellemiyor
- [ ] **Display'de gösterilmesi** — Yok
- [ ] **Hata yönetimi** — Yok

### 3.4 Besin / EC (Nutriment) Sensörü

- [ ] **Sensör bağlantısı** — Pin tanımı yok
- [ ] **Sensör okuma** — `TempratureSensorClass::NutrimentSensor()` **sadece header'da bildirilmiş, implementasyonu yok**
- [ ] **Değerin işlenmesi** — Yok
- [~] **Web'de gösterilmesi** — `index.html` içinde `id="ec-val"` kartı var ama **sabit "1.2 mS"** yazıyor
- [ ] **Display'de gösterilmesi** — Yok
- [ ] **Hata yönetimi** — Yok

### 3.5 Nem Sensörü

- [ ] **Kodda hiçbir izi yok.** Ne pin, ne fonksiyon, ne arayüz elemanı.

### 3.6 Su Seviyesi Sensörü

- [ ] **Kodda hiçbir izi yok.** Ne pin, ne fonksiyon, ne arayüz elemanı.

### 3.7 Ortak Sensör Altyapısı

- [ ] **Ortak sensör arayüzü / soyutlama** — Yok, her sensör bağımsız global sınıf
- [ ] **Değer doğrulama (min/max aralık kontrolü)** — Yok
- [ ] **Filtreleme / ortalama alma** — Yok
- [ ] **Kalibrasyon** — Yok
- [ ] **Sensör verisi geçmişi / loglama** — Yok (bkz. Bölüm 7, SQLite kullanılmıyor)

---

## 4. Aktüatörler

Sistemde **iki adet röle çıkışı** vardır. Kodda "pompa" olarak isimlendirilmiş ayrı bir sınıf
veya soyutlama yoktur; pompa kavramı yalnızca web arayüzü etiketinde geçmektedir.

### 4.1 Su Pompası (RELAY1 — GPIO 16)

- [x] **Açma / Kapatma** — `digitalWrite(RELAY1, ...)` (WebSocket mesajı ile toggle)
- [x] **Manuel kontrol** — Web arayüzündeki "Su Pompası" kartı → `sendCmd(1)` → WebSocket
- [ ] **Otomatik kontrol** — **Yok.** Zamanlayıcı, sensöre bağlı tetikleme veya sulama döngüsü kodu bulunmuyor.
- [x] **Web üzerinden kontrol** — WebSocket `{"id":1}`
- [ ] **Display üzerinden kontrol** — OLED'de pompa/röle sayfası yok
- [ ] **Güvenlik koşulları** — Kuru çalışma koruması, maksimum çalışma süresi, minimum bekleme süresi — hiçbiri yok
- [ ] **Hata durumu** — Röle geri beslemesi veya akış ile doğrulama yok

### 4.2 Röle 2 / "Oksijen" (RELAY2 — GPIO 17)

- [x] **Açma / Kapatma** — `digitalWrite(RELAY2, ...)`
- [x] **Manuel kontrol** — Web arayüzü kartı → `sendCmd(2)`
- [ ] **Otomatik kontrol** — Yok
- [x] **Web üzerinden kontrol** — WebSocket `{"id":2}`
- [ ] **Display üzerinden kontrol** — Yok
- [ ] **Güvenlik koşulları / hata durumu** — Yok

> **Not:** `index.html` içinde bu kart **"Oksijen Sensörü"** olarak etiketlenmiştir, ancak
> bağlı olduğu şey bir röle çıkışıdır (aktüatör). İsimlendirme yanıltıcıdır.

### 4.3 Hava Pompası

- [ ] **Ayrı bir hava pompası kavramı kodda yok.** RELAY2 bu amaçla kullanılıyor olabilir,
      ancak kodda böyle bir isimlendirme veya mantık bulunmamaktadır.

### 4.4 Röle Genel

- [x] **Röle 1 / Röle 2 pin tanımı** ve `LOW` başlangıç durumu
- [~] **Röle durumu** — Yalnızca `digitalRead(pin)` ile anlık okunuyor; kalıcı durum saklanmıyor, yeniden başlatmada durum kayboluyor
- [x] **Manuel kontrol** — WebSocket üzerinden toggle
- [ ] **Otomatik kontrol** — Yok
- [ ] **Röle durumunun EEPROM'a kaydedilmesi** — Yok

### 4.5 Wi-Fi LED (GPIO 23)

- [x] Bağlıyken 200 ms periyotla yanıp sönüyor, bağlı değilken sönük (`Task_WifiLed`)

---

## 5. Web Arayüzü

Statik dosyalar LittleFS üzerinden servis ediliyor: `index.html`, `wifi.html`, `style.css`,
`script.js`, `bootstrap.min.css` (yerel kopya).

### 5.1 Dashboard (`/`)

- [~] **Dashboard** — Sayfa var, Bootstrap tabanlı, mobil uyumlu
- [ ] **Sistem durumu** — Gösterilmiyor
- [~] **Sensör değerleri** — Su sıcaklığı ve akış gerçek veriyle güncelleniyor; pH ve EC **sabit HTML değeri**
- [~] **Pompa durumu** — Butona tıklandığında yerel olarak (iyimser) değişiyor; **cihazdan gelen gerçek durum ile senkronize edilmiyor** (aşağıdaki nota bakınız)
- [~] **Röle durumları** — Aynı sorun
- [ ] **Wi-Fi durumu** — Dashboard'da gösterilmiyor (`script.js` `wifi-mode` elementini arıyor, HTML'de böyle bir element yok)
- [ ] **IP adresi** — Dashboard'da gösterilmiyor (`script.js` `ip` elementini arıyor, HTML'de yok)
- [ ] **Saat** — Web arayüzünde hiç gösterilmiyor
- [ ] **Hata mesajları** — Yok

> **Önemli tutarsızlık:** `script.js` içindeki `ws.onmessage`, röle durumunu `r1` / `r2` id'li
> elemanlara yazmaya çalışır; `index.html` içinde bu id'ler **yoktur**. Sunucudan dönen
> gerçek röle durumu bu nedenle arayüze hiç yansımaz. Sayfa yenilendiğinde de tüm kartlar
> "KAPALI" olarak başlar, gerçek röle durumu okunmaz.

> **Kırık link:** Navbar'daki `SALIXUS` logosu `/index` adresine gider; sunucuda böyle bir
> route yoktur → 404.

### 5.2 Wi-Fi Ayarları (`/wifi`)

- [x] **Wi-Fi ayar sayfası** — `wifi.html`
- [x] **SSID tarama** — `GET /scan`
- [~] **SSID listeleme** — Çalışıyor ama ilk tıklamada 202 yanıtı nedeniyle hata veriyor (bkz. Bölüm 2)
- [x] **SSID seçme** — `<select>`
- [x] **Şifre girişi** — Password alanı + göster/gizle
- [ ] **DHCP / Static IP seçimi** — Arayüzde yok
- [x] **Ayarları kaydetme** — `POST /saveWiFi` → EEPROM
- [ ] **Kaydetme sonrası geri bildirim** — Sunucu düz metin `WIFI:OK` döndürüyor, tarayıcı bu ham metni gösteriyor; kullanıcıya yönlendirme veya onay ekranı yok

### 5.3 Kontrol

- [x] **Pompa kontrolü** — WebSocket toggle
- [x] **Röle kontrolü** — WebSocket toggle
- [ ] **Manuel / otomatik mod seçimi** — Yok
- [ ] **Diğer aktüatör kontrolleri** — Yok
- [ ] **Zamanlayıcı / programlama arayüzü** — Yok
- [ ] **Ayarlar (eşik değerleri vb.) sayfası** — Yok

### 5.4 Gerçek Zamanlı Veri

- [~] **WebSocket** — `/ws` endpoint'i açık, mesaj alıyor ve `textAll` ile yayınlıyor. Ancak **sadece röle toggle** için kullanılıyor; sunucu kendiliğinden hiçbir veri push etmiyor.
- [ ] **Sensör verilerinin canlı güncellenmesi (WebSocket ile)** — Yok. Sensörler 600 ms'de bir **HTTP polling** (`/api/sensors`) ile alınıyor.
- [ ] **Sistem durumunun canlı güncellenmesi** — Yok
- [ ] **Aktüatör durumlarının canlı güncellenmesi** — Sunucu yanıt gönderiyor ama frontend bunu işleyemiyor (bkz. 5.1 notu)
- [~] **JSON işleme** — WebSocket mesajı `indexOf("\"id\":")` ile **elle string parse** ediliyor; ArduinoJson kullanılmıyor. Geçersiz mesajda tanımsız davranış riski var.

### 5.5 API Endpoint'leri

| METHOD | PATH | PURPOSE | CURRENT STATUS |
|---|---|---|---|
| GET | `/` | `index.html` servis et | `[x]` Çalışıyor |
| GET | `/wifi` | `wifi.html` servis et | `[x]` Çalışıyor |
| GET | `/style.css` | CSS servis et | `[x]` Çalışıyor |
| GET | `/bootstrap.min.css` | Bootstrap CSS servis et | `[x]` Çalışıyor |
| GET | `/script.js` | JS servis et | `[x]` Çalışıyor |
| POST | `/saveWiFi` | SSID/şifre kaydet, reconnect tetikle | `[x]` Çalışıyor (düz metin yanıt) |
| GET | `/scan` | Wi-Fi ağlarını tara / listele | `[~]` 202 ara-durumu frontend tarafından işlenmiyor |
| GET | `/api/sensors` | `{waterFlow, temperature}` | `[x]` Çalışıyor (yalnızca 2 sensör) |
| GET | `/api/status` | `{internet: bool}` | `[~]` Endpoint var, **hiçbir istemci çağırmıyor** |
| WS | `/ws` | Röle toggle + yayın | `[~]` Sadece röle; broadcast frontend'de işlenmiyor |
| — | `onNotFound` | 404 düz metin | `[x]` Çalışıyor |
| POST | `/api/clear-sensor` | *(JS `clearSensors()` bunu çağırıyor)* | `[ ]` **Sunucuda böyle bir endpoint yok** |
| — | `/index` | *(navbar linki)* | `[ ]` **Route yok → 404** |

- [ ] **Kimlik doğrulama / login** — `WebServer.h` içinde `handleLogin()` **bildirilmiş ama implementasyonu yok**; login/setup sayfaları commit `385100a` ile silinmiş. Web arayüzü tamamen korumasız.
- [ ] **HTTPS / güvenlik** — Yok
- [ ] **CORS / rate limiting** — Yok

---

## 6. Display (SSD1306 128x64 OLED)

### 6.1 Genel

- [x] **Ana ekran / Başlangıç ekranı** — `GoToPageIntro()`, PROGMEM logo bitmap
- [x] **Wi-Fi durumu** — WIFI sayfasında `MODE: SERVER/CLIENT`, SSID, şifre
- [x] **IP adresi** — INTRO ve WIFI sayfalarında (`ShowIP()`)
- [~] **MAC adresi** — `ShowMac()` var, yalnızca INTRO sayfasında ve yalnızca `SendWifiInfo()` çağrıldığında
- [~] **Saat** — `ShowClock()` var, ancak `SendWifiInfo()` yalnızca `GoToPageIntro()` içinde ve `setup()` sonunda bir kez çağrılıyor → **saat ekranda ilerlemiyor**, sayfa yeniden yüklenene kadar donuk kalıyor
- [x] **Sensör değerleri** — SENSORS sayfası, 500 ms'de bir yenileniyor (`Sensor.SensorValues()`)
- [ ] **Pompa durumu** — Gösterilmiyor
- [ ] **Röle durumu** — Gösterilmiyor
- [ ] **Hata ekranı** — Yok
- [~] **Menü sistemi** — Sayfalar dairesel liste olarak geziliyor; hiyerarşik menü, ayar düzenleme veya seçim listesi yok
- [x] **Kullanıcı etkileşimi** — Encoder ile sayfa değiştirme, encoder push ile sayfa içine gir/çık (girildiğinde köşe işaretleri çiziliyor)
- [x] **Rotary Encoder** — `DisplayProtocol`, çift kanal interrupt, 250 µs debounce, quadrature state machine
- [~] **Butonlar** — `PIN_ENCODER_PUSH` (25) kullanılıyor; `PIN_BACK_BUTTON` (27) yalnızca WPS için; **`PIN_CONFIRM_BUTTON` (26) `pinMode` yapılıyor ama hiç okunmuyor**

### 6.2 Mevcut Display Sayfaları

Toplam `TOTAL_PAGES 4`:

| # | Sabit | Fonksiyon | İçerik | Durum |
|---|---|---|---|---|
| 0 | `PAGE_INTRO` | `GoToPageIntro()` | Logo, saat, IP, MAC | `[~]` Saat/IP/MAC dinamik yenilenmiyor |
| 1 | `PAGE_WIFI` | `GoToPageWifi()` | Mod (SERVER/CLIENT), SSID, şifre, IP + mod değiştirme | `[x]` |
| 2 | `PAGE_WPS` | `GoToPageWPS()` | WPS başlat / internet bağlı durumu | `[~]` Tetikleme ergonomisi sorunlu |
| 3 | `PAGE_SENSORS` | `GoToPageSensors()` | WaterFlow, WaterTemp | `[x]` |

> **Not:** Bluetooth sayfası commit `f8c0de0` ile kaldırılmıştır.

### 6.3 Bilinen Display Sorunları

- `Sensor.cpp` içindeki `SensorValues()` ekrana **doğrudan çiziyor** → sensör okuma ile UI çizimi iç içe geçmiş; hangi sayfada olunduğu kontrol edilmiyor.
- `GoToPageWPS()` ve `StateWPS()` aynı yazıyı farklı Y koordinatlarına (35 / 28) yazıyor → tutarsız görünüm.
- `StateWPS()` yalnızca `WiFi.status() == WL_CONNECTED` iken ekranı güncelliyor; bağlı değilken görsel geri bildirim yok.
- `lib/Adafruit_SH1106-master` projede duruyor ama **kullanılmıyor** (kod SSD1306 kullanıyor).

---

## 7. Depolama / Configuration

### 7.1 Kullanılan Sistemler

- [x] **EEPROM (ESP32'de NVS üzerinden emüle edilir)** — **Ana konfigürasyon deposu.** `MyEEPROM` sınıfı, `StoredData{magic, Settings}` yapısı, magic `0x5A5A5A5A` doğrulaması.
- [x] **LittleFS** — Web statik dosyaları için (`index.html`, `wifi.html`, `style.css`, `script.js`, `bootstrap.min.css`)
- [ ] **Preferences API** — Kullanılmıyor (`MyWiFi` constructor'ı `prefsNamespace` parametresi alıyor ama **hiçbir yerde kullanmıyor** — ölü parametre)
- [ ] **SPIFFS** — `Define.h` içinde include edilmiş ama `platformio.ini` ile devre dışı bırakılmış (`DISABLE_SPIFFS=1`) → gereksiz include
- [~] **SQLite3** — `SqlManager` sınıfı ve `users` tablosu şeması yazılmış, kütüphane `lib/` altında mevcut. Ancak **`SqlManager::Begin()` hiçbir yerden çağrılmıyor** → `IsReady()` her zaman `false`, veritabanı hiç açılmıyor, hiç tablo oluşturulmuyor, hiç veri yazılmıyor. `Task_SensorLogger` içindeki "💾 Kaydedildi" mesajı **yalnızca Serial'e yazıyor, veritabanına hiçbir şey kaydetmiyor.**

### 7.2 Saklanan Ayarlar (`Settings` yapısı)

| Alan | Kullanım |
|---|---|
| `char SSID[32]` | `[x]` Aktif kullanılıyor |
| `char Password[32]` | `[x]` Aktif kullanılıyor |
| `char IP[16]` | `[ ]` Tanımlı ama **hiç yazılmıyor / okunmuyor** |
| `char MAC[16]` | `[ ]` Tanımlı ama **hiç yazılmıyor / okunmuyor** |
| `bool IsServerMode` | `[x]` Aktif (AP/STA mod bayrağı) |
| `bool IsWpsActive` | `[~]` Yazılıyor ama karar mantığında kullanılmıyor |
| `bool LittleFSFormatted` | `[ ]` Tanımlı ama **hiç kullanılmıyor** |

### 7.3 Fonksiyonlar

- [x] **Configuration kaydetme** — `SaveSettings()`
- [x] **Configuration yükleme** — `GetSettings()` (magic kontrolü ile)
- [~] **Factory reset** — `ResetEeprom()` implementasyonu var ama **hiçbir yerden çağrılmıyor**; kullanıcıya erişilebilir değil
- [x] **Wi-Fi bilgilerinin saklanması** — SSID + şifre, **düz metin olarak**
- [ ] **Network ayarlarının saklanması** — Static IP / gateway / subnet / DNS EEPROM'a **yazılmıyor**; `SaveIP()` / `GetIP()` **header'da bildirilmiş, implementasyonu yok**
- [ ] **Diğer ayarların saklanması** — Sensör eşikleri, otomasyon programı, röle durumu vb. hiçbiri saklanmıyor
- [ ] **Ayar versiyonlama / migration** — Yok
- [ ] **OTA güncelleme** — Yok (`no_ota` partition şeması)

---

## 8. Zaman / RTC

- [ ] **Donanımsal RTC (DS3231 vb.)** — Yok. `RealTimeClock` sınıf adına rağmen tamamen NTP tabanlıdır.
- [x] **NTP** — `configTime(gmtOffset, daylightOffset, ntpServer)`, `pool.ntp.org`
- [x] **İnternet üzerinden saat senkronizasyonu** — `rtc.begin()` içinde `setup()` sırasında bir kez
- [~] **Timezone** — `main.cpp` içinde **sabit kodlanmış**: `RealTimeClock rtc("pool.ntp.org", 10800, 0)` (GMT+3). Yapılandırılabilir değil, yaz saati desteği yok (`daylightOffset = 0`).
- [~] **Saatin Display'de gösterilmesi** — `ShowClock()` var ama periyodik çağrılmadığı için **saat ilerlemiyor** (bkz. Bölüm 6)
- [ ] **Saatin Web'de gösterilmesi** — Yok
- [~] **İnternet yokken davranış** — `getFormattedTime()` senkronize olmayan durumda `"00:00:00"` döndürüyor; kullanıcıya "saat geçersiz" bilgisi verilmiyor
- [ ] **Periyodik yeniden senkronizasyon** — `updateTime()` yazılmış ama **hiçbir yerden çağrılmıyor**
- [ ] **Zamana bağlı otomasyon (sulama programı)** — Yok

---

## 9. Güvenlik ve Hata Yönetimi

- [ ] **Sensor failure** — Sensör okuma hatası tespiti yok; geçersiz analog değer matematiksel hataya yol açabilir
- [~] **Wi-Fi failure** — Açılışta AP moduna düşülüyor, çalışırken periyodik reconnect deneniyor. Disconnect event'i işlenmiyor, deneme sayısı / backoff yok.
- [ ] **WebSocket failure** — Sunucu tarafında hata yakalama yok; istemci tarafında `onclose` sadece metin yazıyor, **otomatik yeniden bağlanma yok**
- [ ] **Web Server failure** — `_server.begin()` dönüşü kontrol edilmiyor
- [~] **Storage failure** — LittleFS mount hatası tespit ediliyor ama davranış hatalı (`setup()` erken `return` → task'lar oluşmuyor). EEPROM yazma hatası kontrol edilmiyor.
- [ ] **Invalid sensor value** — Aralık / mantık kontrolü yok
- [ ] **Pump failure** — Pompa çalışıyor mu doğrulaması yok (akış sensörü ile çapraz kontrol yapılmıyor)
- [~] **Watchdog** — `esp_task_wdt_init(WDT_TIMEOUT=15, true)` var. Ancak:
  - **`esp_task_wdt_init()` task'lar oluşturulduktan SONRA çağrılıyor** → task'lardaki `esp_task_wdt_add(NULL)` çağrıları init'ten önce çalışabilir (yarış durumu).
  - `Task_SensorLogger` **watchdog'a hiç kaydolmuyor**, dolayısıyla izlenmiyor.
- [ ] **ESP32 crash recovery** — Panic handler, reset reason kaydı, crash log yok
- [ ] **Automatic restart** — Yok
- [ ] **Safe state (arıza durumunda röleleri kapatma)** — Yok
- [~] **Logging** — Yalnızca `Serial.println` ile dağınık, seviyesiz (INFO/WARN/ERROR ayrımı yok), emoji tabanlı. Kalıcı log yok.
- [ ] **Web arayüzü kimlik doğrulama** — Yok (bkz. Bölüm 5.5)
- [ ] **Şifrelerin güvenli saklanması** — Wi-Fi şifresi EEPROM'da düz metin, ayrıca **OLED WIFI sayfasında açıkça gösteriliyor**
- [ ] **Girdi doğrulama** — `/saveWiFi` ve WebSocket mesajlarında uzunluk / format doğrulaması yok

---

## 10. FreeRTOS / Task Sistemi

Toplam **4 task** `setup()` içinde oluşturuluyor. Hiçbiri belirli bir çekirdeğe sabitlenmemiş
(`xTaskCreate` kullanılıyor, `xTaskCreatePinnedToCore` değil).

### Task_WiFiMonitor

- **Amaç:** Wi-Fi bağlantısını canlı tutmak, WPS ve web kaynaklı bağlanma isteklerini işlemek
- **Çalışma periyodu:** 1000 ms
- **Sorumluluk:** `connect(5000)`, `ConnectFromWPS()`, `ConnectFromWeb()`
- **Bağımlılıklar:** `MyWiFi`, `MyEeprom`, `GrowPlant` (OLED'e doğrudan yazıyor), `WiFi`
- **Stack / Öncelik:** 8192 / 1
- **Watchdog:** `[x]` Kayıtlı
- **Durum:** `[~]` Çalışıyor. Ancak `connect()` içindeki 5 sn'lik **bloklayan while döngüsü** task'ı kilitliyor; `pauseWiFiMonitor()` / `resumeWiFiMonitor()` ile dışarıdan `vTaskSuspend` ediliyor (kırılgan desen). Handle: `xWiFiMonitorHandle`.

### Task_Display

- **Amaç:** Encoder girdisini okuyup sayfa değiştirmek, aktif sayfayı yenilemek
- **Çalışma periyodu:** 50 ms (SENSORS sayfası ayrıca 500 ms'de yenileniyor)
- **Sorumluluk:** `ChangePage()`, `SelectedPage()`, SENSORS sayfasında `Sensor.SensorValues()`
- **Bağımlılıklar:** `DisplayProtocol`, `GrowPlant`, `Sensor`, `oled`
- **Stack / Öncelik:** 4096 / 2
- **Watchdog:** `[x]` Kayıtlı
- **Durum:** `[x]` Çalışıyor

### Task_WifiLed

- **Amaç:** Wi-Fi durumunu LED ile göstermek
- **Çalışma periyodu:** Bağlıyken 200 ms blink, bağlı değilken 1000 ms
- **Sorumluluk:** `digitalWrite(WIFI_LED, ...)`
- **Bağımlılıklar:** `WiFi.status()`
- **Stack / Öncelik:** 2048 / 3
- **Watchdog:** `[x]` Kayıtlı
- **Durum:** `[x]` Çalışıyor

### Task_SensorLogger

- **Amaç:** Sensör değerlerini periyodik okumak ve (hedeflenen) veritabanına kaydetmek
- **Çalışma periyodu:** 50 ms döngü; sensör okuma 600 ms, "log" 10000 ms
- **Sorumluluk:** `Sensor.WaterFlow` / `Sensor.WaterTemprature` güncelleme
- **Bağımlılıklar:** `SpeedSensor`, `TempratureSensor`, `SqlManager`
- **Stack / Öncelik:** 4096 / 4
- **Watchdog:** `[ ]` **Kayıtlı değil**
- **Durum:** `[~]` Sensör okuma çalışıyor; **loglama çalışmıyor** — `DB.IsReady()` hiçbir zaman `true` olmadığı için blok hiç çalışmaz, çalışsa bile yalnızca `Serial.print` yapar.

### Diğer

- [ ] **Web Task / WebSocket Task** — Ayrı task yok; ESPAsyncWebServer kendi AsyncTCP task'ı üzerinden çalışıyor
- [~] **`loop()`** — Yalnızca `ws.cleanupClients()` çağırıyor; `vTaskDelay` veya `delay` yok → boş döngü sürekli CPU harcıyor
- [ ] **Task'lar arası senkronizasyon** — Mutex, semaphore veya queue **hiç kullanılmıyor**. Paylaşılan global değişkenler (`Sensor.WaterFlow`, `currentIP`, `MyEeprom.Setting`) korumasız erişiliyor.
- [ ] **OLED erişimi için mutex** — Yok. `Task_Display`, `Task_WiFiMonitor` (`ConnectFromWPS`) ve `Sensor.SensorValues()` aynı `oled` nesnesine korumasız yazıyor → **yarış durumu riski**.
- [ ] **Task health monitoring / stack watermark** — Yok

---

## 11. Eksik Özellikler

### Critical

- [ ] **Otomasyon mantığı** — Sistemin asıl amacı olan otomatik sulama / besleme döngüsü **hiç yazılmamış**. Röleler yalnızca manuel toggle edilebiliyor.
- [ ] **Pompa güvenlik koşulları** — Kuru çalışma koruması, maksimum çalışma süresi, akış doğrulaması yok. Röle web'den açık bırakılırsa süresiz çalışır.
- [ ] **Safe state / arıza davranışı** — Sensör, Wi-Fi veya sistem hatasında rölelerin güvenli duruma alınması yok.
- [ ] **Paylaşılan kaynak koruması (mutex)** — OLED ve global sensör/ayar değişkenleri task'lar arasında korumasız → kararsızlık riski.
- [ ] **Su seviyesi sensörü** — Pompa güvenliği için zorunlu; kodda hiç yok.
- [ ] **LittleFS mount hatasında sistemin yarı ölü kalması** — `setup()` erken `return` ediyor, hiçbir task oluşmuyor.

### High

- [ ] **pH sensörü** — Header'da bildirim var, implementasyon yok; arayüzde sabit değer gösteriliyor.
- [ ] **EC / besin sensörü** — Aynı durum.
- [ ] **Sensör verisi kalıcı loglama** — SQLite altyapısı hazır ama `Begin()` çağrılmadığı için hiç kullanılmıyor.
- [ ] **Web arayüzü ile cihaz durumunun senkronizasyonu** — Röle durumları sayfa açılışında ve toggle sonrası cihazdan okunmuyor.
- [ ] **WebSocket ile sunucu-taraflı veri push** — Sensörler HTTP polling ile alınıyor (600 ms), WebSocket boşta.
- [ ] **WebSocket otomatik yeniden bağlanma (frontend)** — Bağlantı koptuğunda kurtarma yok.
- [ ] **Saatin periyodik güncellenmesi** — OLED'de saat donuk kalıyor; NTP yeniden senkronizasyonu yok.
- [ ] **Web arayüzü kimlik doğrulama** — Ağdaki herkes röleleri kontrol edebiliyor.
- [ ] **Watchdog kurulum sırası ve tam kapsam** — Init sırası hatalı, `Task_SensorLogger` izlenmiyor.

### Medium

- [ ] **Factory reset arayüzü** — `ResetEeprom()` var ama erişilemiyor.
- [ ] **Static IP arayüzü** — Backend fonksiyonları var ama hiç çağrılmıyor, UI yok.
- [ ] **Sistem yeniden başlatma (web / display)** — `ESP.restart()` hiç kullanılmıyor.
- [ ] **Sensör kalibrasyonu ve eşik ayarları** — Yok, hiçbir eşik değeri saklanmıyor.
- [ ] **Akış sensörü hesabının zaman tabanlı yapılması** — Mevcut formül sabit periyot varsayıyor ve iki farklı çağıran sayacı paylaşıyor.
- [ ] **Sıcaklık değerinin `float` olarak taşınması** — Şu an `int`, ondalık hassasiyet kayboluyor.
- [ ] **`/api/clear-sensor` endpoint'i** — Frontend çağırıyor, backend'de yok.
- [ ] **`/index` route'u veya navbar linkinin düzeltilmesi** — 404 üretiyor.
- [ ] **`/scan` 202 ara-durumunun frontend'de işlenmesi** — İlk tarama denemesi her zaman hata veriyor.
- [ ] **Yapılandırılabilir timezone / NTP sunucusu** — Sabit kodlanmış.
- [ ] **Röle durumunun kalıcı saklanması** — Yeniden başlatmada durum kayboluyor.
- [ ] **Nem sensörü** — Kodda hiç yok.
- [ ] **`loop()` içinde `vTaskDelay`** — Boş döngü CPU harcıyor.

### Low

- [ ] **Yapılandırılmış logging (seviyeli `LOG_I/W/E`)** — Şu an dağınık `Serial.println`.
- [ ] **OTA güncelleme** — `no_ota` partition ile devre dışı.
- [ ] **mDNS / hostname** — `growplant.local` gibi bir erişim yok.
- [ ] **Captive Portal** — AP moduna bağlanan kullanıcı IP'yi elle yazmak zorunda.
- [ ] **`PIN_CONFIRM_BUTTON` (26) kullanımı** — Pin ayrılmış ama okunmuyor.
- [ ] **Display'de pompa / röle sayfası** — OLED'den aktüatör kontrolü yok.
- [ ] **Web'de saat gösterimi** — Yok.
- [ ] **Bağlı ağın sinyal gücü (RSSI) gösterimi** — Yok.
- [ ] **Kullanılmayan kodun temizlenmesi** — `handleLogin()` (bildirim var, gövde yok), `SaveIP()` / `GetIP()` (bildirim var, gövde yok), `extern WebServerManager WebServer` (tanımı yok), `lib/Adafruit_SH1106-master` (kullanılmıyor), `Settings.IP` / `Settings.MAC` / `Settings.LittleFSFormatted` (kullanılmıyor), `MyWiFi::begin()` (hiç çağrılmıyor), `prefsNamespace` parametresi (kullanılmıyor), `lastSave` / `previousTime` global değişkenleri (kullanılmıyor), `#include <SPIFFS.h>` (devre dışı).
- [ ] **Birim / entegrasyon testleri** — `test/` klasörü boş (yalnızca README).

---

## 12. Belirsiz / İncelenmesi Gerekenler

- [ ] **`RELAY2` gerçekte neye bağlı?** Web arayüzünde "Oksijen Sensörü" yazıyor ancak bir röle çıkışı sürüyor. Donanımda hava pompası mı, oksijen sensörü besleme hattı mı, başka bir şey mi olduğu koddan anlaşılmıyor — **donanım şeması ile doğrulanmalı.**
- [ ] **Su sıcaklığı sensörünün tipi ve devresi.** Formül NTC / Beta (3950) varsayıyor ancak seri direnç değeri ve besleme gerilimi koda yansımamış. Sensörün gerçekten NTC olup olmadığı ve bölücü devre değerleri **doğrulanmalı**.
- [ ] **Akış sensörü modeli.** Kodda `YF-S401` yorumu ve `450` katsayısı var; sahadaki sensörün bu model olup olmadığı **doğrulanmalı**.
- [ ] **SQLite3 gerçekten hedefleniyor mu?** Kütüphane, sınıf ve `users` tablosu şeması var ama hiç etkinleştirilmemiş; git geçmişinde defalarca eklenip çıkarılmış (`bad0bac`, `41e5120`, `5a2fee7`, `cd6980b`). ESP32'de sensör loglama için SQLite'ın kalıcı bir tercih mi yoksa terk edilmiş bir deneme mi olduğu **netleştirilmeli**.
- [ ] **`Settings.IsServerMode` ile `_useDHCP` ilişkisi.** `MyWiFi::begin()` içinde `_useDHCP = (IsServerMode == 0)` şeklinde türetiliyor. Bu iki kavramın kasıtlı olarak mı bağlandığı yoksa hata mı olduğu **netleştirilmeli** (`begin()` zaten hiç çağrılmıyor).
- [ ] **Login / kullanıcı sistemi geri gelecek mi?** `login.html`, `setup.html`, `db.html` commit `385100a`'da silinmiş; `handleLogin()` bildirimi ve SQLite `users` tablosu geride kalmış. Hedefin ne olduğu **netleştirilmeli**.
- [ ] **WPS özelliği korunacak mı?** `Wps.cpp` / `Wps.H` silinmiş, mantık `MyWifi.cpp` içine taşınmış, `Define.h`'de include yorum satırında. WPS'in kalıcı bir gereksinim olup olmadığı **netleştirilmeli**.
- [ ] **Encoder `stepsPerDetent = 1.5` değeri.** Kodda "enkoderin kalitesine göre 4 olabilir" yorumu var ve değer `double` olmasına rağmen `int` sayaçla karşılaştırılıyor. Kullanılan encoder ile **sahada doğrulanmalı**.
- [ ] **`data/` ve `Data/` klasör adı.** Git `Data/` olarak takip ediyor, dosya sisteminde `data/` görünüyor. Windows'ta sorun çıkarmıyor ancak büyük/küçük harf duyarlı bir sistemde LittleFS imajı üretilemez — **doğrulanmalı**.
- [ ] **Kullanılmayan `.kilo/worktrees/` kopyaları.** Depo içinde projenin birden fazla eski kopyası duruyor; hangisinin referans olduğu **netleştirilmeli** (bu analiz yalnızca kök dizindeki koda dayanmaktadır).

---

# Proje Durum Özeti

## Tamamlananlar

- ESP32 açılış ve init akışı (Serial, I2C, EEPROM, LittleFS, OLED, pinler, task'lar)
- Rotary encoder ile 4 sayfalı OLED menü gezinme ve sayfa içi giriş / çıkış
- Wi-Fi STA bağlantısı, başarısızlıkta SoftAP'a düşme, periyodik yeniden bağlanma
- Web üzerinden Wi-Fi kurulumu (tarama, seçme, şifre, EEPROM'a kaydetme, reconnect)
- EEPROM tabanlı konfigürasyon kaydetme / yükleme (magic doğrulamalı)
- Asenkron web sunucusu + LittleFS statik dosya servisi (Bootstrap tabanlı responsive arayüz)
- 2 sensörün (su sıcaklığı, su akışı) okunması, OLED ve web'de gösterilmesi
- WebSocket üzerinden 2 rölenin manuel toggle edilmesi
- NTP ile açılışta bir kez saat senkronizasyonu
- 4 FreeRTOS task'ı ve Wi-Fi durum LED'i

## Kısmen Tamamlananlar

- **WPS** — Mantık var, tetikleme ergonomisi ve timeout mantığı sorunlu
- **Watchdog** — Kurulum sırası hatalı, bir task izlenmiyor
- **Wi-Fi tarama** — Backend doğru, frontend ara-durumu (202) işleyemiyor
- **WebSocket** — Sadece röle toggle; durum senkronizasyonu frontend'de kırık
- **Display saat / IP / MAC** — Çizim fonksiyonları var, periyodik yenileme yok
- **Static IP** — Backend fonksiyonları yazılmış, hiç çağrılmıyor, UI yok
- **Factory reset** — Implementasyon var, erişilemiyor
- **SQLite / veri loglama** — Altyapı var, hiç etkinleştirilmemiş
- **Hata yönetimi** — Yalnızca Serial çıktısı düzeyinde, kurtarma davranışı yok

## Eksikler

- Otomasyon mantığının tamamı (zamanlayıcı, sensöre bağlı sulama / besleme döngüsü)
- Pompa güvenlik koşulları ve safe state davranışı
- pH, EC / besin, nem ve su seviyesi sensörleri
- Kalıcı sensör verisi loglama ve geçmiş görüntüleme
- Task'lar arası senkronizasyon (mutex / queue) ve OLED erişim koruması
- Web arayüzü kimlik doğrulama ve güvenlik katmanı
- Sistem yeniden başlatma, crash recovery, yapılandırılmış logging
- OTA, mDNS, captive portal
- Eşik / ayar yönetimi ve bunların kalıcı saklanması
- Test altyapısı

## Kritik Problemler

1. **Otomasyon yok** — Proje bir "topraksız tarım otomasyonu" olarak adlandırılmasına rağmen kodda hiçbir otomatik karar mekanizması bulunmuyor. Sistem şu an uzaktan kumandalı bir röle anahtarı + sensör göstergesi düzeyinde.
2. **Paylaşılan kaynaklar korumasız** — `oled`, `Sensor.*` ve `MyEeprom.Setting` birden fazla task'tan mutex olmadan erişiliyor. Bu, sahada tekrarlanması zor kararsızlıkların en olası kaynağıdır.
3. **Bloklayan Wi-Fi bağlantısı task içinde** — `connect()` 5 saniyeye kadar `delay(50)` döngüsüyle blokluyor; task dışarıdan `vTaskSuspend` / `vTaskResume` ile yönetiliyor (kırılgan desen).
4. **Hata durumunda sistem yarı ölü kalıyor** — LittleFS mount hatasında `setup()` erken dönüyor ve hiçbir task oluşmuyor; OLED hatasında sonsuz döngüye giriliyor.
5. **Frontend ile backend sözleşmesi tutarsız** — Var olmayan endpoint çağrısı (`/api/clear-sensor`), var olmayan DOM id'leri (`r1`, `r2`, `ip`, `wifi-mode`), var olmayan route (`/index`), işlenmeyen 202 yanıtı.
6. **Ölü kod yükü** — Bildirilip yazılmamış fonksiyonlar, hiç çağrılmayan sınıflar (`SqlManager`), kullanılmayan struct alanları ve kullanılmayan kütüphaneler mevcut yapıyı okunmaz hale getiriyor.
7. **Güvenlik yok** — Web arayüzü korumasız; Wi-Fi şifresi hem EEPROM'da düz metin hem de OLED ekranında açıkça görünüyor.

## Genel Durum

**Kullanıcının verdiği tahmini tamamlanma oranı: %60**

**Kod analizi sonucundaki yaklaşık fonksiyonel kapsam: %40–45**

Bu fark, projenin **altyapı** tarafının (bağlantı, arayüz, ekran, konfigürasyon) büyük ölçüde
çalışır durumda olmasından; buna karşılık **uygulama alanı** tarafının (sensör çeşitliliği,
otomasyon, güvenlik, veri kalıcılığı) neredeyse tamamen eksik olmasından kaynaklanmaktadır.

| Alan | Yaklaşık kapsam |
|---|---|
| Sistem / Boot | %60 |
| Wi-Fi / Network | %70 |
| Sensörler | %30 (6 sensörden 2'si çalışıyor) |
| Aktüatörler | %35 (manuel var, otomatik yok) |
| Web Arayüzü | %50 |
| Display | %70 |
| Depolama / Configuration | %45 |
| Zaman / RTC | %40 |
| Güvenlik / Hata Yönetimi | %15 |
| FreeRTOS / Task | %50 |

**Not:**
Yukarıdaki oranlar, kodda **fiilen çalışan** işlevlere göre hesaplanmıştır. Yorum satırında,
header bildiriminde veya git geçmişinde geçen ancak çalışmayan hiçbir özellik tamamlanmış
sayılmamıştır. Bölüm 12'deki belirsizlikler netleştirildiğinde — özellikle sensör donanımı ve
SQLite kararı konusunda — bu oranlar yeniden değerlendirilmelidir.

---

*Bu doküman yalnızca mevcut durumu belgeler. Mimari tasarım, refactor planı ve hedef özellik
seti bir sonraki aşamada ayrı dokümanlarda ele alınacaktır.*
