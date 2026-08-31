# Kodlama Standardı ve Katman Kuralları

> Kaynak: TASK-003 · Dayanak: `ARCHITECTURE.md` §0, §1.2, §5.1
>
> **Bu doküman denetlenebilir olmak için yazıldı.** Her yasak desenin yanında
> **aranabilir bir iz** vardır; inceleme yapan kişi tahmin etmez, arar.
>
> Buradaki her madde `REQUIREMENTS.md`'de belgelenmiş **gerçek bir ihlale** dayanır.
> Uydurma kural yoktur.

---

## 1. Katman Bağımlılık Kuralları

```text
  L4  interfaces/   web/ · ui/
        |
  L3  domain/       SafetyMonitor · AutomationEngine · ActuatorManager
        |
  L2  services/     SensorService · NetworkService · StorageService · TimeService
        |
  L1  hal/          suruculer
        |
  L0  donanim

  +==========================================================+
  | core/  — hicbir katmana bagimli DEGIL, herkes ona bagimli |
  +==========================================================+
```

| # | Kural | İhlal nasıl aranır |
|---|---|---|
| **D1** | Bağımlılık yalnızca aşağı doğru akar (L4→L3→L2→L1) | `grep -rn '#include "\.\./' src/` — üst katmana giden yol var mı |
| **D2** | Alt katman üst katmanı çağıramaz | `hal/` içinde `services/`, `domain/`, `interfaces/` include'u |
| **D3** | `web/` ile `ui/` birbirini tanımaz | `grep -rn 'ui/' src/interfaces/web/` ve tersi |
| **D4** | Servisler birbirini doğrudan çağırmaz | `services/` içinde başka bir servis include'u |
| **D5** | `core/` hiçbir katmana bağımlı değil | `grep -rn '#include' src/core/` — yalnızca stdlib/FreeRTOS olmalı |
| **D6** | Sürücüler iş kuralı içermez | `hal/` içinde eşik, süre, kalibrasyon, cooldown sabiti |

### Include kuralları

- Her dosya **yalnızca kullandığı** arayüzü include eder.
- **Toplayıcı header yasaktır.** `Define.h` gibi her şeyi her şeye bağlayan bir
  header oluşturulmaz. Denetim: `src/` altında 20+ include içeren header var mı.
- Header'lar `#pragma once` kullanır.
- Sıralama: kendi header'ı → C/C++ stdlib → framework → proje header'ları.

---

## 2. Yasaklı Desenler

Her satır mevcut projede **gerçekten yapılmış** bir hatadır.

| # | Yasak | Nerede yapılmıştı | Neden yasak | Aranan iz |
|---|---|---|---|---|
| Y1 | Global mutable değişken | `currentIP`, `currentMAC`, `waterTemp`, `Sensor.WaterFlow` | Task'lar arası korumasız paylaşım (Kritik Problem 2) | `.cpp` üst seviyesinde `static` olmayan tanım |
| Y2 | Toplayıcı header | `Define.h` | Her şeyi her şeye bağlar, katman ihlalini gizler | 20+ `#include` içeren header |
| Y3 | Task içinde bloklayan bekleme | `MyWiFi::connect()` — `while(...) delay(50)` | Task'ı 5 sn kilitler, watchdog görmez (Kritik Problem 3) | `while` + `delay(` aynı fonksiyonda |
| Y4 | Sonsuz döngü ile hata yakalama | `setup()` OLED init | Sistemi tamamen kilitler (Kritik Problem 4) | `while (true)` / `while(1)` |
| Y5 | `setup()` içinden erken `return` | LittleFS mount hatası | Task'lar hiç oluşmaz, sistem yarı ölü kalır | `setup()` gövdesinde `return;` |
| Y6 | Başka task'ı askıya almak | `pauseWiFiMonitor()` | Kilit tutan task askıya alınırsa deadlock | `vTaskSuspend` / `vTaskResume` |
| Y7 | Servis/UI katmanından donanım çizimi | `Sensor::SensorValues()` OLED'e yazıyordu | Katman ihlali; UI donanım zamanlamasına bağlanır | `services/` içinde `oled` |
| Y8 | Bildirilip implement edilmemiş fonksiyon | `PhSensor()`, `handleLogin()`, `SaveIP()`, `GetIP()` | Sahte API; okuyanı yanıltır (P7) | header'da bildirim, `.cpp`'de tanım yok |
| Y9 | Elle string parse ile JSON | `indexOf` ile alan arama | Geçersiz mesajda tanımsız davranış | `indexOf` + tırnaklı alan adı |
| Y10 | Sıcak yolda `String` birleştirme | WebSocket yanıt üretimi | Heap parçalanması | döngü/callback içinde `String` toplama |
| Y11 | Dönüş değeri kontrol edilmeyen init | `_server.begin()` | Sessiz başarısızlık | `.begin()` sonucu atanmıyor |
| Y12 | Kullanılmayan struct alanı / pin | `Settings.IP`, `.MAC`, `LittleFSFormatted`, `PIN_CONFIRM_BUTTON` | Ölü kod yükü (P7) | tanımlı ama hiç okunmayan alan |
| Y13 | Sessiz varsayılana dönüş | `memset(&Setting, 0, ...)` | Sıfırlanmış güvenlik eşiği = kapalı koruma | log'suz `memset` / varsayılan atama |
| Y14 | Sabit/sahte gösterge değeri | Web'de sabit "pH 6.5", "EC 1.2" | Kullanıcıya yanlış bilgi | HTML/UI'da elle yazılmış ölçüm değeri |

---

## 3. Zorunlu Desenler

