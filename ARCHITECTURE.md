# Topraksız Tarım Sistemi

## Mimari Tasarım (ARCHITECTURE)

> **Doküman amacı:** Bu dosya sistemin **NASIL** inşa edileceğini tanımlar.
> **NE** yapılacağı `REQUIREMENTS.md` dosyasındadır. Bu doküman kod içermez;
> modül sınırlarını, sahiplik kurallarını, veri akışını ve task mimarisini belirler.
> Implementation task'ları bu dokümandan türetilecektir.
>
> **Kapsam dışı:** Sulama algoritmasının detaylı matematiği, UI piksel tasarımı,
> sensör kalibrasyon katsayıları. Bunlar Implementation aşamasında netleşir.
>
> **Referans:** `REQUIREMENTS.md` (2026-08-30, branch `V.0.1.1`, commit `a2807e3`)
>
> **Not:** Mevcut kod bu tasarımda **referans değil, karşı-örnektir**. Hiçbir mevcut sınıf
> olduğu gibi taşınmaz. Mevcut sistemde çalışan davranışlar (menü gezinme, Wi-Fi kurulum akışı,
> sensör gösterimi) korunur; bu davranışların **implementasyonu** korunmaz.

---

## Bölüm 0 — Tasarım İlkeleri

Bu mimarinin tamamı 7 kurala dayanır. Sonraki her bölüm bu kuralların bir uygulamasıdır.

| # | Kural | Çözdüğü problem (REQUIREMENTS ref.) |
|---|---|---|
| **P1** | **Tek yazar (single writer).** Her state parçasının tam olarak bir sahip task'ı vardır. Diğerleri yalnızca okur. | Kritik Problem 2 — korumasız shared state |
| **P2** | **Donanıma tek kapı.** Her fiziksel kaynağa (OLED, röle GPIO, ADC, radyo) yalnızca onu sahiplenen modül dokunur. | Kritik Problem 2 — OLED'e 3 yerden erişim |
| **P3** | **Bloklama yasak.** Task döngüsünde `while(hazır değil)` veya `delay()` yoktur. Uzun işler durum makinesine bölünür. | Kritik Problem 3 — bloklayan Wi-Fi bağlantısı |
| **P4** | **Fail-degraded, fail-never-halt.** Hiçbir hata `while(true)` veya erken `return` ile sistemi durdurmaz. Sistem kısıtlı modda ayakta kalır. | Kritik Problem 4 — yarı ölü sistem |
| **P5** | **Tek doğruluk kaynağı cihazdadır.** Arayüzler (web, OLED) state üretmez, yalnızca cihazdan gelen state'i gösterir. | Kritik Problem 5 — frontend/backend tutarsızlığı |
| **P6** | **Güvenlik otomasyonun üstündedir.** Hiçbir otomasyon veya manuel komut, güvenlik kilidini geçemez. | REQUIREMENTS §4, §11-Critical |
| **P7** | **Yazılmayan kod yoktur.** Bildirilip implement edilmemiş fonksiyon, kullanılmayan alan, çağrılmayan sınıf mimariye girmez. | Kritik Problem 6 — ölü kod yükü |

### Kasıtlı sadelik sınırı

Bu bir ESP32 projesidir; kurumsal katman mimarisi değil. Aşağıdakiler **bilinçli olarak
kullanılmayacaktır**:

- Dependency injection container / service locator
- Her sensör için ayrı sınıf hiyerarşisi ve fabrika deseni
- Her özellik için ayrı FreeRTOS task'ı
- Generic event bus (tip silme, dinamik abonelik, heap tabanlı event nesneleri)
- Dinamik bellek üzerinde çalışan soyutlamalar (`std::function`, `std::vector` sıcak yolda)

