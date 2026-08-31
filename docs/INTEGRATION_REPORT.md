# Entegrasyon Raporu — TASK-060 … 063

**Tarih:** 2026-08-31
**Firmware:** `V.0.1.1` dalı, boot wiring sonrası

> **Bu rapor ölçülmemiş hiçbir değeri içermez.** Donanım gerektiren her
> ölçüm açıkça `ÖLÇÜLMEDİ` olarak işaretlidir. Ölçülmemiş bir değeri
> raporlamak, raporlamamaktan kötüdür.

---

## 1. En önemli bulgu: sistem ilk kez BAĞLANDI

ISSUE-013 → ISSUE-018 boyunca kayıtlı olan boşluk kapatıldı. TASK-060'ın
ilk kapsam maddesi ("tüm task'ların birlikte çalıştırılması") onsuz tanım
gereği imkânsızdı.

Eklenenler: `src/BootWiring.{h,cpp}` (9 aşamalık boot tablosu + 5 task
tablosu) ve gerçek bir `src/main.cpp`.

### Bunun ortaya çıkardığı gerçek

```text
                      ONCE (bagli degil)   SONRA (bagli)
Flash                 %27.8   437 253      %65.4  1 028 181
RAM (statik)          % 7.2    23 580      %22.0     72 208
```

**Önceki tüm boyut ölçümleri anlamsızdı.** Task giriş noktaları hiçbir
yerden çağrılmadığı için linker uygulamanın TAMAMINI ölü kod olarak
atıyordu — ağ yığını, web sunucusu, WebSocket, OLED, otomasyon, depolama.
33 task boyunca raporladığım flash rakamları yalnızca `core/` ve birkaç
`hal/` dosyasını ölçüyormuş.

Gerçek rakamlar bütçe içinde: uygulama bölümü 1,5 MB (OTA için iki tane),
kullanım %65.4. RAM'de 255 KB heap kalıyor (Wi-Fi ~50 KB ister).

### Boot wiring'in AÇIĞA ÇIKARDIĞI link hatası

Zincir canlanır canlanmaz çözümlenmemiş bir sembol ortaya çıktı:

```text
undefined reference to `AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(...)`
```

Nedeni: `ESPAsyncWebServer/AsyncJson.cpp` içeriği tamamen
`#if ASYNC_JSON_SUPPORT == 1` altında ve bu bayrak
`__has_include("ArduinoJson.h")` ile belirleniyor. PlatformIO her
kütüphaneyi KENDİ include yollarıyla derlediği için ArduinoJson o derleme
birimine görünmüyor → bayrak 0 → sınıfın kurucusu hiç üretilmiyor.

**Çözüm:** kütüphanenin sınıfı kullanılmıyor; gövde `interfaces/web/JsonBody.cpp`
içinde kendimiz topluyoruz ve ArduinoJson'ı KENDİ derleme birimimizde
kullanıyoruz. Gövde tamponu `request->_tempObject` içinde yaşıyor ve isteğin
yıkıcısı onu `free()` ediyor (`WebRequest.cpp:109`) — sızıntı yok, istekler
arası karışma yok.

---

## 2. §16.3 arıza → davranış matrisi denetimi

| Arıza | Kod yolu var mı | Donanımda doğrulandı |
|---|---|---|
| Sensor failure | ✅ `SensorPipeline` → kalite; `SafetyMonitor` kilitler | ❌ ÖLÇÜLMEDİ |
| Network failure | ✅ FSM `BACKOFF`/`AP_FALLBACK`; güvenlik bağımsız | ❌ ÖLÇÜLMEDİ |
| Storage failure | ✅ `STORAGE_*` kodları, 30 kullanım noktası | ❌ ÖLÇÜLMEDİ |
| Display failure | ✅ `UI_DISPLAY_UNAVAILABLE`; `ui` çizmeden devam | ❌ ÖLÇÜLMEDİ |
| Actuator failure | ✅ akış doğrulama + mandal | ❌ ÖLÇÜLMEDİ |
| Invalid data (API) | ✅ `ConfigValidation` + alan adlı hata | ❌ ÖLÇÜLMEDİ |
| Watchdog reset | ✅ `SYS_WATCHDOG_RESET`, boot raporunda | ❌ ÖLÇÜLMEDİ |
| Task heartbeat kaybı | ✅ `forceAllOff()` + `DEGRADED` | ❌ ÖLÇÜLMEDİ |
| **Heap kritik seviyede** | ❌ **YOKTU → BU TURDA EKLENDİ** | ❌ ÖLÇÜLMEDİ |

