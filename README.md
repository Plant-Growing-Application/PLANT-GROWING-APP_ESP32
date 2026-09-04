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

- **8 sensör** okur (su sıcaklığı, debi, pH, EC, su seviyesi, ortam nemi,
  hava sıcaklığı, ışık) ve her ölçümü bir **kalite** etiketiyle taşır
- **5 aktüatör** sürer (su pompası, hava pompası, büyütme ışığı, ısıtıcı,
  besin dozaj pompası) — her biri min/max çalışma süresi ve cooldown
  kısıtlarıyla, rolüne göre farklı üst sınırlarla
- Kuru çalışma, yetersiz su seviyesi ve süre aşımına karşı **mandallı
  güvenlik zinciri** işletir; ısıtıcı da seviye kilitlerine bağlıdır
- **Ürün profili:** çilek/domates/biber/salatalık/marul/fesleğen seçilir,
  sistem o bitkinin **o dönemdeki** hedeflerini ve sulama/ışık/ısıtma
  programını kendisi kurar (`docs/CROP_PROFILES.md`)
- Eşik ve çizelge kurallarıyla otomatik sulama yapar (kural motoru)
- **İki seviyeli web arayüzü:** basit modda 3 sekme ve düz Türkçe tavsiyeler,
  uzman modunda kural düzenleyici ve teşhis ekranları
- 128×64 OLED sunar ve geçmiş sensör verisini halka dosyada saklar (~12 gün)

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
| Frontend | **Çerçevesiz** — el yazımı HTML/CSS/JS, 5 modül → tek gzip'li paket, 48,5 KB |
| Test | Unity (host, 46 test) + 134 `static_assert` |

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

### Ürün profili (meyve modu)

Kural motorunun bir katman üstü. Profiller `.rodata`'da sabittir; config'te
yalnızca **seçim** saklanır (16 bayt).

```
ürün × dönem  →  hedef bantlar (pH · EC · su/hava sıcaklığı · nem · ışık)
                 + üretilen kurallar (sulama · havalandırma · ışık · ısı · dozaj)
```

Neden dönem ayrımı var: çileği domates EC'sinde çalıştırmak yaprak büyütür ve
**meyve tutumunu öldürür**; fide dönemi de meyve dönemiyle aynı çözeltiyi
kaldıramaz. Değerler ve kaynakları `docs/CROP_PROFILES.md`'de.

Üç kural:

1. **Önizlemesiz uygulama yok** — seçim mevcut kuralların üzerine yazar;
   `POST /api/crop/preview` ne değişeceğini önce söyler.
2. **Çalışamayacak kural üretilmez** — hedef röle bağlı değilse veya eşiğin
   sensörü takılı değilse o kural hiç doğmaz.
3. **Profil uygulamak sulamayı başlatmaz** — kurallar etkin doğar ama
   `automation.mode` `MANUAL` kalır ve motor MANUEL'de hiçbir kuralı
   değerlendirmez. M4 kapısı kapalı.

Kullanıcı bir kuralı elle değiştirirse profil `ÖZEL`'e düşer ve arayüz
"Özel (Çilek temelli)" der — ekranda çilek yazarken kuralların çilek profiline
ait olmadığı bir durum bırakmıyoruz. Hedef bantlar korunur: bantlar **bitkiyi**
anlatır, kuralları değil.

Dönem, dikim tarihinden gün sayılarak kendiliğinden ilerler — **cihaz saati
geçerliyken**. Geçersizken donar (donanımsal RTC yok, ISSUE-005) ve arayüz
bunu söyler.

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

### Cihaz olmadan arayüzü çalıştırma

```bash
python tools/mock_device.py
```

`http://127.0.0.1:8099` — parola herhangi bir şey olabilir. Sahte cihaz,
firmware'in **döndürdüğü şemayı** taklit eder ve arayüzü gerçek bir kurulum
gibi besler:

- WebSocket üzerinden canlı telemetri, komut → **ack** → gerçek durum yolu
- fiziksel benzetim: pompa çalışınca debi oluşur ve depo boşalır, ısıtıcı suyu
  ısıtır, bitki EC'yi düşürür, dozaj pompası yükseltir, ışık gün içi salınır