| # | Zorunlu | Gerekçe |
|---|---|---|
| Z1 | Her state parçasının **tek yazar** task'ı olur | ARCHITECTURE P1 |
| Z2 | Her donanım kaynağına **tek modül** dokunur, **tek task**'tan | ARCHITECTURE P2 |
| Z3 | Arayüzler yalnızca `CommandQueue.post()` ile dışarı yazar | ARCHITECTURE §13.2, §14.2 |
| Z4 | Süre ölçümleri **monotonik** zaman kullanır, duvar saati değil | NTP saati geriye alabilir; `maxRunTime` bozulur |
| Z5 | Süre karşılaştırması **fark alarak** yapılır | `millis()` ~49.7 günde taşar; sistem aylarca çalışacak |
| Z6 | Sensör değeri **kalite bilgisiyle birlikte** taşınır | Değer tek başına anlamsız; 0 hem ölçüm hem arıza olabilir |
| Z7 | Güvenlik sensörü okunamıyorsa **en kötü durum** varsayılır | Fail-safe (ARCHITECTURE §9.5) |
| Z8 | Watchdog beslemesi döngünün **en sonunda** | Ortada besleme, ilerlemeyi kanıtlamaz |
| Z9 | Task oluşturma `xTaskCreatePinnedToCore` ile | Çekirdek yalıtımı (ARCHITECTURE §6.2) |

---

## 4. Hata Yönetimi Konvansiyonu

### İstisna kullanılmaz

Arduino-ESP32'de C++ istisnaları varsayılan olarak kapalıdır ve maliyetlidir.

| Fonksiyon tipi | Dönüş |
|---|---|
| Değer döndürmeyen işlem | Hata kodu enum'u |
| Değer döndüren işlem | Değer/hata birliği (`Result` benzeri — TASK-004 tanımlar) |
| Sorgu (her zaman başarılı) | Doğrudan değer |

### Kurallar

- **Hata kodu makine tarafından karşılaştırılır; metin yalnızca insan içindir.**
  Karar mantığı asla hata metnine bakmaz.
- Hata kodu `{subsystem, code}` çiftidir (ARCHITECTURE §16.2).
- Dönüş değeri **kontrol edilmeden** bırakılmaz (Y11).
- Hiçbir hata sonsuz döngü (Y4) veya erken `return` (Y5) ile karşılanmaz.
- Her `ERROR` / `CRITICAL` kullanıcıya görünür olur — yalnızca seri porta yazmak
  yeterli değildir; sahada seri port yoktur.
- Varsayılana her düşüş **loglanır** (Y13).

---

## 5. Bellek Kuralları

| Kural | Gerekçe |
|---|---|
| Sıcak yolda dinamik ayırma yok (task döngüsü, ISR, callback) | Heap parçalanması ESP32'de ciddi sorundur |
| State ve config yapıları **POD** ve sabit boyutlu | `StateStore` snapshot deseni `memcpy` gerektirir |
| Diziler sabit üst sınırlı; `std::vector` sıcak yolda yok | Öngörülebilir bellek |
| String alanları sabit boyutlu tampon; `String` sınıfı state/config'te yok | Heap parçalanması |
| JSON tamponları önceden boyutlandırılır | AsyncTCP callback'inde ayırma riski |
| Büyük yapılar dönüş değeriyle değil, çağıranın verdiği tampona yazılır | Stack baskısı |

---

## 6. ISR Kuralları

ISR fonksiyonları `IRAM_ATTR` işaretlenir ve **yalnızca** olay üretir.

**ISR içinde YASAK:**

- Log çağrısı (`Serial`, `Diagnostics`)
- Dinamik bellek ayırma
- Bloklayan çağrı, mutex bekleme
- Dosya sistemi / NVS erişimi
- ADC okuma
- Uzun hesaplama

**ISR içinde SERBEST:** sayaç artırma, ISR-güvenli kuyruğa olay koyma.

Dekodlama, debounce yorumlama ve tüm karar mantığı **task bağlamındadır**.

---

## 7. Eşzamanlılık Kuralları

- Paylaşılan durum yalnızca `StateStore` üzerinden okunur/yazılır.
- Kritik bölge içinde: log, seri port, başka kilit alma, bloklama **yok**.
- Mutex beklemesi **sınırlı süreli** olur; sonsuz bekleme yasaktır.
- Task'lar birbirini askıya almaz (Y6).
- Periyodik döngüde `vTaskDelayUntil` tercih edilir — periyot kayması birikmez.

---

## 8. Biçim Kuralları

| Konu | Kural |
|---|---|
| Girinti | 4 boşluk, tab yok |
| Satır uzunluğu | ~100 karakter hedef |
| Süslü parantez | Fonksiyon ve blok için ayrı satırda |
| Dosya adı | Sınıf adıyla aynı: `SafetyMonitor.h` / `.cpp` |
| Tip adı | `PascalCase` |
| Fonksiyon / değişken | `camelCase` |
| Sabit / enum üyesi | `UPPER_SNAKE_CASE` |
| Üye değişken | `_` öneki veya soneki — dosya içinde tutarlı |
| Yorum dili | Türkçe serbest; **kod tanımlayıcıları İngilizce** |

> `.clang-format` bu aşamada eklenmedi: ortamda `clang-format` kurulu değil ve
> doğrulanamayan bir kural seti eklemek P7'ye aykırıdır. Araç kurulduğunda eklenir.

---

## 9. Task Dosyası Uyum Kuralları

- Yalnızca task'ın **Scope**'undaki iş yapılır.
- **Out of Scope**'a dokunulmaz.
- **Files** bölümünde listelenmeyen dosya değiştirilmez.
- Kapsam dışı sorun fark edilirse `docs/ISSUES.md`'ye kaydedilir, **çözülmez**.
- STEP 1 tasarım kararı yazılmadan kod yazılmaz.
