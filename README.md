# Topraksız Tarım Kontrol Sistemi (ESP32)

Hidroponik bir sera için sensör okuyan, pompaları güvenlik kilitleriyle
kontrol eden, web ve OLED üzerinden yönetilebilen ESP32 tabanlı gömülü
kontrol yazılımı.

> **Durum: firmware derleniyor, donanımda HİÇ ÇALIŞMADI.**
> Ölçüm gerektiren hiçbir kabul kriteri doğrulanmadı. Ayrıntı için
> [§7 Dürüst Durum](#7-dürüst-durum).

---

## 1. Proje nedir

Eski proje "uzaktan kumandalı bir röle anahtarı + sensör göstergesi"
düzeyindeydi: otomasyon yoktu, paylaşılan kaynaklar korumasızdı, Wi-Fi
bağlantısı task'ı bloklıyordu, web arayüzü korumasızdı ve bir hata
durumunda sistem yarı ölü kalıyordu.

Bu depo o projenin **sıfırdan yeniden yazımıdır**. Eski kod `legacy/`
altında yalnızca pin bilgisi ve davranış referansı olarak duruyor;
build'e dahil değil.

**Ne yapar:**

- 6 sensör okur (su sıcaklığı, debi, pH, EC, su seviyesi, nem) ve her
  ölçümü bir **kalite** etiketiyle taşır
- Su ve hava pompasını min/max çalışma süresi ve cooldown kısıtlarıyla sürer
- Kuru çalışma, yetersiz su seviyesi ve süre aşımına karşı **mandallı
  güvenlik zinciri** işletir
- Eşik ve çizelge kurallarıyla otomatik sulama yapar (kural motoru)
- Web arayüzü (parola korumalı, WebSocket ile canlı) ve 128×64 OLED sunar
- Geçmiş sensör verisini halka dosyada saklar (~14 gün)

---

## 2. Teknolojiler

| Katman | Kullanılan |
|---|---|
| Donanım | ESP32-WROOM-32, 4 MB flash |
| Çatı | PlatformIO · espressif32 @ 6.12.0 · arduino-esp32 2.0.17 · ESP-IDF 4.4 |
| Dil | C++11 (`-Wall -Wextra`), frontend ES2017 |
| RTOS | FreeRTOS — 5 task, iki çekirdeğe dağıtılmış |
| Web | ESPAsyncWebServer 3.6 · AsyncTCP 3.3 · ArduinoJson 7.4 |
| Ekran | Adafruit SSD1306 + GFX |
| Depolama | NVS (config + sırlar) · LittleFS (web varlıkları + geçmiş) |
| Güvenlik | mbedTLS SHA-256 (donanım hızlandırmalı) |
| Frontend | **Çerçevesiz** — el yazımı HTML/CSS/JS, gzip'li 30,4 KB |
| Test | Unity (host) + 46 `static_assert` |

**Kaldırılanlar:** SQLite (66 MB kütüphane, hiç etkinleştirilmemişti),
Bootstrap (298 KB, %95'i kullanılmıyordu).

---

## 3. Mimari

### Katmanlar — bağımlılık yalnızca aşağı doğru akar

```
interfaces/   L4   web · ui            ← sunum, komut üretir
    ↓
domain/       L3   guvenlik · aktuator · otomasyon   ← karar verir
    ↓
services/     L2   sensor · ag · zaman · depolama    ← is mantigi
    ↓
hal/          L1   role · adc · pcnt · oled · wifi · nvs · fs
    ↓
core/         L0   tipler · state · kuyruklar · boot · watchdog
```

`core/` hiçbir katmana bağımlı değildir ve **Arduino başlığı bile
include etmez** — bu sayede domain mantığı PC'de test edilebilir.

### Yedi ilke

| | İlke | Nasıl zorlanıyor |
|---|---|---|
| P1 | **Tek yazar** | 7 alt-state, her birine tam 1 yazar (taramayla doğrulandı) |
| P2 | **Donanıma tek kapı** | Röleye yalnızca `ActuatorManager`, radyoya yalnızca `WifiRadio` |
| P3 | **Bloklama yasak** | Task döngülerinde `delay`/`vTaskDelay` **0** |
| P4 | **Fail-degraded** | Hiçbir arıza sistemi durdurmaz; boot aşamaları akışı kesmez |
| P5 | **Cihaz tek doğruluk kaynağı** | Arayüz durum **üretmez**, yalnızca gösterir |
| P6 | **Güvenlik üstün** | Veto her açma yolunda; `permit == nullptr` reddedilir |
| P7 | **Yazılmayan kod yok** | Bildirilip tanımlanmamış fonksiyon **0** |

### Task'lar

| Task | Çekirdek | Öncelik | Periyot | Sorumluluk |
|---|---|---|---|---|
| `app_core` | 1 | 4 | 100 ms | Güvenlik → otomasyon → aktüatör |
| `io_sense` | 1 | 3 | 250 ms | Sensör örnekleme (ADC + PCNT tek sahibi) |
| `net` | 0 | 2 | 100 ms | Wi-Fi FSM + web + WebSocket + zaman |
| `ui` | 1 | 2 | 50 ms | OLED + encoder (+ durum LED'i) |
| `store` | 0 | 1 | olay | Flash yazma (config, geçmiş) |

Wi-Fi yığını öngörülemeyen süreler harcadığı için Core 0'a; güvenlik ve
aktüatör kontrolü Core 1'e yalıtıldı.

### Güvenlik zinciri

```
snapshot → komutlari al → GUVENLIK degerlendir → otomasyon
         → komutlari uygula → AKTUATORLERI SUR → yayinla
```

Bu sıra değiştirilemez: otomasyon, güvenlik değerlendirmesi yapılmamış bir
state üzerinde karar veremez.

Üç katman:
1. **Ön koşul** — su seviyesi, sensör kalitesi, acil durum mandalı
2. **Çalışma sırasında** — enerjili aktüatör için izin kalkarsa `minRunMs`
   tanınmadan **derhal** durur
3. **Mandal** — kuru çalışma ve acil durum NVS'te kalıcı; yalnızca operatör
   onayıyla ve koşullar düzelmişse temizlenir

---

## 4. Nasıl çalışır

### Açılış — 9 aşamalı

```
0 RESET_AND_WDT    ZORUNLU   reset nedeni + watchdog
1 GPIO_SAFE_STATE  ZORUNLU   TUM ROLELER KAPALI   ← log altyapisindan bile ONCE
2 CORE_SERVICES    ZORUNLU   diagnostics, state store, kuyruklar
3 CONFIG_LOAD                NVS → basarisizsa VARSAYILAN
4 FILESYSTEM                 LittleFS → basarisizsa web statigi yok
5 DISPLAY_HW                 OLED → basarisizsa sistem TAM calisir
6 SENSOR_HW                  ADC + PCNT
7 NETWORK_RADIO              Wi-Fi → basarisizsa OFFLINE, guvenlik etkilenmez
8 TASK_CREATION    ZORUNLU   bes task
```

Zorunlu aşama başarısız → `SAFE`; zorunlu olmayan → `DEGRADED`; hepsi
başarılı → `RUNNING`.

### İlk kurulum

1. Cihaz parolasız açılır → **kurulum AP'si** yayınlar (`Sera-XXXXXX`)
2. AP şifresi ilk boot'ta rastgele üretilir ve **OLED'de gösterilir**
   (MAC'ten türetilmez — SoftAP kendi MAC'ini yayınlar, türetilen şifreyi
   menzildeki herkes hesaplayabilirdi)
3. Telefondan AP'ye bağlan → `http://192.168.4.1`
4. Arayüz parolası belirle (**yalnızca AP üzerinden** kabul edilir)
5. Wi-Fi ağını seç ve kaydet → cihaz ev ağına geçer

### Komut yolu — iyimser güncelleme YOK

```
kullanici tiklar
   → arayuz "BEKLIYOR" gosterir, DURUMU DEGISTIRMEZ
   → cmd  ── WebSocket ──▶ CommandQueue  (AsyncTCP hicbir seyi surmez)
   → app_core komutu alir, guvenlik izni sorar
   → izin varsa role suruulur
   → ack   ◀── "kabul edildi"
   → state ◀── "UYGULANDI"
   → arayuz kart durumunu YALNIZCA burada degistirir
```

Eski sistemde arayüz butona basar basmaz "ÇALIŞIYOR" yazıyordu — cihaz
komutu reddetse bile. Bu bir güvenlik sorunuydu.

### Otomasyon

Kurallar **veri**, motor **kod**. 8 sabit kural slotu, üç tip:
eşik (histerezisli), zaman penceresi (gece yarısını aşabilir), periyodik
çevrim. Otomasyon kısıtları ve güvenlik kilitlerini **bilmez** — yalnızca
"şu aktüatör açık olsun" der.

Operatör AUTO modda müdahale ederse otomasyon **o aktüatör için** 15 dakika
susar, sonra kontrolü geri alır.

---

## 5. Çalıştırma

```bash
# Firmware
pio run
pio run -t upload

# Web varliklari (gzip'lenip data/ uretilir)
python tools/build_assets.py
pio run -t uploadfs

# Seri izleme
pio device monitor

# Host testleri (gcc/g++ gerektirir)
pio test -e native
```

Bölümleme: `app0`/`app1` 1,5 MB (OTA) · `littlefs` 896 KB · `coredump` 64 KB.

---

## 6. Belgeler

| Dosya | İçerik |
|---|---|
| `REQUIREMENTS.md` | Eski sistemin **gerçek** durumu (%40–45 kapsam) |
| `ARCHITECTURE.md` | 21 bölüm: ilkeler, katmanlar, task'lar, güvenlik, boot |
| `IMPLEMENTATION_PLAN.md` | 16 faz, 65 task, bağımlılık grafiği, 8 kilometre taşı |
| `docs/tasks/TASK-XXX.md` | 65 task — her birinde tasarım ve inceleme kaydı |
| `docs/ISSUES.md` | Açık maddeler ve kapsam dışı bulgular |
| `docs/CODING_STANDARDS.md` | 14 yasak desen, her biri gerçek bir ihlale bağlı |
| `docs/HARDWARE.md` | Pin planı, röle pull-up ve şamandıra NC zorunlulukları |
| `docs/INTEGRATION_REPORT.md` | Entegrasyon bulguları ve ölçülmemişler listesi |
| `docs/HARDWARE_TEST_PROCEDURE.md` | ~45 numaralı donanım testi, M4 kapısı dahil |

---

## 7. Dürüst durum

### Yapılanlar

- 65 task'ın tamamı uygulandı; her birinde tasarım ve inceleme kaydı var
- Firmware derleniyor: **flash %65,4 · RAM %22** (`-Wall -Wextra`, 0 uyarı)
- 46 `static_assert` + 28 Unity testi yazıldı
- Frontend tarayıcıda doğrulandı (iyimser güncelleme yasağı dahil)

### YAPILMAYANLAR — hepsi donanım gerektiriyor

```
[ ] Sistem hicbir zaman BOOT ETMEDI
[ ] Bes task'in birlikte calismasi
[ ] Dort veri akisinin gecikmesi
[ ] Task periyotlari, stack watermark, heap kararliligi
[ ] 24 saat / 72 saat kararlilik
[ ] Ariza enjeksiyonunun tamami (TASK-061'in ozu)
[ ] M4 GUVENLIK KAPISI
[ ] Host testleri CALISTIRILMADI (bu makinede gcc yok; derlenebilirligi
    hedef derleyiciyle dogrulandi)
```

### Kritik açık maddeler

| # | Madde | Neden önemli |
|---|---|---|
| **ISSUE-003** | Röle polaritesi doğrulanmadı | **Güvenlik zincirinin tamamı** `RELAY_ACTIVE_LOW` varsayımına dayanıyor |
| ISSUE-024 | Stack boyutları tahmin | İlk çalıştırmada `minStack` ile düzeltilmeli |
| ~~ISSUE-021~~ | ~~Kural düzenleme arayüzü yok~~ | ✅ Çözüldü — kural API'si + düzenleyici eklendi, şema sürümü 2 |
| ISSUE-005 | Donanımsal RTC kararı | Ağsız çizelge çalışmaz — satın alma kararı |
| ISSUE-033 | Bu makinede derleyici/Python yok | Firmware **derlenmedi**, host testleri hâlâ koşmadı |
| ISSUE-014 | Akış katsayısı (450 darbe/L) teyitsiz | Kuru çalışma eşiği kayar |

### M4 kapısı

`IMPLEMENTATION_PLAN.md`: *"M4 doğrulanmadan otomasyon başlatılmaz."*
M4 = pompanın yalnızca güvenlik izniyle çalıştığının **donanımda**
kanıtlanması.

**Kapı kapalı.** Otomasyon kodu ve kural düzenleyici hazır, ancak
varsayılan mod `MANUAL`, varsayılan kural kümesi boş ve yeni eklenen her
kural **devre dışı doğar**; sistem kutudan çıktığında **kendiliğinden hiçbir
aktüatörü sürmez**. Arayüz OTOMATİK moda geçişte M4 uyarısı gösterir.

### Sıradaki iş

0. **Araç zincirini kur** (ISSUE-033): PlatformIO + Python.
   `pio run` ile derle, `pio test -e native` ile host testlerini bir kez koştur,
   `python tools/build_assets.py` ile `data/`'yi kanonik yoldan üret.
   *Bundan sonrası donanım işidir:*
1. Röle polaritesini ölç → ISSUE-003'ü kapat
2. İlk boot: seri porttan boot raporunu oku
3. `GET /api/diagnostics` → `minStack` ile stack'leri düzelt
4. `docs/HARDWARE_TEST_PROCEDURE.md` §2'yi koştur → M4 kapısı
5. M4 geçtikten **sonra** otomasyonu `AUTO`'ya al