- güvenlik zinciri: seviye düşünce pompa **ve ısıtıcı** kilitlenir, acil
  durdurma mandallanır
- kural motoru: ürün profili uygulanıp OTOMATİK moda geçilince çevrim, pencere
  ve eşik kuralları gerçekten röleleri sürer

Sanal saat varsayılan olarak **60× hızlı** akar (`--speed`); 30 dakikalık bir
sulama çevrimi 30 saniyede izlenebilir. `--port` ile bağlantı noktası
değiştirilir.

> Bu bir **simülatördür, firmware değildir**: güvenlik zinciri ve aktüatör
> kısıtları basitleştirilmiştir. Buradaki davranış firmware'in doğru olduğunun
> kanıtı değildir — arayüzün doğru çizildiğinin kanıtıdır.

---

## 6. Belgeler

| Dosya | İçerik |
|---|---|
| `REQUIREMENTS.md` | Eski sistemin **gerçek** durumu (%40–45 kapsam) |
| `ARCHITECTURE.md` | 21 bölüm: ilkeler, katmanlar, task'lar, güvenlik, boot |
| `IMPLEMENTATION_PLAN.md` | 16 faz, 65 task, bağımlılık grafiği, 8 kilometre taşı |
| `docs/tasks/TASK-XXX.md` | 74 task — her birinde tasarım ve inceleme kaydı |
| `docs/CROP_PROFILES.md` | Ürün × dönem parametre tablosu ve **kaynakları** |
| `docs/ISSUES.md` | Açık maddeler ve kapsam dışı bulgular |
| `docs/CODING_STANDARDS.md` | 14 yasak desen, her biri gerçek bir ihlale bağlı |
| `docs/HARDWARE.md` | Pin planı, röle pull-up ve şamandıra NC zorunlulukları |
| `docs/INTEGRATION_REPORT.md` | Entegrasyon bulguları ve ölçülmemişler listesi |
| `docs/HARDWARE_TEST_PROCEDURE.md` | ~45 numaralı donanım testi, M4 kapısı dahil |
| `docs/PC_TEST_REHBERI.md` | **Cihaz olmadan PC'de her şeyi test etme — adım adım** |

---

## 7. Dürüst durum

### Yapılanlar

- 74 task'ın tamamı uygulandı; her birinde tasarım ve inceleme kaydı var
- Firmware derleniyor: **flash %66,9 · RAM %23,9** (`-Wall -Wextra`, 0 uyarı)
- 134 `static_assert` + 46 Unity testi yazıldı
- Frontend tarayıcıda uçtan uca doğrulandı: giriş, kurulum sihirbazı, ürün
  profili uygulama, hedef bantlı ölçüm kartları, tavsiye motoru, sensör açma
  ve kalibrasyon, geçmiş grafiği, uzman modu, mobil yerleşim
  (iyimser güncelleme yasağı dahil)
- Sistem geneli kod denetimi yapıldı; bulunan 11 kusurun **10'u kapatıldı**
  (TASK-071 · 072 · 073), kalan biri ISSUE-037 olarak kayıtlı

### YAPILMAYANLAR — hepsi donanım gerektiriyor

```
[ ] Sistem hicbir zaman BOOT ETMEDI
[ ] Bes task'in birlikte calismasi
[ ] Dort veri akisinin gecikmesi
[ ] Task periyotlari, stack watermark, heap kararliligi
[ ] 24 saat / 72 saat kararlilik
[ ] Ariza enjeksiyonunun tamami (TASK-061'in ozu)
[ ] M4 GUVENLIK KAPISI
[ ] Host testleri CALISTIRILMADI (bu makinede gcc/g++ yok; derlenebilirligi
    hedef derleyiciyle dogrulandi)
[ ] Uc yeni rolenin (isik 19, isitici 26, dozaj 18) polaritesi olculmedi
[ ] AHT20 ve BH1750 hicbir zaman OKUNMADI — I2C adresleri, hat yuku ve
    CRC yolu donanimda dogrulanmadi
[ ] Urun profili GERCEK BIR CIHAZDA uygulanmadi (tarayicida sahte cihazla
    dogrulandi)
[ ] Donem otomatik ilerlemesi gercek zamanda gozlenmedi (gunler surer)
```