Gerekçe: 320 KB DRAM, tek 240 MHz çekirdek çifti ve gerçek zamanlı güvenlik kısıtları
altında soyutlamanın maliyeti faydasından büyür. Soyutlama yalnızca **birden fazla somut
uygulaması olan** yerlerde kullanılır (sensörler, aktüatörler, storage backend'leri).

---

## Bölüm 1 — Katman Mimarisi

### 1.1 Katman diyagramı

```text
┌──────────────────────────────────────────────────────────────────────────┐
│  L4  PRESENTATION / INTERFACES                                           │
│                                                                          │
│   ┌────────────────────┐              ┌────────────────────┐             │
│   │   UiService        │              │   WebService       │             │
│   │  (OLED + Encoder)  │              │  (HTTP + WS + API) │             │
│   └────────────────────┘              └────────────────────┘             │
│      salt okuma: StateSnapshot   ·   yazma: yalnızca Command Queue       │
└──────────────────────────────────────────────────────────────────────────┘
                    │ snapshot ↑              │ command ↓
┌──────────────────────────────────────────────────────────────────────────┐
│  L3  DOMAIN / APPLICATION                     (tek karar merkezi)        │
│                                                                          │
│   ┌──────────────┐   ┌───────────────────┐   ┌────────────────────┐      │
│   │ SafetyMonitor│──▶│ AutomationEngine  │──▶│ ActuatorManager    │      │
│   │  (veto)      │   │ (schedule+rules)  │   │ (arbitration)      │      │
│   └──────────────┘   └───────────────────┘   └────────────────────┘      │
│                    ┌────────────────────┐                                │
│                    │ SystemSupervisor   │  (mod, sağlık, acil durum)     │
│                    └────────────────────┘                                │
└──────────────────────────────────────────────────────────────────────────┘
                    │                          │
┌──────────────────────────────────────────────────────────────────────────┐
│  L2  SERVICES                                (donanımı yöneten sahipler) │
│                                                                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌──────────────┐        │
│  │SensorService│ │NetworkSvc   │ │StorageSvc   │ │ TimeService  │        │
│  │ örnekleme + │ │ Wi-Fi FSM   │ │ config +    │ │ SNTP + TZ    │        │
│  │ filtre +    │ │ AP/STA      │ │ history     │ │              │        │
│  │ doğrulama   │ │ backoff     │ │             │ │              │        │
│  └─────────────┘ └─────────────┘ └─────────────┘ └──────────────┘        │
└──────────────────────────────────────────────────────────────────────────┘
                    │
┌──────────────────────────────────────────────────────────────────────────┐
│  L1  DRIVERS / HAL                            (ince, durumsuz sarmalayıcı)│
│                                                                          │
│  AdcInput · PulseCounter · RelayOutput · OledPanel · RotaryEncoder ·      │
│  ButtonInput · NvsStore · FileStore · WifiRadio                          │
└──────────────────────────────────────────────────────────────────────────┘
                    │
┌──────────────────────────────────────────────────────────────────────────┐
│  L0  HARDWARE   ESP32 · GPIO · ADC1 · PCNT · I2C · Wi-Fi radyo · Flash    │
└──────────────────────────────────────────────────────────────────────────┘

╔══════════════════════════════════════════════════════════════════════════╗
║  CROSS-CUTTING (her katmandan erişilebilir, hiçbirine bağımlı değil)      ║
║                                                                          ║
║   StateStore  ·  CommandQueue  ·  Diagnostics/Log  ·  Config  ·  Wdt     ║
╚══════════════════════════════════════════════════════════════════════════╝
```

### 1.2 Bağımlılık kuralları

| Kural | Açıklama |
|---|---|
| **D1** | Bağımlılık **yalnızca aşağı doğru** akar. L4 → L3 → L2 → L1 → L0. Ters yön yasaktır. |
| **D2** | Alt katman üst katmanı **çağıramaz**. Alt katman bilgi yukarı taşıyacaksa `StateStore`'a yazar veya event yayınlar. |
| **D3** | **L4 katmanları birbirini tanımaz.** UiService ile WebService arasında doğrudan bağ yoktur; ikisi de aynı snapshot'ı okur, aynı kuyruğa yazar. |
| **D4** | **L2 servisleri birbirini doğrudan çağırmaz.** SensorService, NetworkService'i tanımaz. Koordinasyon L3'ün işidir. |
| **D5** | Cross-cutting modüller (StateStore, Log, Config) **hiçbir katmana bağımlı değildir** — yalnızca POD veri tipleri ve standart kütüphane kullanır. Bu, döngüsel bağımlılığı yapısal olarak imkânsız kılar. |
| **D6** | L1 sürücüleri **durumsuzdur veya yalnızca donanım durumunu tutar**. İş kuralı, eşik, zamanlama içermez. |

> **Mevcut projeyle fark:** Bugün `Define.h` her şeyi her şeye include ediyor ve
> `Sensor.cpp` (L2 olması gereken) doğrudan OLED'e çiziyor (L4). Yeni yapıda `Define.h`
> benzeri bir "her şeyi toplayan header" **yoktur**; her modül yalnızca ihtiyaç duyduğu
> arayüzü include eder.

---

## Bölüm 2 — Modül Kataloğu

Her modül için: sorumluluk, sahip olduğu state, dışa açık arayüz, bağımlılıklar, iletişim şekli.

---

### 2.1 `StateStore` (Cross-cutting)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Sistemin tüm gözlemlenebilir durumunu tek bir yerde tutmak; tutarlı anlık görüntü (snapshot) dağıtmak. |
| **Sahip olduğu state** | `SystemState` (bkz. Bölüm 4) — tüm alt-state'lerin birleşimi |
| **Public arayüz** | `publishSensors(...)`, `publishNetwork(...)`, `publishActuators(...)`, `publishSystem(...)`, `snapshot()`, `version()` |
| **Bağımlılıklar** | Yok (yalnızca FreeRTOS mutex + POD tipler) |
| **İletişim** | Yazma: yalnızca ilgili alt-state'in sahibi task. Okuma: herkes, kopya alarak. |
| **Eşzamanlılık** | Tek `SemaphoreHandle_t` mutex. Kritik bölge yalnızca `memcpy` süresi kadar (< 10 µs). |

**Neden mutex, neden queue değil:** State okuma çok sık (UI 20 Hz, web 1 Hz, otomasyon 10 Hz)
ve okuyucuların **en güncel değeri** istemesi gerekir, geçmiş olayları değil. Queue burada
geriye birikmiş eski değerler üretir. Snapshot deseni okuyucuyu kilit dışında serbest bırakır.

---

### 2.2 `CommandQueue` (Cross-cutting)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Arayüzlerden (web, OLED, gelecekte MQTT) gelen niyet bildirimlerini domain katmanına sıralı ve güvenli iletmek. |
| **Sahip olduğu state** | Kuyruk içeriği (FreeRTOS queue, sabit boyutlu POD `Command` yapıları) |
| **Public arayüz** | `post(Command) → bool`, `receive(timeout) → optional<Command>` |
| **Bağımlılıklar** | Yok |
| **İletişim** | Çok yazar → tek okuyucu (`app_core` task). Kuyruk dolarsa **en eski değil, yeni komut reddedilir** ve çağırana `false` döner; arayüz kullanıcıya "meşgul" bildirir. |

**Neden queue:** Komutlar olaydır, durum değil. Sıra önemlidir, kaybolmamalıdır ve gönderen
sonucu beklememelidir (P3). Ayrıca kuyruk, AsyncTCP callback'i ile aktüatör GPIO'su arasına
**task sınırı** koyar — web callback'i asla doğrudan röle sürmez.

---

### 2.3 `Diagnostics` (Cross-cutting)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Seviyeli loglama, hata kodu kaydı, son N olayın halka tamponunda tutulması, sistem sağlık bayrakları. |
| **Sahip olduğu state** | Log halka tamponu (RAM, sabit boyut), aktif hata bayrakları bitmask'i, boot raporu |
| **Public arayüz** | `log(level, subsystem, code, msg)`, `raise(errorCode)`, `clear(errorCode)`, `activeFaults()`, `recent(n)` |
| **Bağımlılıklar** | Yok (opsiyonel: StorageService'e kalıcı yazma, gecikmeli) |
| **İletişim** | Herkes yazabilir (kısa mutex). Web ve OLED okur. |

---

### 2.4 `ConfigService` (Cross-cutting / L2 sınırı)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Kalıcı ayarların şemalı olarak yüklenmesi, doğrulanması, varsayılana düşülmesi ve kaydedilmesi. |
| **Sahip olduğu state** | `Config` yapısı (RAM'de tek kopya) + şema versiyonu |
| **Public arayüz** | `load()`, `get() → const Config&`, `update(section, values) → Result`, `resetToDefaults()`, `schemaVersion()` |
| **Bağımlılıklar** | `NvsStore` (L1) |
| **İletişim** | Okuma serbest (const referans, boot sonrası nadiren değişir). Yazma yalnızca `app_core` üzerinden, komut kuyruğu ile. |
| **Kural** | Her alan için **varsayılan değer ve geçerli aralık** şemada tanımlıdır. Geçersiz kayıt → varsayılan + WARNING log. Sessiz `memset(0)` **yoktur**. |

---

### 2.5 `SensorService` (L2)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Kayıtlı tüm sensörleri periyodik örneklemek; ham değeri kalibrasyon → filtre → doğrulama hattından geçirip `SensorState`'e yayınlamak. |
| **Sahip olduğu state** | Her sensör için: filtre geçmişi, son geçerli değer, hata sayacı, kalite durumu |
| **Public arayüz** | `begin(registry)`, `tick()` — task döngüsünden çağrılır |
| **Bağımlılıklar** | `AdcInput`, `PulseCounter` (L1), `ConfigService` (kalibrasyon), `StateStore` (yazma), `Diagnostics` |
| **İletişim** | `StateStore.publishSensors()` — **tek yazar**. Hiç kimseye doğrudan değer döndürmez. |
| **Yasak** | Ekrana çizmez, ağa bağlanmaz, aktüatör tetiklemez. (Mevcut `Sensor.cpp`'nin üçünü de yaptığı yer.) |

---

### 2.6 `ActuatorManager` (L3)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Aktüatörlerin **tek sahibi** olmak. Talep edilen durumu, kısıtlar (min/max çalışma, cooldown) ve güvenlik vetosu ile karşılaştırıp fiziksel çıkışa uygulamak. |
| **Sahip olduğu state** | Her aktüatör için: mevcut durum, talep edilen durum, son değişim zamanı, toplam çalışma süresi, kısıt sayaçları, kilit nedeni |
| **Public arayüz** | `request(actuatorId, desiredState, source) → RequestResult`, `apply()`, `forceAllOff(reason)` |
| **Bağımlılıklar** | `RelayOutput` (L1), `SafetyMonitor` (L3), `StateStore`, `Diagnostics` |
| **İletişim** | Yalnızca `app_core` task'ı içinden çağrılır. Web/UI buraya **doğrudan erişemez** — komut kuyruğundan geçmek zorundadır. |

---

### 2.7 `SafetyMonitor` (L3)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Güvenlik kilitlerini (interlock) her döngüde hesaplamak; herhangi bir aktüatör açma talebine **veto** hakkı kullanmak; acil durumu mandallamak (latch). |
| **Sahip olduğu state** | Aktif kilitler bitmask'i, acil durdurma mandalı, ihlal zaman damgaları, akış doğrulama sayacı |
| **Public arayüz** | `evaluate(snapshot)`, `permits(actuatorId) → Verdict{allow, reason}`, `emergencyStop(reason)`, `clearEmergency(operatorAck) → bool` |
| **Bağımlılıklar** | `StateStore` (okuma), `ConfigService` (eşikler), `Diagnostics` |
| **İletişim** | `AutomationEngine`'den **önce** çalışır. `ActuatorManager` her açma talebinde ona danışır. |
| **Kural** | Veto **her zaman** kazanır — manuel komut, otomasyon veya web isteği fark etmez (P6). |

---

### 2.8 `AutomationEngine` (L3)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Zamanlanmış (schedule) ve sensöre bağlı (threshold) kuralları değerlendirip **istenen aktüatör durumlarını** üretmek. |
| **Sahip olduğu state** | Aktif kural setinin çalışma durumu, son tetiklenme zamanları, aktif çevrim (cycle) bilgisi |
| **Public arayüz** | `evaluate(snapshot, now) → DesiredStates`, `setMode(mode)`, `reload(config)` |
| **Bağımlılıklar** | `ConfigService` (kurallar), `TimeService` (zaman geçerliliği), `StateStore` (okuma) |
| **İletişim** | Çıktısı `ActuatorManager.request()` çağrılarına dönüşür. Doğrudan GPIO'ya erişmez. |
| **Kural** | Zaman geçersizse (NTP yok, RTC yok) **schedule kuralları devre dışı**, threshold kuralları çalışmaya devam eder. |

---

### 2.9 `NetworkService` (L2)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Wi-Fi'yi bloklamayan bir durum makinesi olarak yönetmek: STA bağlantısı, backoff'lu yeniden deneme, AP'ye düşme, tarama, credential yönetimi. |
| **Sahip olduğu state** | FSM durumu, deneme sayacı, backoff zamanlayıcısı, tarama sonuç tamponu, aktif IP/RSSI |
| **Public arayüz** | `tick()`, `applyCredentials(ssid, pass)`, `requestScan()`, `scanResults()`, `forceApMode()`, `forgetCredentials()` |
| **Bağımlılıklar** | `WifiRadio` (L1), `ConfigService`, `StateStore`, `Diagnostics` |
| **İletişim** | Radyo event'leri → iç kuyruk → FSM. Sonuç `StateStore.publishNetwork()`. |
| **Yasak** | Ekrana çizmez. (Mevcut `MyWifi.cpp`'nin OLED'e doğrudan yazdığı yer.) Bloklayan bekleme yapmaz. |

---

### 2.10 `UiService` (L4)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Encoder/buton girdisini navigasyona çevirmek; snapshot'tan **ViewModel** üretip OLED'e çizmek. |
| **Sahip olduğu state** | Aktif ekran, menü imleci, düzenleme modu, son çizilen ViewModel (kirli-alan tespiti için) |
| **Public arayüz** | `tick()` |
| **Bağımlılıklar** | `OledPanel`, `RotaryEncoder`, `ButtonInput` (L1), `StateStore` (okuma), `CommandQueue` (yazma) |
| **İletişim** | Okuma: snapshot. Yazma: yalnızca `CommandQueue.post()`. |
| **Kural** | **OLED'e yalnızca bu task dokunur** (P2). Sensör okumaz, Wi-Fi'ye bağlanmaz, röle sürmez. |

---

### 2.11 `WebService` (L4)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Statik dosya servisi, REST API, WebSocket üzerinden durum yayını ve komut kabulü, kimlik doğrulama. |
| **Sahip olduğu state** | Oturum/token tablosu, WS istemci listesi, son yayınlanan state versiyonu |
| **Public arayüz** | `begin()`, `publishState(snapshot)`, `publishEvent(event)` |
| **Bağımlılıklar** | `FileStore` (L1), `StateStore` (okuma), `CommandQueue` (yazma), `Diagnostics` |
| **İletişim** | HTTP/WS callback'leri **yalnızca doğrula + kuyruğa koy + hemen yanıtla** yapar. |
| **Kural** | Callback içinde dosya sistemi taraması, uzun JSON üretimi veya bekleme yoktur (AsyncTCP task'ını bloklar). |

---

### 2.12 `StorageService` (L2)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Konfigürasyonun kalıcı yazılması ve geçmiş sensör verisinin halka dosyaya kaydedilmesi. |
| **Sahip olduğu state** | Halka tampon yazma indeksi, bekleyen yazma kuyruğu, flash yazma sayaçları |
| **Public arayüz** | `tick()`, `appendSample(record)`, `readRange(from, to, limit)`, `persistConfig(config)`, `stats()` |
| **Bağımlılıklar** | `NvsStore`, `FileStore` (L1), `Diagnostics` |
| **İletişim** | Düşük öncelikli task'ta çalışır; yazma isteklerini kuyruktan alır. Çağıran asla flash yazmasını beklemez. |

---

### 2.13 `TimeService` (L2)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | SNTP senkronizasyonu, POSIX TZ string ile yerel saat/DST, zaman geçerlilik bayrağı, monotonik uptime. |
| **Sahip olduğu state** | Senkronizasyon durumu, son senkron zamanı, TZ ayarı |
| **Public arayüz** | `tick()`, `isValid()`, `localTime()`, `uptimeMs()`, `setTimezone(tz)` |
| **Bağımlılıklar** | `ConfigService`, `StateStore`, `NetworkService` durumu (snapshot üzerinden, doğrudan değil) |
| **Kural** | `isValid() == false` iken schedule tabanlı otomasyon çalışmaz; UI "saat geçersiz" gösterir. Sessizce `00:00:00` döndürmez. |

---

### 2.14 `SystemSupervisor` (L3)

| Konu | Tanım |
|---|---|
| **Sorumluluk** | Sistem modunu (BOOTING / RUNNING / DEGRADED / SAFE / EMERGENCY) belirlemek, task heartbeat'lerini izlemek, kontrollü yeniden başlatmayı yönetmek. |
| **Sahip olduğu state** | Sistem modu, task heartbeat sayaçları, boot raporu, restart talebi |
| **Public arayüz** | `tick()`, `mode()`, `requestRestart(reason, delayMs)`, `heartbeat(taskId)` |
| **Bağımlılıklar** | `Diagnostics`, `StateStore`, `WatchdogGuard` |

---

### 2.15 L1 Sürücü Katmanı

| Sürücü | Sorumluluk | Kritik not |
|---|---|---|
| `AdcInput` | ADC1 kanal okuma, çoklu örnekleme, gerilime çevirme | **Yalnızca ADC1** (GPIO 32–39). Wi-Fi aktifken ADC2 kullanılamaz. |
| `PulseCounter` | Akış sensörü darbe sayımı, **donanımsal PCNT birimi** | ISR yerine PCNT: jitter yok, kayıp darbe yok, zaman penceresi doğru. |
| `RelayOutput` | Röle çıkışı, aktif-yüksek/aktif-düşük yapılandırması, boot'ta güvenli seviye | Röle modülü genellikle **aktif-düşük**tür; boot'ta yanlış seviye pompayı çalıştırabilir. Seviye yapılandırılabilir olmalıdır. |
| `OledPanel` | SSD1306 I2C çizim yüzeyi | Yalnızca UI task'ından çağrılır. |
| `RotaryEncoder` | Quadrature dekodlama, debounce, detent normalizasyonu | Adım/detent oranı **yapılandırılabilir tamsayı** olmalı (mevcut koddaki `1.5` double değeri gibi olmamalı). |
| `ButtonInput` | Debounce, kısa/uzun basış ayrımı, olay üretimi | Uzun basış ile "geri" / "onay" ayrımı UI'da değil burada üretilir. |
| `NvsStore` | Anahtar-değer kalıcı saklama, namespace ayrımı | ESP-IDF NVS; aşınma dengeleme ve atomik yazma yerleşiktir. |
| `FileStore` | LittleFS dosya erişimi, gzip'li varlık servisi | Mount hatası bir istisna değil, bir **durumdur**; üst katman bunu okur. |
| `WifiRadio` | Radyo başlatma, mod geçişi, tarama, event kaydı | Yalnızca `NetworkService` sahiptir. |

---

## Bölüm 3 — Veri Akışı

### 3.1 Sensör → Web (telemetri)

```text
 [ADC1 / PCNT]
      │  ham örnek
      ▼
 ┌─────────────────────────────────────────────┐
 │ SensorService  (task: io_sense, 250 ms)     │
 │  1. sample()        ham değer                │
 │  2. calibrate()     Config'ten katsayı       │
 │  3. filter()        EMA / medyan             │
 │  4. validate()      aralık + değişim hızı    │
 │  5. quality karar   OK/STALE/OUT_OF_RANGE/FAULT│
 └─────────────────────────────────────────────┘
      │ publishSensors()   ◀── TEK YAZAR
      ▼
 ┌─────────────────────────────────────────────┐
 │ StateStore.sensors   (mutex korumalı)       │
 └─────────────────────────────────────────────┘
      │ snapshot()  (kopya, kilitsiz kullanım)
      ▼
 ┌─────────────────────────────────────────────┐
 │ WebService.publishState()  (1 Hz veya değişimde)│
 │  → WebSocket: {"type":"state","v":1234,...} │
 └─────────────────────────────────────────────┘
      │
      ▼
 [ Tarayıcı: yalnızca gelen state'i render eder ]
```

**Kritik kural:** Sensör değeri hattın hiçbir noktasında "iyimser" olarak tahmin edilmez.
Kalite alanı (`quality`) değerin kendisiyle **birlikte** taşınır; arayüz `FAULT` durumunda
son değeri değil, "—" gösterir.

### 3.2 Sensör → Display

```text
 StateStore ──snapshot()──▶ UiService (task: ui, 50 ms)
                                │
                                │ buildViewModel(snapshot, activeScreen)
                                ▼
                          ┌──────────────┐
                          │  ViewModel   │  (saf veri: string, sayı, ikon kodu)
                          └──────────────┘
                                │ render(diff)
                                ▼
                            [OledPanel]
```

UI task'ı snapshot'ı **kendi periyodunda** okur; sensör task'ı ile senkronize değildir.
Kirli-alan (dirty region) tespiti ile yalnızca değişen bölgeler yeniden çizilir.

### 3.3 Web/Display komutu → Aktüatör

```text
 [Tarayıcı]  veya  [Encoder]
      │ POST /api/actuators/pump   veya   menü seçimi
      ▼
 WebService / UiService
      │  1. yetki kontrolü      (web)
      │  2. şema doğrulaması    (tip, aralık, enum)
      │  3. Command yapısı üret
      ▼
 CommandQueue.post(cmd) ───────────────┐   dolu ise → 503 / "MEŞGUL"
                                       ▼
 ┌───────────────────────────────────────────────────────────┐
 │ app_core task (100 ms)                                     │
 │                                                            │
 │   drain(CommandQueue)                                      │
 │        ▼                                                   │
 │   SafetyMonitor.evaluate(snapshot)   ◀── ÖNCE güvenlik      │
 │        ▼                                                   │
 │   AutomationEngine.evaluate()  (AUTO modda)                │
 │        ▼                                                   │
 │   ActuatorManager.request(id, desired, source)             │
 │        ├── permits()?  hayır → reddet + neden logla        │
 │        ├── minRunTime / cooldown ihlali? → ertele          │
 │        └── evet → RelayOutput.set()   ◀── TEK KAPI          │
 │        ▼                                                   │
 │   StateStore.publishActuators(gerçek durum)                │
 └───────────────────────────────────────────────────────────┘
```

### 3.4 Aktüatör durumu → Web + Display (geri besleme halkası)

```text
 ActuatorManager ──publishActuators()──▶ StateStore
                                            │
                        ┌───────────────────┴───────────────────┐
                        ▼                                       ▼
                 UiService (50 ms)                     WebService (değişimde + 1 Hz)
                        │                                       │
                    [OLED]                            WS: {"type":"state", ...}
                                                                │
                                                                ▼
                                                   [ Tarayıcı kartı GÜNCELLENİR ]
```

**Mevcut projedeki hatanın çözümü:** Tarayıcı butona basınca kart görünümünü **değiştirmez**;
yalnızca komutu gönderir ve `pending` (bekliyor) görsel durumuna geçer. Kart ancak cihazdan
gelen state ile gerçek durumuna geçer. Komut reddedilirse (`SAFETY_BLOCKED`, `COOLDOWN`)
sunucu bunu bildirir ve kart eski durumuna döner. Böylece **arayüzde görünen her durum,
cihazda gerçekten var olan durumdur** (P5).

---

## Bölüm 4 — State Yönetimi

### 4.1 State ağacı ve sahiplik

`SystemState` tek bir POD yapıdır (dinamik bellek içermez, `memcpy` ile kopyalanabilir).

| Alt-state | İçerik | **Tek yazar** | Okuyanlar |
|---|---|---|---|
| `system` | mod, uptime, boot raporu, sağlık bayrakları, aktif hata sayısı | `app_core` (SystemSupervisor) | UI, Web |
| `network` | FSM durumu, SSID, IP, RSSI, AP aktif mi, son hata, deneme sayısı | `net` task (NetworkService) | UI, Web, app_core |
| `sensors[]` | her sensör: değer, birim, kalite, zaman damgası, hata sayacı | `io_sense` (SensorService) | app_core, UI, Web |
| `actuators[]` | her aktüatör: durum, kaynak (MANUAL/AUTO/SAFETY), süre, kilit nedeni | `app_core` (ActuatorManager) | UI, Web |
| `safety` | aktif kilitler, acil durdurma mandalı, son ihlal nedeni | `app_core` (SafetyMonitor) | UI, Web |
| `automation` | mod (MANUAL/AUTO), aktif kural, sonraki tetikleme zamanı, override kalan süresi | `app_core` (AutomationEngine) | UI, Web |
| `time` | geçerlilik, yerel saat, son senkron | `net` task (TimeService) | UI, Web, app_core |

### 4.2 Versiyonlama ve değişim tespiti

`StateStore` monotonik artan bir `version` sayacı tutar. Her `publish*()` çağrısı sayacı
artırır.

| Kullanım | Fayda |
|---|---|
| WebService | Yayınlanan son versiyon ile karşılaştırıp **değişim yoksa WS trafiği üretmez** |
| UiService | Aynı ViewModel ise ekranı yeniden çizmez → I2C yükü ve titreme azalır |
| Frontend | `v` alanı ile eski/geç gelen paketi ayırt eder, sıra dışı paketi yok sayar |

### 4.3 Neden merkezi state, neden modül içi getter değil

Alternatif, her servisin `getTemperature()` gibi getter'lar sunmasıydı. Reddedilme nedenleri:

1. **Tutarsız görüntü:** UI beş ayrı getter çağırırsa, aralarında sensör güncellenirse ekranda
   birbiriyle tutarsız değerler oluşur. Snapshot atomik bir kesit verir.
2. **Ters bağımlılık:** Getter deseni UI'yi tüm servislere bağlar (D3 ihlali).
3. **Kilit yayılımı:** Her getter kendi kilidini alır; çağıran beş kilit alır. Snapshot tek
   kilit alır.

### 4.4 Oluşturulmayan state'ler (bilinçli karar)

- **UI state'i merkezi değildir.** Aktif ekran ve imleç yalnızca UI task'ını ilgilendirir,
  StateStore'a girmez.
- **Web oturum state'i merkezi değildir.** WebService'in iç meselesidir.
- **Ham sensör örnekleri merkezi değildir.** Yalnızca işlenmiş sonuç yayınlanır.

---

## Bölüm 5 — Event / Mesaj Mimarisi

Global değişkene doğrudan erişim yerine, **amaca göre farklı mekanizma** kullanılır.
Tek bir "her şeye uyan" event bus kurulmaz.

| Mekanizma | Nerede | Neden bu seçildi |
|---|---|---|
| **Mutex + Snapshot** (`StateStore`) | Durum paylaşımı | Okuyucu **en güncel** değeri ister, geçmişi değil. Kritik bölge çok kısa; kopya sonrası okuyucu serbest. Kuyruk burada eskimiş veri birikmesine yol açardı. |
| **Queue** (`CommandQueue`) | Arayüz → domain komutları | Komut bir **olaydır**: sıralı, kaybolmamalı, gönderen beklememeli. Ayrıca AsyncTCP callback'i ile GPIO arasına task sınırı koyar. |
| **Queue** (`NetworkEventQueue`) | Wi-Fi radyo event'leri → NetworkService FSM | Radyo event'i **ISR/sistem task bağlamında** gelir; orada iş yapılamaz. Kuyruk bağlam değiştirir. |
| **Queue** (`InputEventQueue`) | Encoder/buton ISR → UI task | ISR'de yalnızca olay üretilir; dekodlama ve çizim task bağlamına taşınır. |
| **Queue** (`StorageWriteQueue`) | Yazma istekleri → storage task | Flash yazma yavaş ve değişken süreli; çağıran asla beklememeli. |
| **EventGroup** (`SystemReadyBits`) | Boot senkronizasyonu | Task'lar "config yüklendi", "storage hazır", "ağ hazır" gibi **eşiklerin geçilmesini** bekler. Bu, sayılabilir olay değil, kalıcı koşuldur — EventGroup tam olarak bunun içindir. |
| **Doğrudan çağrı** | Aynı task içindeki L3 modülleri (Safety → Automation → ActuatorManager) | Hepsi `app_core` task'ının içinde, tek iş parçacığında sırayla çalışır. Araya kuyruk koymak gecikme ve karmaşıklık ekler, hiçbir güvenlik kazandırmaz. |

### 5.1 Kullanılmayacaklar ve gerekçesi

| Değerlendirilen | Karar | Gerekçe |
|---|---|---|
| Generic event bus (dinamik abonelik) | **Kullanılmayacak** | Heap tabanlı event nesneleri ve tip silme; ESP32'de bellek parçalanması ve teşhis zorluğu getirir. Akış sayısı (7) elle yönetilebilir düzeydedir. |
| Her state için ayrı mutex | **Kullanılmayacak** | Atomik kesit garantisini bozar ve kilit sırası kaynaklı deadlock riski doğurur. Tek kilit + kısa kritik bölge daha güvenlidir. |
| Task'lar arası doğrudan `vTaskSuspend` | **Yasak** | Mevcut projedeki `pauseWiFiMonitor()` deseni. Suspend edilen task kilit tutuyorsa deadlock olur. Yerine: FSM durumu ve kuyruk. |
| Paylaşılan global değişkenler | **Yasak** | `currentIP`, `currentMAC`, `Sensor.WaterFlow` gibi. Tamamı StateStore'a taşınır. |

---

## Bölüm 6 — FreeRTOS Task Mimarisi

### 6.1 Task tablosu

**5 task.** "Her özellik için bir task" yaklaşımı kullanılmaz; task ayrımı **zamanlama sınıfı
ve kaynak sahipliği** ile belirlenir.

| Task | Sorumluluk | Period | Prio | Stack | Çekirdek | Sahip olduğu kaynak | İletişim | WDT |
|---|---|---|---|---|---|---|---|---|
| `app_core` | Güvenlik → otomasyon → aktüatör; komut işleme; sistem modu | 100 ms | 4 (en yüksek) | 4 KB | 1 | **Röle GPIO'ları** | CommandQueue tüketici; StateStore yazar | Kayıtlı, kısa timeout |
| `io_sense` | Sensör örnekleme, filtreleme, doğrulama | 250 ms | 3 | 3 KB | 1 | **ADC1, PCNT** | StateStore yazar | Kayıtlı |
| `net` | Wi-Fi FSM, tarama, SNTP, bağlantı yönetimi | 100 ms (olay güdümlü) | 2 | 5 KB | 0 | **Wi-Fi radyosu** | NetworkEventQueue tüketici; StateStore yazar | Kayıtlı, uzun timeout |
| `ui` | Girdi işleme, ViewModel üretimi, OLED çizimi | 50 ms | 2 | 3.5 KB | 1 | **OLED (I2C)**, encoder, butonlar | InputEventQueue tüketici; CommandQueue yazar | Kayıtlı |
| `store` | Config kalıcılaştırma, geçmiş veri yazımı, log flush | Olay güdümlü (blokan kuyruk beklemesi) | 1 (en düşük) | 4 KB | 0 | **Flash (NVS + LittleFS)** | StorageWriteQueue tüketici | Kayıtlı, uzun timeout |
| *(AsyncTCP)* | HTTP/WS kabul ve callback'ler — **kütüphane task'ı** | — | (kütüphane) | (kütüphane) | 0 | TCP soketleri | CommandQueue yazar; StateStore okur | Kaydedilmez |
| *(Arduino loop)* | Kullanılmaz — boş bırakılır veya silinir | — | — | — | 1 | — | — | — |

### 6.2 Çekirdek dağılımı gerekçesi

```text
  CORE 0  (ağ ağırlıklı)              CORE 1  (kontrol ağırlıklı)
  ┌──────────────────────┐            ┌──────────────────────┐
  │ Wi-Fi / lwIP stack   │            │ app_core   (prio 4)  │  ← gerçek zamanlı
  │ AsyncTCP             │            │ io_sense   (prio 3)  │  ← deterministik
  │ net        (prio 2)  │            │ ui         (prio 2)  │
  │ store      (prio 1)  │            │                      │
  └──────────────────────┘            └──────────────────────┘
```

Gerekçe: ESP32'de Wi-Fi/lwIP yığını Core 0'da çalışır ve **öngörülemeyen uzun süreler**
harcayabilir. Güvenlik ve aktüatör kontrolü bu belirsizlikten yalıtılmalıdır. Kontrol ve UI
Core 1'e sabitlenir. Tüm task'lar `xTaskCreatePinnedToCore` ile oluşturulur; mevcut projedeki
gibi çekirdeğe bırakılmaz.

### 6.3 Öncelik gerekçesi

`app_core` en yüksek önceliğe sahiptir çünkü **güvenlik kararları gecikmemelidir**.
Mevcut projede en yüksek öncelik loglama task'ındaydı (`Task_SensorLogger`, prio 4) —
bu ters çevrilmiştir.

### 6.4 Task'a dönüşmeyen işler

| İş | Neden ayrı task değil |
|---|---|
| Wi-Fi LED blink | `ui` task'ının 50 ms periyodu içinde bir sayaçla yapılır. 2 KB stack + context switch bir LED için israftır. |
| Telemetri yayını | `net` task'ının döngüsünde 1 Hz kontrolüyle yapılır. |
| Watchdog besleme | Her task kendi döngü sonunda besler; merkezi bir "watchdog task" **anti-pattern**'dir (asıl task kilitlenmişken beslemeye devam eder). |
| Otomasyon değerlendirmesi | `app_core` içinde sıralı adımdır; ayrı task güvenlik ile otomasyon arasına yarış durumu sokar. |

### 6.5 Watchdog davranışı

| Konu | Tasarım |
|---|---|
| **Kurulum sırası** | TWDT **boot'un ilk adımında**, hiçbir task oluşturulmadan önce yapılandırılır. (Mevcut projedeki ters sıra düzeltilir.) |
| **Kayıt** | Her denetlenen task **kendi başlangıcında** kendini kaydeder. |
| **Besleme** | Yalnızca döngünün **sonunda**, tüm iş bittikten sonra. Döngü ortasında besleme yasaktır. |
| **Kapsam** | 5 task'ın **tamamı** kayıtlıdır. (Mevcut projede `Task_SensorLogger` kayıtsızdı.) |
| **Timeout** | Kontrol task'ları için kısa (~5 s), ağ ve storage için uzun (~15 s). Tek bir global değer yerine görev sınıfına göre. |
| **Panik davranışı** | TWDT panic açık → donanımsal reset. Reset nedeni boot'ta okunur, `Diagnostics`'e CRITICAL olarak kaydedilir ve web'de gösterilir. |
| **Uygulama katmanı denetimi** | Ek olarak her task `SystemSupervisor.heartbeat(taskId)` çağırır. Bir task'ın heartbeat'i 3 periyot boyunca artmazsa donanım WDT'sini beklemeden DEGRADED moda geçilir ve aktüatörler güvenli duruma alınır. |

---

## Bölüm 7 — Boot / Initialization Mimarisi

### 7.1 Aşamalı boot

Mevcut projede init hatası `return` veya `while(true)` ile sonuçlanıyordu. Yeni akışta
**her aşama bir sonuç döndürür ve boot raporuna yazılır; hiçbir aşama boot'u durdurmaz** (P4).

```text
 ┌───────────────────────────────────────────────────────────────────┐
 │ AŞAMA 0  Reset nedenini oku, TWDT'yi yapılandır                   │  ZORUNLU
 ├───────────────────────────────────────────────────────────────────┤
 │ AŞAMA 1  GPIO güvenli seviyeye al  (TÜM RÖLELER KAPALI)           │  ZORUNLU
 ├───────────────────────────────────────────────────────────────────┤
 │ AŞAMA 2  Diagnostics + StateStore + kuyruklar                     │  ZORUNLU
 ├───────────────────────────────────────────────────────────────────┤
 │ AŞAMA 3  NVS → Config yükle          başarısız → VARSAYILAN + WARN│
 │ AŞAMA 4  LittleFS mount              başarısız → web statik YOK   │
 │ AŞAMA 5  I2C + OLED                  başarısız → DISPLAY YOK      │
 │ AŞAMA 6  ADC + PCNT                  başarısız → SENSÖR kalitesi  │
 │ AŞAMA 7  Wi-Fi radyo başlat          başarısız → OFFLINE mod      │
 ├───────────────────────────────────────────────────────────────────┤
 │ AŞAMA 8  Task'ları oluştur (pinned)                               │  ZORUNLU
 │ AŞAMA 9  Boot raporunu yayınla, modu belirle                      │
 └───────────────────────────────────────────────────────────────────┘
```

### 7.2 Boot sonucu → sistem modu

| Koşul | Mod | Davranış |
|---|---|---|
| Tüm aşamalar başarılı | `RUNNING` | Tam işlevsellik |
| Kritik olmayan aşama başarısız (FS, OLED, Wi-Fi) | `DEGRADED` | Kalan işlevler çalışır; eksik olan web ve OLED'de açıkça bildirilir |
| Config bozuk **ve** güvenlik sensörleri okunamıyor | `SAFE` | Tüm aktüatörler kapalı ve kilitli; yalnızca teşhis ve kurulum arayüzü açık |
| Güvenlik ihlali mandallandı | `EMERGENCY` | Aktüatörler kapalı; açık operatör onayı olmadan çıkılmaz |

**Kural:** OLED başlatılamadıysa sistem **çalışmaya devam eder** — web arayüzü ve otomasyon
etkilenmez. LittleFS mount edilemediyse **task'lar yine de oluşturulur**; yalnızca statik
web dosyaları servis edilemez ve bu durum API'den bildirilir.

---

## Bölüm 8 — Network (Wi-Fi) Mimarisi

### 8.1 Bloklamayan durum makinesi

```text
                       ┌──────────────┐
                       │     BOOT     │
                       └──────┬───────┘
                              │ credential var?
                 hayır ┌──────┴──────┐ evet
                       ▼             ▼
              ┌─────────────┐  ┌──────────────┐
              │  AP_ONLY    │  │  CONNECTING  │◀────────┐
              │ (kurulum)   │  │  (timeout T) │         │
              └──────┬──────┘  └──────┬───────┘         │
                     │                │                 │
                     │      ┌─────────┴─────────┐       │
                     │  başarı              başarısız    │
                     │      ▼                 ▼         │
                     │ ┌───────────┐   ┌──────────────┐ │
                     │ │ CONNECTED │   │   BACKOFF    │─┘
                     │ └─────┬─────┘   │ 1→2→4→8→…→60s│
                     │       │ kopma   └──────┬───────┘
                     │       └────────────────┘
                     │                        │ N deneme aşıldı
                     │                        ▼
                     │              ┌────────────────────┐
                     └─────────────▶│   AP_FALLBACK      │
                                    │ (AP açık, STA arka │
                                    │  planda denemeye   │
                                    │  devam eder)       │
                                    └────────────────────┘
```

### 8.2 Tasarım kararları

| Konu | Karar | Mevcut projedeki problem |
|---|---|---|
| **Bloklama** | `WiFi.begin()` çağrılır ve **hemen dönülür**. Bağlantı sonucu event ile gelir. FSM'de bekleme yoktur. | `connect()` içinde 5 sn'ye kadar `while + delay(50)` — task kilitleniyordu |
| **Backoff** | Üstel: 1, 2, 4, 8, 16, 20 s (tavan). Rastgele ±%20 jitter. Tavan 60 sn'den 20 sn'ye indirildi: backoff'un işi ulaşılamayan bir ağı sonsuz denememektir, kullanıcıyı bekletmek değil — ağ döndüğünde en kötü ihtimalle 20 sn'de geri bağlanılır. | Sabit 1 sn'de bir sonsuz deneme — radyo ve güç israfı |
| **AP fallback** | Kalıcı kopmada (**45 sn**) AP açılır, **STA denemesi arka planda sürer** (AP_STA). Ağ geri gelince otomatik STA'ya döner. Eşik 90 sn'ydi: internet gittiğinde cihaza ulaşılamayan bir buçuk dakika demekti. | Yalnızca boot'ta bir kez AP'ye düşülüyordu; çalışma sırasında kopma kalıcıydı |
| **Event işleme** | Radyo event'i → `NetworkEventQueue` → FSM. Event handler'da iş yapılmaz. | Event handler içinde EEPROM yazma yapılıyordu |
| **Disconnect nedeni** | `reason` kodu saklanır ve ayrıştırılır: yanlış şifre → **yeniden denemez**, kullanıcıya bildirir. AP bulunamadı → denemeye devam eder. | Disconnect event'i hiç işlenmiyordu; yanlış şifreyle sonsuz deneme |
| **Tarama** | Asenkron; sonuç tamponda tutulur ve zaman damgalıdır. API "hazır değil" durumunu **açık bir durum alanıyla** döndürür. | 202 ara-durumu frontend'de işlenmiyordu → ilk tarama hep hata |
| **Static IP** | Config'te ayrı bir `network.mode` alanı (DHCP / STATIC) ile yönetilir. AP/STA modundan **tamamen bağımsızdır**. | `_useDHCP` yanlışlıkla `IsServerMode`'dan türetiliyordu |
| **Credential saklama** | NVS'te ayrı namespace. Şifre API yanıtlarında **asla** döndürülmez, OLED'de **asla** gösterilmez. | EEPROM'da düz metin + OLED'de açıkça gösteriliyordu |
| **Güç yönetimi** | Modem sleep kapalı (web yanıt süresi için). Bu bir yapılandırma kararıdır, kodda sabit değildir. | Sabit kodlanmıştı |

### 8.3 Ağ durumu bilgisi

`network` alt-state'i şunları içerir: FSM durumu, SSID, IP, gateway, RSSI, AP aktif mi,
bağlantı süresi, son kopma nedeni, deneme sayısı, sonraki deneme zamanı. RSSI periyodik
okunur ve hem OLED'de hem web'de sinyal göstergesi olarak sunulur.

### 8.4 İlk kurulumun bitişi: kontrollü yeniden başlatma

Kurulum AP'sinde Wi-Fi bilgisi girilip bağlantı kurulduğunda kurulum **bitmiş**
sayılır ve cihaz kendini kontrollü biçimde yeniden başlatır. Eski davranışta
böyle bir bitiş noktası yoktu: cihaz `AP_STA` modunda kalıyor, kurulum AP'si
linger süresince açık duruyor ve kullanıcı iki ağ arasında, cihazın yeni
adresini bilmeden ortada kalıyordu.

| Konu | Karar | Gerekçe |
|---|---|---|
| **Tetikleyici** | Yalnızca **kurulum oturumunda** ilk `STA_GOT_IP`. Oturum, AP'nin *kimlik bilgisi olmadığı için* açılmasıyla başlar: ilk açılış veya "ağı unut". | AP fallback'te kayıtlı ve daha önce çalışmış bir ağ vardır; her dönüşte reset atmak, sinyali zayıf bir kurulumu yeniden başlatma döngüsüne sokar. |
| **Nefes payı** | Reset'ten önce 4 sn beklenir; bu sürede AP açık TUTULUR ve durum hemen yayınlanır. | Kullanıcının telefonu hâlâ kurulum AP'sindedir. "Bağlandı, yeni adres X" mesajını GÖRMESİ gerekir; anında reset, tarayıcıda yalnızca kopmuş bir bağlantı bırakır. |
| **Kayıt garantisi** | `config::isDirty()` temizlenene kadar reset ERTELENİR; `store` task'ından anında yazma istenir. Üst sınır 20 sn. | `ConfigService` yazmayı 2 sn geciktirir. Yazılmadan reset atmak SSID'yi siler ve cihaz kurulum AP'sine geri döner. |
| **Sınır aşılırsa** | Yeniden başlatma **iptal** edilir, ERROR loglanır. | Cihaz bağlı ve erişilebilirdir; kimlik bilgisini kaybetme riskine girmektense AP'nin linger ile kapanmasını beklemek yeğdir. |
| **Reset yolu** | `net` doğrudan reset ATMAZ; `SYSTEM_RESTART` komutunu kuyruğa koyar, işi `app_core` + `SystemSupervisor` yapar. | Aktüatörlerin önce güvenli duruma alınması gerekir (§14.3). Radyo task'ının röle durumunu bilme yetkisi yoktur. |
| **Kullanıcıya bildirim** | `network.provisioning`, `network.setupReboot`, `network.rebootIn` yayınlanır; web arayüzü devir teslim ekranını, OLED "KURULUM TAMAM + adres" ekranını gösterir. | Habersiz yeniden başlayan bir cihaz, bozulmuş bir cihazdan ayırt edilemez. Beklenen kopma için "bağlantı kesildi" alarmı da gösterilmez. |

Yeniden başlayan cihaz `BOOT → CONNECTING → CONNECTED` yolunu izler: AP hiç
açılmaz, radyo saf `STA` olur ve kurulum tanımlı bir noktada biter.

---

## Bölüm 9 — Sensör Mimarisi

### 9.1 Ortak soyutlama gerekli mi?

**Evet, ancak sığ olarak.** Altı sensörden dördü (sıcaklık, pH, EC, seviye) **analog okuma +
kalibrasyon + filtre** hattını paylaşır. Ortak hat kurmamak bu kodu dört kez tekrarlamak
demektir. Buna karşılık derin bir sınıf hiyerarşisi de gereksizdir.

**Karar:** Tek bir `ISensor` arayüzü (`sample()`, `descriptor()`) + sensörlerin **derleme
zamanı sabit bir tabloda** (registry) tanımlanması. Sanal fonksiyon sayısı ikiyle sınırlıdır;
dinamik kayıt, fabrika ve heap kullanımı yoktur.

### 9.2 İşleme hattı

```text
  sample()          calibrate()        filter()         validate()        publish()
 ┌─────────┐      ┌────────────┐    ┌───────────┐    ┌────────────┐    ┌──────────┐
 │ ham ADC │─────▶│ Config'ten │───▶│ EMA veya  │───▶│ aralık +   │───▶│StateStore│
 │ / darbe │      │ katsayı,   │    │ medyan-3  │    │ değişim    │    │ + kalite │
 │ sayısı  │      │ ofset,eğri │    │           │    │ hızı       │    │          │
 └─────────┘      └────────────┘    └───────────┘    └────────────┘    └──────────┘
                                                            │
                                                            ▼
                                                    kalite ataması:
                                                    OK / STALE /
                                                    OUT_OF_RANGE / FAULT
```

### 9.3 Sensör kataloğu

| Sensör | Tip | Arayüz | Örnekleme | Kalibrasyon | Güvenlik rolü |
|---|---|---|---|---|---|
| **Su sıcaklığı** | Analog (NTC) veya dijital (DS18B20) | ADC1 / 1-Wire | 1 s | Beta + seri direnç, **veya** doğrudan dijital | Bilgi amaçlı |
| **Su akışı** | Darbe (YF-S401) | **PCNT** | 1 s pencere | Darbe/litre katsayısı | **Akış doğrulama** (kritik) |
| **pH** | Analog | ADC1 | 2 s | 2 nokta (pH 4 / 7) | Bilgi + otomasyon eşiği |
| **EC / Besin** | Analog | ADC1 | 2 s | 1–2 nokta + sıcaklık telafisi | Bilgi + otomasyon eşiği |
| **Nem** | Dijital (DHT22 / SHT3x) | 1-Wire / I2C | 5 s | Fabrika | Bilgi amaçlı |
| **Su seviyesi** | Şamandıra (dijital) **veya** analog/ultrasonik | GPIO / ADC1 | 500 ms | Eşik / mesafe eğrisi | **Pompa kilidi (en kritik)** |

> **Tasarım önerisi — su seviyesi:** Güvenlik zincirinin temeli olduğu için **iki dijital
> şamandıra** (düşük + kritik-düşük) önerilir. Analog seviye sensörü tek noktada arızalanırsa
> "dolu" okuyabilir; iki bağımsız şamandıra bu tek nokta hatasını ortadan kaldırır. Bu bir
> donanım kararıdır ve Bölüm 17'de açık madde olarak listelenmiştir.

### 9.4 Okuma / işleme / gösterim ayrımı

| Katman | Yapar | Yapmaz |
|---|---|---|
| `AdcInput` / `PulseCounter` (L1) | Ham değer üretir | Anlamlandırmaz, birim bilmez |
| `SensorService` (L2) | Kalibre eder, filtreler, doğrular, yayınlar | **Ekrana çizmez**, karar vermez |
| `AutomationEngine` / `SafetyMonitor` (L3) | Değerlerle karar üretir | Sensör okumaz |
| `UiService` / `WebService` (L4) | Biçimlendirip gösterir | Sensör okumaz, filtre uygulamaz |

Bu ayrım, mevcut `Sensor.cpp`'nin (okuma + OLED çizimi aynı fonksiyonda) doğrudan çözümüdür.

### 9.5 Sensör hata yönetimi

| Durum | Tespit | Sonuç |
|---|---|---|
| Kısa devre / kopuk | ADC uçta sabit (0 veya tam ölçek) | `FAULT`, değer gösterilmez |
| Aralık dışı | Konfigüre edilebilir min/max | `OUT_OF_RANGE`, otomasyonda kullanılmaz |
| Donmuş değer | N örnek boyunca hiç değişim yok | `STALE`, WARNING |
| Fiziksel olmayan sıçrama | Değişim hızı sınırı aşıldı | Örnek atılır, sayaç artar |
| Kalite `FAULT` **ve** güvenlik sensörü | — | İlgili aktüatör **kilitlenir** (fail-safe) |

**Kural:** Güvenlik kararı veren bir sensör okunamıyorsa, sistem "sorun yok" varsaymaz;
**en kötü durumu** varsayar ve ilgili aktüatörü kilitler.

---

## Bölüm 10 — Aktüatör Mimarisi

### 10.1 Katmanlama

```text
  ┌────────────────────────────────────────────────────────┐
  │ Actuator (domain nesnesi)                              │
  │  · mantıksal kimlik (WATER_PUMP, AIR_PUMP, AUX)        │
  │  · kısıtlar: minRunTime, maxRunTime, cooldown          │
  │  · sayaçlar: toplam süre, çevrim sayısı, son değişim   │
  │  · kaynak: MANUAL / AUTO / SAFETY                      │
  └───────────────────────┬────────────────────────────────┘
                          │
  ┌───────────────────────▼────────────────────────────────┐
  │ RelayOutput (L1 sürücü)                                │
  │  · GPIO pini, aktif seviye (HIGH/LOW), boot seviyesi   │
  │  · yalnızca set(bool) — hiçbir iş kuralı yok           │
  └────────────────────────────────────────────────────────┘
```

Mantıksal aktüatör ile fiziksel röle ayrıdır. Böylece "pompa 2 röle sürüyor" veya
"röle aktif-düşük" gibi donanım değişiklikleri domain katmanını etkilemez.

### 10.2 Aktüatör kataloğu

| Aktüatör | Fiziksel | Kısıtlar | Güvenlik bağımlılığı |
|---|---|---|---|
| `WATER_PUMP` | Röle 1 | minRun, maxRun, cooldown | Su seviyesi + akış doğrulama + acil durdurma |
| `AIR_PUMP` | Röle 2 | minRun (kısa çevrim koruması) | Acil durdurma |
| `AUX` (opsiyonel) | Röle 3+ | Yapılandırılabilir | Acil durdurma |

> **Not:** `REQUIREMENTS.md` §12'de RELAY2'nin gerçekte neye bağlı olduğu açık bir sorudur.
> Mimari, mantıksal kimlik ile fiziksel pini ayırdığı için bu netleştiğinde yalnızca
> yapılandırma değişir, kod değişmez.

### 10.3 Manuel / otomatik ayrımı ve tahkim (arbitration)

Aynı aktüatör için üç kaynak talep üretebilir. Öncelik sırası **kesin**dir:

```text
   1. SAFETY      ── veto / zorla kapat        (her zaman kazanır)
   2. MANUAL      ── operatör override         (süreli)
   3. AUTOMATION  ── kural motoru              (varsayılan)
```

| Mod | Davranış |
|---|---|
| `MANUAL` | Otomasyon kuralları değerlendirilmez. Aktüatörler yalnızca komutla değişir. Güvenlik yine geçerlidir. |
| `AUTO` | Otomasyon kuralları aktüatörleri sürer. |
| `AUTO + MANUAL_OVERRIDE` | Operatör AUTO modda manuel komut verirse, **süreli** bir override başlar (varsayılan: yapılandırılabilir, örn. 15 dk). Süre dolunca otomasyon kontrolü geri alır. |

**Neden süreli override:** Kalıcı override, operatörün unuttuğu bir manuel komutun otomasyonu
süresiz devre dışı bırakmasına yol açar — hidroponik sistemde bitki kaybı demektir.
Süreli override hem esneklik hem güvenli varsayılan sağlar. Kalan süre state'te taşınır ve
arayüzde gösterilir.

### 10.4 Komut sonucu sözleşmesi

Her `request()` çağrısı açık bir sonuç döndürür; sessiz başarısızlık yoktur.

| Sonuç | Anlamı |
|---|---|
| `ACCEPTED` | Uygulandı |
| `REJECTED_SAFETY` | Güvenlik kilidi engelledi (neden alanıyla birlikte) |
| `DEFERRED_MIN_RUNTIME` | Minimum çalışma süresi dolmadı, şimdi kapatılamaz |
| `DEFERRED_COOLDOWN` | Bekleme süresi dolmadı, şimdi açılamaz |
| `REJECTED_MODE` | Mevcut modda bu komut geçersiz |
| `NO_CHANGE` | Zaten istenen durumda |

Bu sonuç hem API yanıtına hem WS olayına hem de OLED geri bildirimine dönüşür.

---

## Bölüm 11 — Otomasyon Mimarisi

> Bu bölüm **yapıyı** tanımlar; sulama algoritmasının kendisi Implementation aşamasında
> yazılacaktır. Amaç, algoritma değiştiğinde mimarinin değişmemesini sağlamaktır.

### 11.1 Değerlendirme döngüsü (`app_core`, 100 ms)

```text
  ┌──────────────────────────────────────────────────────────────┐
  │ 1. snapshot = StateStore.snapshot()                          │
  ├──────────────────────────────────────────────────────────────┤
  │ 2. commands = CommandQueue.drainAll()      (sınırlı sayıda)   │
  ├──────────────────────────────────────────────────────────────┤
  │ 3. SafetyMonitor.evaluate(snapshot)     ◀── HER ZAMAN ÖNCE    │
  │      · kilitleri hesapla                                     │
  │      · ihlal varsa forceAllOff() + mandalla                  │
  ├──────────────────────────────────────────────────────────────┤
  │ 4. mod == AUTO ?                                             │
  │      evet → desired = AutomationEngine.evaluate(snapshot,now) │
  │      hayır → desired = mevcut durum                          │
  ├──────────────────────────────────────────────────────────────┤
  │ 5. komutları uygula (override / mod değişimi / config)       │
  ├──────────────────────────────────────────────────────────────┤
  │ 6. ActuatorManager.apply(desired)                            │
  │      · her talep için SafetyMonitor.permits() sorgusu         │
  │      · kısıt kontrolü (min/max/cooldown)                     │
  │      · fiziksel çıkışa yaz                                   │
  ├──────────────────────────────────────────────────────────────┤
  │ 7. StateStore.publish*()  · Supervisor.heartbeat()  · WDT     │
  └──────────────────────────────────────────────────────────────┘
```

### 11.2 Kural modeli

İki kural tipi, ortak bir değerlendirme sözleşmesiyle:

| Tip | Tetikleyici | Örnek kullanım | Zaman geçerliliği gerekir mi |
|---|---|---|---|
| `ScheduleRule` | Zaman penceresi / periyodik çevrim (ON süresi + OFF süresi) | "Her 2 saatte 15 dk sula", "07:00–19:00 arası aktif" | **Evet** |
| `ThresholdRule` | Sensör değeri + histerezis | "EC < 1.0 ise besin pompasını çalıştır" | Hayır |

Her kural yapısı şunları taşır: hedef aktüatör, etkinlik bayrağı, öncelik, histerezis,
minimum tetikleme aralığı. Kurallar **konfigürasyon verisidir** — kod değil. Bu sayede yeni
bir sulama profili eklemek firmware değişikliği gerektirmez.

### 11.3 Mimaride yeri belirlenen otomasyon kavramları

| Kavram | Nerede yaşar | Not |
|---|---|---|
| `schedule` | `AutomationEngine` + Config | Zaman geçersizse pas geçilir |
| `sensor based control` | `AutomationEngine` (ThresholdRule) | Histerezis zorunlu (röle çırpınmasını önler) |
| `thresholds` | Config (kalıcı, doğrulanmış aralık) | Web'den düzenlenebilir |
| `safety conditions` | **`SafetyMonitor`** — otomasyonda değil | Otomasyon kapalıyken de geçerli |
| `minimum run time` | `ActuatorManager` kısıtı | Kural motoru bunu bilmek zorunda değil |
| `maximum run time` | `ActuatorManager` + `SafetyMonitor` | Aşımda kapatma **ve** hata kaydı |
| `cooldown` | `ActuatorManager` kısıtı | Kompresör/pompa ömrü koruması |
| `water level protection` | `SafetyMonitor` kilidi | Otomasyondan bağımsız, vetoludur |
| `flow verification` | `SafetyMonitor` gecikmeli kontrol | Pompa ON + N sn sonra akış yoksa → arıza |
| `emergency stop` | `SafetyMonitor` mandalı + `SystemSupervisor` modu | Manuel onayla temizlenir |

### 11.4 Otomasyonun bilmediği şeyler (bilinçli)

`AutomationEngine`, güvenlik kilitlerini, aktüatör kısıtlarını ve donanımı **bilmez**.
Yalnızca "şu aktüatörün açık olmasını istiyorum" der. Bu ayrım sayesinde kural motoru
karmaşıklaşsa bile güvenlik mantığı sabit, denetlenebilir ve test edilebilir kalır.

---

## Bölüm 12 — Güvenlik (Safety) Mimarisi

### 12.1 Güvenlik zinciri

```text
   ┌──────────────────────────────────────────────────────────────┐
   │  KATMAN 1 — ÖN KOŞULLAR   (pompa açılmadan önce kontrol)     │
   │                                                              │
   │    Su seviyesi yeterli mi?          hayır → İZİN YOK         │
   │    Seviye sensörü sağlıklı mı?      hayır → İZİN YOK         │
   │    Acil durdurma mandalı açık mı?   evet  → İZİN YOK         │
   │    Cooldown doldu mu?               hayır → ERTELE           │
   └───────────────────────────┬──────────────────────────────────┘
                               │ izin verildi
   ┌───────────────────────────▼──────────────────────────────────┐
   │  KATMAN 2 — ÇALIŞMA SIRASINDA İZLEME                         │
   │                                                              │
   │    Akış doğrulama:  ON + T saniye sonra akış < eşik          │
   │                     → KURU ÇALIŞMA ARIZASI → KAPAT + MANDAL  │
   │                                                              │
   │    Maksimum süre:   çalışma > maxRunTime                     │
   │                     → KAPAT + WARNING                        │
   │                                                              │
   │    Seviye düşüşü:   çalışırken seviye kritik altına indi     │
   │                     → ANINDA KAPAT                           │
   └───────────────────────────┬──────────────────────────────────┘
                               │ ihlal
   ┌───────────────────────────▼──────────────────────────────────┐
   │  KATMAN 3 — ACİL DURUM (mandallı / latching)                 │
   │                                                              │
   │    Tüm aktüatörler KAPALI ve KİLİTLİ                         │
   │    Sistem modu = EMERGENCY                                   │
   │    Neden kaydedilir, web + OLED'de kalıcı gösterilir         │
   │    Otomatik çıkış YOK — açık operatör onayı gerekir          │
   └──────────────────────────────────────────────────────────────┘
```

### 12.2 Güvenlik ilkeleri

| İlke | Uygulama |
|---|---|
| **Fail-safe varsayılan** | Bilinmeyen/okunamayan durum = tehlikeli durum varsayılır. Sensör arızalıysa pompa çalışmaz. |
| **Boot güvenliği** | GPIO'lar, hiçbir servis başlamadan önce (Aşama 1) güvenli seviyeye alınır. |
| **Mandallama** | Kritik ihlal kendiliğinden temizlenmez. Koşul düzelse bile operatör onayı gerekir — aralıklı arızanın sessizce tekrarlanmasını önler. |
| **Tek veto noktası** | Tüm açma yolları `SafetyMonitor.permits()` üzerinden geçer. Yan kapı yoktur. |
| **Gözlemlenebilirlik** | Her veto ve her ihlal, neden koduyla loglanır ve API'den okunabilir. Sessiz engelleme yoktur. |
| **Bağımsızlık** | Güvenlik, otomasyon modundan bağımsız çalışır. MANUAL modda da tam yetkilidir. |

### 12.3 Acil durdurma tetikleyicileri

| Tetikleyici | Kaynak |
|---|---|
| Kuru çalışma (akış doğrulama başarısız) | `SafetyMonitor` |
| Kritik su seviyesi | `SafetyMonitor` |
| Maksimum çalışma süresi tekrarlı aşımı | `SafetyMonitor` |
| Güvenlik sensörü arızası | `SensorService` kalitesi → `SafetyMonitor` |
| Task heartbeat kaybı | `SystemSupervisor` |
| Operatör komutu (web / OLED) | `CommandQueue` |

---

## Bölüm 13 — Display Mimarisi

### 13.1 ViewModel deseni

```text
   StateStore.snapshot()
            │
            ▼
   ┌─────────────────────┐
   │  ViewModelBuilder   │   snapshot + aktif ekran → saf görüntü verisi
   │  (saf fonksiyon)    │   · biçimlendirilmiş string'ler
   └─────────────────────┘   · ikon/durum kodları
            │                · kalite → "—" dönüşümü
            ▼
   ┌─────────────────────┐
   │  ScreenRenderer     │   ViewModel → OledPanel çizim çağrıları
   │  (ekran başına bir) │   · yalnızca değişen alanı çizer
   └─────────────────────┘
            │
            ▼
        [OledPanel]        ◀── yalnızca `ui` task'ı erişir
```

`ViewModelBuilder` saf bir dönüşümdür: donanıma dokunmaz, yan etkisi yoktur. Bu, ekran
mantığının **donanımsız test edilebilmesini** sağlar.

### 13.2 Display kuralları (P2'nin uygulaması)

| Yasak | Neden |
|---|---|
| Sensör okumak | `Sensor.cpp` bugün bunu yapıyor; UI donanım zamanlamasına bağlanıyor |
| Wi-Fi'ye bağlanmak / mod değiştirmek | `MyWifi.cpp` bugün OLED'e yazıyor, `GrowPlant.cpp` Wi-Fi modunu değiştiriyor |
| Röle sürmek | Aktüatör sahibi yalnızca `ActuatorManager` |
| EEPROM/NVS yazmak | Kalıcılaştırma `store` task'ının işi |
| Task suspend/resume çağırmak | `pauseWiFiMonitor()` deseni tamamen kaldırılır |

UI'nin dış dünyaya **tek çıkışı** `CommandQueue.post()`'tur.

### 13.3 Ekran yapısı

```text
  ┌─ STATUS BAR (tüm ekranlarda sabit) ────────────────┐
  │  saat · Wi-Fi ikonu+RSSI · mod rozeti · hata sayısı │
  └────────────────────────────────────────────────────┘

  Ana ekranlar (SAYFA MODUNDA encoder ile yatay gezinme — döngüsel):
    HOME       → özet: mod, pompa durumu, kritik sensörler, **IP adresi**
    CROP       → ürün seçimi + programı başlat/durdur
    SENSORS    → tüm sensörler + kalite göstergesi
    CONTROL    → aktüatör durumu + manuel komut + acil durdurma
    NETWORK    → durum, SSID, IP, RSSI, AP bilgisi  (şifre GÖSTERİLMEZ)
    SYSTEM     → uptime, mod, saat, yeniden başlat
    ALERTS     → aktif hatalar, acil durum, onay/temizleme

  Öncelikli ekran:
    EMERGENCY  → acil durum mandallıyken diğer ekranların üzerine gelir,
                 nedeni gösterir, onay istemeden kapanmaz
```

### 13.4 Girdi işleme

```text
  [Encoder ISR] ──▶ InputEventQueue ──▶ ui task ──▶ navigasyon / düzenleme
  [Buton ISR]   ──▶ (kısa/uzun basış) ──▶ onay / geri / acil durdurma (uzun basış)
```

**İki seviyeli gezinme (TASK-075).** Encoder tek bir listede hem sayfaları hem
satırları gezerse, sayfa değiştirmek yoldaki her seçeneğin üzerinden geçmek
demektir (`SENSORS` + `CONTROL` + `CROP` tek başına 21 detent):

```text
  SAYFA MODU   çevir → sayfa (döngüsel, en fazla 3 detent)
               BAS   → sayfanın içine gir
  ÖĞE MODU     çevir → sayfa içindeki satırlar (döngüsel)
               BAS   → onay akışı (iki basış)
               GERİ  → sayfa moduna dön
```

**Öğe moduna yalnızca kullanıcı girer**: hiçbir olay ve hiçbir ekran geçişi
gezinmeyi kendiliğinden sayfanın içine almaz. Aksi hâlde kullanıcı iki seviye
derinde başlar, encoder'ı çevirir, sayfa değişmez ve ekran donmuş görünür.

İki zaman aşımı vardır: 20 sn hareketsizlikte onay ve öğe modu düşer (ekran
değişmez), 60 sn'de `HOME`'a dönülür. Kısası acil durum ekranında da işler —
kimse bir modun içinde kilitli kalamaz.

Seçili satır **ters renkle** vurgulanır; sayfa modunda hiçbir satır
vurgulanmaz (imleç o modda kullanıcıya ait değildir). Ayırıcı çizginin yerini
mod göstergesi alır: sayfa modunda dilimlenmiş konum şeridi + ▼, öğe modunda
**tamamen dolu şerit** + ▲.

ISR yalnızca ham olay üretir; dekodlama, debounce sonrası yorumlama ve tüm çizim task
bağlamındadır. Detent normalizasyonu tamsayı aritmetiğiyle yapılır ve yapılandırılabilirdir.

---

## Bölüm 14 — Web Mimarisi

### 14.1 Sorumluluk ayrımı

| Kanal | Kullanım | Neden |
|---|---|---|
| **HTTP GET** | Statik varlıklar, konfigürasyon okuma, geçmiş veri sorgusu | Önbelleklenebilir, idempotent, sayfa yenilemede güvenli |
| **HTTP POST/PUT** | Konfigürasyon yazma, kimlik doğrulama | Doğrulama ve açık hata kodu gerektiren işlemler |
| **WebSocket** | Canlı durum yayını **ve** aktüatör komutları | Düşük gecikme, sunucu-güdümlü push; polling'i ortadan kaldırır |

Mevcut projedeki 600 ms'lik HTTP polling kaldırılır — hem trafik hem de gecikme açısından
WebSocket push üstündür.

### 14.2 Durum senkronizasyonu (Kritik Problem 5'in çözümü)

```text
   İstemci bağlanır
        │
        ▼
   ── SUNUCU: FULL STATE  { type:"state", v:1234, full:true, ... } ──▶
        │
        │  (bundan sonra)
        ▼
   ── SUNUCU: değişimde veya en fazla 1 Hz ─────────────────────────▶
        │
   İstemci komut gönderir
        │
        ▼
   ── İSTEMCİ: { type:"cmd", reqId:"a7", target:"pump", action:"on" } ──▶
        │
        │   arayüz "BEKLİYOR" durumuna geçer  (durumu DEĞİŞTİRMEZ)
        ▼
   ── SUNUCU: { type:"ack", reqId:"a7", result:"ACCEPTED" } ─────────▶
        │
   ── SUNUCU: { type:"state", v:1235, ... }  ◀── gerçek durum burada ─▶
        │
   İstemci kartı YALNIZCA bu state ile günceller
```

| Kural | Gerekçe |
|---|---|
| İstemci **iyimser güncelleme yapmaz** | Mevcut projedeki en yaygın tutarsızlık kaynağı |
| Her komutun `reqId`'si vardır | Ack ile eşleştirme; hangi isteğin reddedildiği belli olur |
| Her state paketinin `v` versiyonu vardır | Sıra dışı/eski paket yok sayılır |
| Bağlantıda **tam state** gönderilir | Sayfa yenilendiğinde gerçek durum anında görünür |
| Otomatik yeniden bağlanma | Üstel backoff; bağlantı kopukken arayüz "bağlantı yok" durumu gösterir ve **eski veriyi canlıymış gibi göstermez** |
| Reddedilen komut açık nedenle bildirilir | `SAFETY_BLOCKED`, `COOLDOWN`, `UNAUTHORIZED` |

### 14.3 API sözleşmesi (taslak)

| METHOD | PATH | Amaç | Yetki |
|---|---|---|---|
| `GET` | `/` , `/assets/*` | Statik arayüz (gzip'li) | Açık |
| `POST` | `/api/auth/login` | Oturum token'ı al | Açık |
| `POST` | `/api/auth/logout` | Oturumu sonlandır | Token |
| `GET` | `/api/state` | Tam durum anlık görüntüsü (WS alternatifi) | Token |
| `GET` | `/api/config` | Konfigürasyon (**sırlar maskeli**) | Token |
| `PUT` | `/api/config/network` | Wi-Fi/IP ayarları | Token |
| `PUT` | `/api/config/automation` | Kurallar, eşikler, çizelgeler | Token |
| `PUT` | `/api/config/calibration` | Sensör kalibrasyonu | Token |
| `POST` | `/api/network/scan` | Tarama başlat | Token |
| `GET` | `/api/network/scan` | Tarama sonucu **ve durumu** (`idle`/`running`/`done`) | Token |
| `POST` | `/api/actuators/{id}` | Manuel komut (WS alternatifi) | Token |
| `POST` | `/api/system/emergency-stop` | Acil durdurma tetikle | Token |
| `POST` | `/api/system/emergency-clear` | Acil durumu onayla ve temizle | Token |
| `POST` | `/api/system/restart` | Kontrollü yeniden başlatma | Token |
| `POST` | `/api/system/factory-reset` | Fabrika ayarları (onay parametreli) | Token |
| `GET` | `/api/diagnostics` | Son olaylar, aktif hatalar, boot raporu | Token |
| `GET` | `/api/history?from&to` | Geçmiş sensör verisi (sayfalı) | Token |
| `WS` | `/ws` | Canlı durum + komut + ack | Token (el sıkışmada) |

**Kaldırılanlar:** `/index` (kırık link), `/api/clear-sensor` (karşılığı olmayan çağrı).
Her endpoint'in hem sunucu hem istemci tarafında karşılığı vardır — tek taraflı endpoint
mimariye girmez (P7).

### 14.4 Kimlik doğrulama

| Konu | Karar |
|---|---|
| Yöntem | Kurulum sırasında belirlenen parola → oturum token'ı (bellekte, süreli) |
| Saklama | Parola **hash + salt** olarak NVS'te. Düz metin saklanmaz. |
| İlk kurulum | Parola belirlenene kadar sistem "kurulum modunda"; yalnızca AP üzerinden ve yalnızca kurulum endpoint'leri açık |
| Kapsam | Tüm yazma işlemleri ve tüm state okuma yetki ister; yalnızca statik varlıklar ve login açıktır |
| Sınır | HTTPS **yoktur** (ESP32'de TLS maliyeti + sertifika yönetimi). Sistem yerel ağ cihazı olarak konumlanır; bu bilinçli ve belgelenmiş bir kısıttır. |
| Kaba kuvvet | Başarısız denemede artan gecikme + geçici kilit |

### 14.5 Hata yanıtları ve doğrulama

Tüm hatalar tek biçimde döner: `{ "error": { "code": "...", "message": "...", "field": "..." } }`

Doğrulama **sunucuda zorunludur** (istemci doğrulaması yalnızca kullanıcı deneyimidir):
tip, aralık, uzunluk, enum üyeliği ve alanlar arası tutarlılık (örn. static IP seçildiyse
gateway zorunlu). Geçersiz istek aktüatöre veya konfigürasyona **hiç ulaşmaz**.

### 14.6 AsyncTCP bağlam kuralları

| Kural | Gerekçe |
|---|---|
| Callback içinde dosya taraması / uzun döngü / bekleme yok | AsyncTCP task'ını bloklar, tüm web arayüzü donar |
| Callback yalnızca doğrular ve kuyruğa koyar | Komut yürütme `app_core`'a aittir |
| Büyük JSON üretimi sınırlı ve önceden boyutlandırılmış | Heap parçalanmasını önler |
| İstemci yazma kuyruğu doluysa telemetri paketi **düşürülür** | Yavaş istemci sistemi geriye doğru bloklamamalı |
| Statik varlıklar **önceden gzip'lenmiş** servis edilir | 298 KB'lık Bootstrap dosyası bugünkü haliyle flash ve bant genişliği israfıdır |

---

## Bölüm 15 — Storage Mimarisi

### 15.1 Veri sınıflandırması ve teknoloji eşlemesi

| Veri sınıfı | İçerik | Teknoloji | Gerekçe |
|---|---|---|---|
| **Sırlar** | Wi-Fi şifresi, arayüz parola hash'i | **NVS** (ayrı namespace) | Anahtar-değer, atomik yazma, aşınma dengeleme. Şifre asla dosyada tutulmaz. |
| **Konfigürasyon** | Ağ modu, kalibrasyon, eşikler, çizelgeler, aktüatör kısıtları | **NVS** (şemalı, versiyonlu) | Küçük, sık okunan, nadiren yazılan. Bozulmaya karşı anahtar bazında dayanıklı. |
| **Web varlıkları** | HTML/CSS/JS (gzip'li) | **LittleFS** | Dosya semantiği gerekli; okuma ağırlıklı. |
| **Geçmiş sensör verisi** | Zaman damgalı örnekler | **LittleFS üzerinde sabit boyutlu halka dosya** | Sıralı yazma, sabit boyut, öngörülebilir aşınma. |
| **Olay/hata günlüğü** | Son N kritik olay | **RAM halka tamponu** + kritikler LittleFS'e | Çoğu teşhis için RAM yeter; yalnızca CRITICAL kalıcılaşır. |
| **Çalışma zamanı durumu** | Sensör değerleri, aktüatör durumu, FSM | **Yalnızca RAM** | Kalıcılaştırma gereksiz flash aşınması yaratır. |
| **Son bilinen aktüatör durumu** | Yeniden başlatma sonrası bilgi | **NVS, geciktirilmiş yazma** | Yalnızca bilgi amaçlı. Boot'ta **otomatik geri yüklenmez** — güvenlik gereği her boot rölesiz başlar. |

### 15.2 Teknoloji kararları ve gerekçeleri

| Teknoloji | Karar | Gerekçe |
|---|---|---|
| **Ham `EEPROM.h`** | **Terk edilir** | Tüm ayarları tek bloğa yazan `StoredData` deseni; tek alan değişse bile tüm blok yeniden yazılır, kısmi yazmada tüm config kaybolur, şema evrimi imkânsızdır. NVS bu üç sorunu da çözer. |
| **NVS / Preferences** | **Birincil config deposu** | Anahtar bazında atomiklik, aşınma dengeleme, namespace ayrımı, alan bazında migration imkânı. |
| **LittleFS** | **Korunur** | Güç kesintisine dayanıklı, aşınma dengelemeli; web varlıkları ve halka dosya için uygun. |
| **SPIFFS** | **Tamamen kaldırılır** | Zaten devre dışı; include'u bile bırakılmaz. |
| **SQLite3** | **Kaldırılır** | Gerekçe: (1) sorgu ihtiyacı yok — erişim deseni "zaman aralığı oku", (2) ciddi flash ve RAM maliyeti, (3) LittleFS üzerinde B-tree yazması aşınmayı artırır, (4) mevcut projede hiç etkinleştirilmemiş — yani kaybedilen çalışan işlevsellik yok. Sabit kayıtlı halka dosya aynı ihtiyacı kat kat ucuza karşılar. |
| **OTA** | **Etkinleştirilmesi önerilir** | SQLite kalkınca binary küçülür ve 4 MB flash'ta çift uygulama bölümü mümkün hale gelir. Fiziksel erişim gerektirmeyen güncelleme, sahadaki bir sera cihazı için önemli bir kazanımdır. Partition şeması bu karara göre yeniden belirlenmelidir. |

### 15.3 Config şema versiyonlama

Her kayıt bir `schemaVersion` taşır. Boot'ta:

```text
  okunan sürüm == mevcut  →  doğrudan kullan
  okunan sürüm <  mevcut  →  migration uygula, yeni sürümle yaz, INFO logla
  okunan sürüm >  mevcut  →  varsayılana dön, WARNING logla  (firmware geri alınmış)
  kayıt yok / bozuk       →  varsayılana dön, WARNING logla
```

Sessiz `memset(0)` yoktur; her varsayılana dönüş görünür ve loglanır.

---

## Bölüm 16 — Hata Yönetimi ve Teşhis

### 16.1 Seviyeler

| Seviye | Anlam | Aksiyon |
|---|---|---|
| `INFO` | Normal olay (bağlandı, kural tetiklendi) | Halka tamponuna yazılır |
| `WARNING` | Beklenmeyen ama tolere edilebilir (varsayılana dönüldü, sensör aralık dışı) | Tamponda + web'de görünür |
| `ERROR` | Bir işlev kullanılamıyor (FS mount edilemedi, sensör arızalı) | Sistem `DEGRADED`; ilgili işlev devre dışı |
| `CRITICAL` | Güvenlik veya bütünlük tehlikede (kuru çalışma, task kaybı, WDT reset) | Aktüatörler güvenli duruma; kalıcı kayıt; `EMERGENCY`/`SAFE` mod |

### 16.2 Hata kodu yapısı

Her hata `{subsystem, code}` çiftidir (örn. `SENSOR.FLOW_NO_PULSE`, `NET.AUTH_FAILED`,
`SAFETY.DRY_RUN`). Serbest metin yalnızca insan okunabilirlik içindir; **karar mantığı
yalnızca koda bakar**. Aktif hatalar bitmask olarak tutulur; böylece UI ve API "kaç aktif
hata var" sorusunu maliyetsiz yanıtlar.

### 16.3 Arıza → davranış matrisi

| Arıza | Tespit | Sistem davranışı | Kullanıcıya |
|---|---|---|---|
| **Sensor failure** | Kalite `FAULT` | Değer otomasyonda kullanılmaz; güvenlik sensörüyse ilgili aktüatör kilitlenir | OLED + web'de "—" ve uyarı rozeti |
| **Network failure** | FSM `BACKOFF`/`AP_FALLBACK` | **Otomasyon ve güvenlik etkilenmez.** AP açılır, yerel kontrol sürer | Wi-Fi ikonu + neden |
| **Storage failure** | Mount/yazma hatası | Config RAM'de çalışır, kalıcılaştırma devre dışı; geçmiş veri yazılmaz | "Ayarlar kaydedilemiyor" uyarısı |
| **Display failure** | I2C/init hatası | **Sistem tam çalışır**; UI task'ı çizmeden devam eder veya askıya alınır | Web'de `DISPLAY_UNAVAILABLE` |
| **Actuator failure** | Akış doğrulama / kısıt ihlali | Aktüatör kapatılır ve kilitlenir; `CRITICAL` | Acil durum ekranı, onay gerekir |
| **Invalid data (API)** | Şema doğrulaması | İstek reddedilir, state değişmez | 400 + alan adı |
| **Watchdog reset** | Boot'ta reset nedeni | Aktüatörler kapalı başlar; olay kalıcı kaydedilir | Boot raporunda "önceki oturum WDT ile sonlandı" |
| **Task heartbeat kaybı** | `SystemSupervisor` | `forceAllOff()` + `DEGRADED`/`EMERGENCY` | Acil durum ekranı |
| **Heap kritik seviyede** | Periyodik izleme | Telemetri hızı düşürülür, geçmiş yazımı duraklatılır | Uyarı |

### 16.4 Yasaklanan hata davranışları

| Yasak | Yerine |
|---|---|
| `while(true)` ile kilitlenme | Bayrak kaydı + degraded mod |
| `setup()` içinden erken `return` | Aşamalı boot; sonraki aşamalar yine çalışır |
| Sessiz yutma (dönüş değeri kontrol edilmemiş) | Her başlatma sonucu boot raporuna yazılır |
| Yalnızca `Serial.println` ile raporlama | Seviyeli, kodlu, API'den okunabilir kayıt |
| Kullanıcıya gösterilmeyen dahili hata | Her `ERROR`/`CRITICAL` arayüzde görünür |

---

## Bölüm 17 — Modül ve Klasör Yapısı (öneri)

```text
  src/
    main.cpp                    ← yalnızca boot sırası ve task oluşturma

    core/                       ← cross-cutting, hiçbir katmana bağımlı değil
      StateStore    CommandQueue    Diagnostics    Config    WatchdogGuard

    hal/                        ← L1: ince donanım sarmalayıcıları
      AdcInput   PulseCounter   RelayOutput   OledPanel
      RotaryEncoder   ButtonInput   NvsStore   FileStore   WifiRadio

    services/                   ← L2: donanım sahibi servisler
      SensorService   NetworkService   StorageService   TimeService

    domain/                     ← L3: karar mantığı, donanımsız test edilebilir
      SafetyMonitor   AutomationEngine   ActuatorManager   SystemSupervisor
      rules/   models/

    interfaces/                 ← L4: sunum
      web/      WebService   ApiRoutes   WsProtocol   AuthService
      ui/       UiService    ViewModelBuilder   screens/

    tasks/                      ← task giriş fonksiyonları ve zamanlama
      AppCoreTask   SensorTask   NetworkTask   UiTask   StorageTask

  data/                         ← LittleFS imajı (gzip'li varlıklar)
  test/                         ← domain katmanı için donanımsız testler
```

**Kural:** `Define.h` benzeri, tüm projeyi birbirine bağlayan toplayıcı header
**oluşturulmaz**. Her dosya yalnızca kullandığı arayüzü include eder. Bu, hem derleme
süresini hem de katman ihlallerinin fark edilmesini doğrudan etkiler.

**Test edilebilirlik:** `domain/` katmanı yalnızca `core/` tiplerine bağımlıdır ve donanım
çağrısı içermez. Bu sayede güvenlik kuralları, otomasyon kararları ve state geçişleri
ESP32 olmadan, masaüstünde test edilebilir. Test stratejisinin ana dayanağı budur.

---

## Bölüm 18 — Problem → Çözüm İzlenebilirliği

`REQUIREMENTS.md` Kritik Problemler bölümünün bu mimarideki karşılıkları:

| # | Problem (REQUIREMENTS) | Mimarideki çözüm | Bölüm |
|---|---|---|---|
| 1 | Otomasyon mantığı yok | `AutomationEngine` + kural modeli + değerlendirme döngüsü | 11 |
| 2 | Korumasız shared state | `StateStore` (tek yazar + snapshot), global değişken yasağı | 4, 5 |
| 2b | OLED'e 3 farklı task'tan erişim | OLED sahipliği yalnızca `ui` task'ında; ViewModel deseni | 13 |
| 3 | Bloklayan Wi-Fi bağlantısı | Olay güdümlü, bloklamayan Wi-Fi FSM + backoff | 8 |
| 3b | `vTaskSuspend` ile task yönetimi | Kaldırıldı; FSM durumu ve kuyruklar | 5 |
| 4 | Hata durumunda yarı ölü sistem | Aşamalı boot + degraded mod + halt yasağı | 7, 16 |
| 5 | Frontend/backend tutarsızlığı | Sunucu-otoriter state, `reqId`+ack, versiyonlu paket, iyimser güncelleme yasağı | 14 |
| 6 | Ölü kod yükü | P7; SQLite/SPIFFS/SH1106 kaldırılır, yazılmayan bildirim kalmaz | 0, 15 |
| 7 | Güvenlik yok | Kimlik doğrulama + hash'li parola + şifrenin gösterilmemesi | 14 |
| — | Watchdog kurulum sırası ve kapsamı | TWDT boot'un ilk adımı; 5 task'ın tamamı kayıtlı; heartbeat denetimi | 6.5 |
| — | Sensör okuma ile UI'nin karışması | Katman ayrımı + işleme hattı | 9.4 |
| — | Saat ilerlemiyor | `TimeService` + UI'nin snapshot'tan periyodik çizmesi | 2.13, 13 |
| — | Factory reset erişilemez | `POST /api/system/factory-reset` + OLED SYSTEM ekranı | 14.3 |
| — | Static IP çağrılmıyor | `network.mode` config alanı + API + AP/STA'dan bağımsızlık | 8.2 |
| — | HTTP polling (600 ms) | WebSocket push | 14.1 |

---

## Bölüm 19 — Bilinçli Kararlar ve Kabul Edilen Ödünler

| Karar | Kazanç | Ödün |
|---|---|---|
| Tek `StateStore` mutex'i | Atomik tutarlı görüntü, deadlock riski yok | Tüm yazarlar aynı kilidi paylaşır (kritik bölge çok kısa olduğu için kabul edilebilir) |
| 5 task | Öngörülebilir zamanlama, düşük bellek | Bazı işler aynı task içinde sıraya girer |
| Snapshot kopyalama | Okuyucu kilit tutmaz | Her okumada birkaç yüz baytlık kopya maliyeti |
| SQLite kaldırılması | Ciddi flash/RAM tasarrufu, OTA imkânı | Karmaşık sorgu yeteneği kaybı (mevcut ihtiyaçta yok) |
| HTTPS yok | Bellek ve karmaşıklık tasarrufu | Yerel ağ güveni varsayımı — belgelenmiş kısıt |
| Süreli manuel override | Otomasyonun kalıcı devre dışı kalmasını önler | Operatörün kalıcı manuel istediği senaryoda mod değiştirmesi gerekir |
| Boot'ta aktüatör durumu geri yüklenmez | Güvenli varsayılan | Güç kesintisi sonrası pompa otomatik devam etmez (otomasyon devralır) |
| Domain katmanının donanımsız olması | Masaüstünde test edilebilirlik | Bir soyutlama katmanı fazladan bakım maliyeti |

---

## Bölüm 20 — Implementation Öncesi Kapatılması Gereken Açık Maddeler

Bunlar mimariyi değiştirmez ancak **Planning aşamasında netleşmelidir**:

- [ ] **Su seviyesi sensörü tipi.** Öneri: iki dijital şamandıra (düşük + kritik). Güvenlik zincirinin temelidir; tek analog sensöre bağlı kalınacaksa arıza tespiti tasarımı genişletilmelidir.
- [ ] **Pin bütçesi ve ADC1 çakışması.** pH, EC ve analog seviye **ADC1** (GPIO 32–39) gerektirir; Wi-Fi aktifken ADC2 kullanılamaz. Mevcut kullanımda encoder GPIO 32/33'ü (ADC1 kanalları) işgal ediyor. **Encoder ADC olmayan pinlere taşınmalıdır.**
- [ ] **GPIO 34–39 giriş-only ve dahili pull-up yoktur.** Akış sensörü bugün GPIO 34'te `INPUT_PULLUP` ile kullanılıyor; bu ayar donanımsal olarak etkisizdir. Harici pull-up gerekir veya pin değişmelidir.
- [ ] **Röle modülü aktif seviyesi.** Aktif-düşük modüllerde boot anında pompa çalışabilir. Doğrulanmalı; gerekiyorsa donanımsal pull-down/pull-up eklenmelidir.
- [ ] **RELAY2'nin gerçek yükü** (`REQUIREMENTS.md` §12). Mantıksal kimlik/fiziksel pin ayrımı sayesinde yalnızca yapılandırma etkilenir.
- [ ] **Su sıcaklığı sensörü: analog NTC mi, dijital DS18B20 mı.** Dijital seçenek bir ADC1 kanalı boşaltır ve kalibrasyon ihtiyacını ortadan kaldırır.
- [ ] **Donanımsal RTC gerekli mi.** Zamanlı sulama programı ağ olmadan da çalışacaksa DS3231 gerekir; aksi halde `TimeService` yalnızca SNTP ile yeterlidir ve zaman geçersizken schedule kuralları devre dışı kalır.
- [ ] **Partition şeması ve OTA kararı.** 4 MB flash içinde çift uygulama bölümü + LittleFS dengesi belirlenmelidir.
- [ ] **Geçmiş veri saklama süresi ve örnekleme aralığı.** Halka dosyanın boyutunu ve kayıt formatını bu belirler.
- [ ] **İlk kurulum akışı.** AP moduna bağlanan kullanıcının parolayı nasıl belirleyeceği ve captive portal'ın kapsama alınıp alınmayacağı.

---

## Bölüm 21 — Sonraki Aşama

Bu doküman onaylandıktan sonra Implementation aşaması şu sırayla ilerlemelidir. Sıra
rastgele değildir: her adım kendisinden öncekine dayanır ve **güvenlik zinciri, otomasyondan
önce** tamamlanır.

```text
  1. core/        StateStore, CommandQueue, Diagnostics, Config, Watchdog
  2. hal/         sürücüler + aşamalı boot iskeleti
  3. services/    SensorService, StorageService
  4. domain/      SafetyMonitor + ActuatorManager        ◀── otomasyondan ÖNCE
  5. services/    NetworkService (Wi-Fi FSM), TimeService
  6. interfaces/  WebService (state push + komut sözleşmesi)
  7. interfaces/  UiService (ViewModel + ekranlar)
  8. domain/      AutomationEngine (kurallar ve çizelgeler)
  9. test/        domain katmanı donanımsız testleri
```

---

*Bu doküman sistemin NASIL inşa edileceğini tanımlar. NE inşa edileceği `REQUIREMENTS.md`
dosyasındadır. Implementation task'ları bu dokümandaki modül sınırlarından ve Bölüm 21'deki
sıradan türetilecektir.*