### Eklenen: heap kritik seviyesinde kendini kısma

Matrisin bu satırı **hiç uygulanmamıştı**. `SYS_LOW_HEAP` yalnızca tahsis
hatasında kullanılıyordu; periyodik izleme ve degradasyon yoktu.

```text
core::LOW_HEAP_BYTES = 32 768

SystemSupervisor  → periyodik olcum, esigin altinda SYS_LOW_HEAP yukselt
WsProtocol::tick  → telemetri araligi ×4  (her paket bir tahsis demek)
StorageService    → gecmis yazimi DURAKLATILIR (en az zararli kayip)
```

Eşik `core/SystemState.h`'ta: `domain/`, `services/` ve `interfaces/`
üçü de okuyor ve hiçbiri diğerine bağımlı olmuyor.

---

## 3. Bulunan ve düzeltilen entegrasyon hataları

### 3.1 Config değişiklikleri hiç yazılmıyordu *(TASK-059'da bulundu)*

`config::persist()` ve `isDirty()` **hiçbir yerden çağrılmıyordu**.
Kullanıcı ayar değiştirir, "Kaydedildi" görür, yeniden başlatmada her şey
eski hâline dönerdi. `storage::tick()` artık 2 sn'lik birleştirmeyle
persist tetikliyor.

### 3.2 `FACTORY_RESET` komutu sessizce yok sayılıyordu *(bu turda)*

Uç nokta `confirm=FACTORY_RESET` istiyor, yetki arıyor, komutu kuyruğa
koyuyor ve `{"ok":true}` dönüyordu. `app_core` ise komutu **sahipsiz**
listesinde bırakıp hiçbir şey yapmıyordu.

Kullanıcı fabrika ayarlarına döndüğünü sanıyor, cihaz hiçbir şey
yapmıyordu. Bağlandı: `config::factoryReset()` (config NVS + sırlar) +
kontrollü yeniden başlatma. Parola hash'i silindiği için sistem kurulum
moduna döner.

### 3.3 Kurulum uç noktası AP dışından erişilebiliyordu *(bu turda)*

TASK-042 tasarım kaydı "kurulum modunda yalnızca AP üzerinden erişilebilir"
diyordu; uygulanmamıştı. Parolası olmayan bir cihaz ev ağına bağlıysa
(firmware yükseltmesi, kısmi NVS bozulması) ağdaki **herkes** ilk parolayı
belirleyip cihazı sahiplenebilirdi.

`fromSetupAp()` eklendi: `POST /api/setup/password` yalnızca `192.168.4.x`
istemcilerini kabul ediyor.

---

## 4. Profilleme verisi artık GÖRÜNÜR *(TASK-062)*

`TaskRunner` stack watermark ve döngü süresi topluyordu; `TaskRegistry`
saklıyordu; **hiçbir yerde sunulmuyordu.** Ölçülemeyen bir şey
doğrulanamaz.

`GET /api/diagnostics` artık her task için: `registered`, `beats`,
`maxLoopUs`, `overruns`, `minStack` (BAYT — ESP-IDF), `lastBeat`.

### Statik yapılandırma (ölçüm değil, tahsis)

| Task | Çekirdek | Öncelik | Stack (bayt) | Periyot |
|---|---|---|---|---|
| `app_core` | 1 | 4 | 4096 | 100 ms |
| `io_sense` | 1 | 3 | 3072 | 250 ms |
| `net` | 0 | 2 | 5120 | 100 ms |
| `ui` | 1 | 2 | 3584 | 50 ms |
| `store` | 0 | 1 | 4096 | olay güdümlü |

**Gerçek watermark ÖLÇÜLMEDİ.** Stack boyutları tahminlere dayanıyor ve
ilk çalıştırmadan sonra `minStack` değerlerine göre düzeltilmelidir.

---

## 5. Güvenlik sertleştirme taraması *(TASK-063)*