### Kritik açık maddeler

| # | Madde | Neden önemli |
|---|---|---|
| **ISSUE-003** | Röle polaritesi doğrulanmadı | **Güvenlik zincirinin tamamı** `RELAY_ACTIVE_LOW` varsayımına dayanıyor |
| ISSUE-024 | Stack boyutları tahmin | İlk çalıştırmada `minStack` ile düzeltilmeli |
| ~~ISSUE-021~~ | ~~Kural düzenleme arayüzü yok~~ | ✅ Çözüldü — kural API'si + düzenleyici eklendi, şema sürümü 2 |
| ISSUE-005 | Donanımsal RTC kararı | Ağsız çizelge çalışmaz — satın alma kararı |
| ISSUE-033 | Host derleyicisi (gcc/g++) yok | PlatformIO ve Python **kuruldu**; firmware artık derleniyor ve `data/` üretiliyor. `pio test -e native` hâlâ **koşmuyor** — `native` ortamı sistemin C++ derleyicisini ister. Çözüm: MinGW-w64 kurmak. |
| ~~ISSUE-014~~ | ~~Akış katsayısı (450 darbe/L) teyitsiz~~ | ✅ Çözüldü (TASK-074) — sahada çalışan formülden türetildi: **270 darbe/L**. Eski değer debiyi 1,67 kat düşük gösteriyordu |
| ~~ISSUE-034~~ | ~~Geçmiş kaydı slot sırası uyuşmazlığı~~ | ✅ Çözüldü (TASK-073) — sıra tek tabloda (`SLOT_ORDER`), 8 slot, dosya sürüm 2 |
| ~~ISSUE-035~~ | ~~Sensörler arayüzden açılamıyor~~ | ✅ Çözüldü (TASK-073) — `PUT /api/config/sensors`, "Takılı sensörler" ekranı, kalibrasyon, çalışma anında sürücü başlatma |
| ~~ISSUE-036~~ | ~~OLED yeni donanımı görmüyor~~ | ✅ Çözüldü (TASK-073) — 8 sensör / 5 aktüatör, kayan pencere |
| ISSUE-037 | GPIO kesmesi IRAM'de kayıtlı değil | Flash yazarken (geçmiş kaydı her 60 sn) encoder geçişleri düşer. TASK-071 bunu zararsız kıldı ama kaynağı duruyor |

### M4 kapısı

`IMPLEMENTATION_PLAN.md`: *"M4 doğrulanmadan otomasyon başlatılmaz."*
M4 = pompanın yalnızca güvenlik izniyle çalıştığının **donanımda**
kanıtlanması.

**Kapı kapalı.** Otomasyon kodu, kural düzenleyici ve ürün profilleri hazır,
ancak varsayılan mod `MANUAL`, varsayılan kural kümesi boş, elle eklenen her
kural **devre dışı doğar** ve ürün seçilmemiştir; sistem kutudan çıktığında
**kendiliğinden hiçbir aktüatörü sürmez**.

Ürün profili uygulamak bunu **değiştirmez**: profil kuralları etkin üretir ama
`automation.mode`'a dokunmaz ve motor MANUEL modda hiçbir kuralı
değerlendirmez. Sihirbazın son ekranı ve Ayarlar → Otomatik çalışma bölümü
bunu açıkça yazar; moda geçişte onay istenir.

### Sıradaki iş

0. **Host derleyicisini kur** (ISSUE-033'ün kalanı): MinGW-w64 →
   `pio test -e native` ile 37 host testini bir kez koştur.
   *Bundan sonrası donanım işidir:*
1. Röle polaritesini ölç — **beş röle** → ISSUE-003'ü kapat
2. I2C hattını tara: AHT20 (0x38) ve BH1750 (0x23) yanıt veriyor mu
3. İlk boot: seri porttan boot raporunu oku
4. `GET /api/diagnostics` → `minStack` ile stack'leri düzelt
5. `docs/HARDWARE_TEST_PROCEDURE.md` §2'yi koştur → M4 kapısı
6. M4 geçtikten **sonra** ürün profilini uygula ve otomasyonu `AUTO`'ya al