| Denetim | Sonuç |
|---|---|
| Wi-Fi şifresi okuyan yer | 1 (`ConnectionManager`, bağlanma anı) |
| AP şifresi okuyan yer | 1 (`SoftApManager`, OLED gösterimi) |
| Auth hash okuyan yer | 1 (`AuthService`) |
| Log satırında sır | **0** |
| `SystemState`'te şifre alanı | **YOK** (alan hiç tanımlı değil) |
| `GET /api/config` maskeleme | `passwordSet` / `authSet` — değer okunmuyor bile |
| `strcpy`/`sprintf`/`gets`/`strcat` | **0** (yalnızca `strncpy`/`snprintf`) |
| Yetki kapsamı | 19 uç noktadan 15'i `requireAuth`; 4 istisna bilinçli |
| Sırlar ayrı NVS namespace | ✅ `sec` / `cfg` / `sys` |

### Bilinçli yetkisiz uç noktalar

```text
GET  /api/auth/status    istemci giris ekranini cizmeden once modu bilmeli
POST /api/auth/login     giris noktasi
POST /api/auth/logout    token sahibi kendi oturumunu duurur
POST /api/setup/password YALNIZCA kurulum modunda VE YALNIZCA AP uzerinden
```

### Kaynak tüketimi sınırları

```text
istek govdesi   4096 B   WS mesaji     512 B   WS istemci     4
oturum             4     kaba kuvvet   5/60sn  gecmis sayfa 240
komut/dongu        4     tarama sonucu  20
```

### Varsayılan yapılandırma güvenliği

```text
otomasyon modu        MANUAL      → kendiliginden sulama YOK
kural kumesi          BOS         → hicbir kural etkin degil
requireLevelSensor    1           → sensor yoksa pompa KILITLI
sensorler             cogu kapali → guvenlik sensorleri (seviye, akis) ACIK
aktuator maxRunMs     5 dk        → KISA varsayilan
arayuz parolasi       YOK         → kurulum modu, AP-only
```

### Belgelenmiş kalan riskler

1. **HTTPS yok.** Parola ağ üzerinde açık gider. Bilinçli kısıt (§14.4);
   `GET /api/auth/status` `"secure": false` döndürüyor ve giriş ekranı
   uyarıyı gösteriyor. Cihaz yerel ağ cihazıdır, internete açılmamalıdır.
2. **AP şifresi OLED'de görünür.** Kurulum için zorunlu; fiziksel erişimi
   olan biri zaten cihaza erişebilir.
3. **Hash tur sayısı ölçülmedi.** 20 000 tur ESP32'nin donanım
   hızlandırıcılı SHA-256 hızına dayanan bir tahmindir (ISSUE kaydı).
4. **Röle polaritesi doğrulanmadı** (ISSUE-003) — güvenlik zincirinin
   tamamı `RELAY_ACTIVE_LOW` varsayımına dayanıyor.

---

## 6. YAPILMAYAN ölçümler — dürüst liste

Aşağıdakilerin hiçbiri yapılmadı çünkü **çalışan donanım gerekiyor** ve
cihaza erişim yok:

- [ ] Beş task'ın birlikte kararlı çalışması
- [ ] Dört veri akışının uçtan uca gecikmesi
- [ ] Task periyotlarının gerçek ölçümü
- [ ] Heap kullanımı, tüm alt sistemler aktifken
- [ ] I2C ve ADC paylaşımında çakışma
- [ ] Wi-Fi trafiğinin ADC okumalarına etkisi
- [ ] 24 saat kesintisiz çalışma / WDT reset yokluğu
- [ ] Stack watermark → stack boyutlarının düzeltilmesi
- [ ] 72 saat kararlılık
- [ ] Flash aşınma tahmininin doğrulanması
- [ ] Her arıza senaryosunun kasıtlı üretilmesi (TASK-061'in ÖZÜ)
- [ ] Kuru çalışma testi / M4 kapısı

**M4 kapısı hâlâ KAPALI.** Otomasyon kodu yazıldı ama varsayılan `MANUAL`
ve kural kümesi boş; M4 donanımda doğrulanmadan `AUTO`'ya geçilmemelidir.

---

## 7. Bir sonraki adım

Kod tarafı bu noktada tamamlanmış durumda. Sıradaki iş **kod değil,
donanım**:

1. Röle polaritesini ölçüp ISSUE-003'ü kapat (güvenlik zinciri buna dayalı)
2. İlk boot: boot raporunu seri porttan oku, aşama sonuçlarını doğrula
3. `GET /api/diagnostics` → `minStack` değerlerine göre stack'leri düzelt
4. M4 senaryolarını sırayla koştur (seviye, kuru çalışma, maks süre)
5. M4 kapandıktan SONRA otomasyonu `AUTO`'ya al
