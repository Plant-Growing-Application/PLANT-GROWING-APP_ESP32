# TASK-060 — System Integration Bring-Up

**Phase:** 14 — Integration & Hardening · **Priority:** **P0**

## Objective

Tüm alt sistemleri birlikte çalıştırmak ve uçtan uca akışların gerçek donanımda
çalıştığını doğrulamak. Bu task, bileşenlerin ayrı ayrı değil **birlikte** doğru
davrandığını kanıtlar.

## Scope

- Tüm task'ların birlikte çalıştırılması
- `ARCHITECTURE.md` §3'teki dört veri akışının uçtan uca doğrulanması
- Boot senkronizasyonunun doğrulanması
- Alt sistemler arası zamanlama etkileşimlerinin ölçülmesi
- Entegrasyon sorunlarının tespiti ve düzeltilmesi

## Out of Scope

- Arıza enjeksiyonu (TASK-061)
- Kaynak profilleme (TASK-062)
- Güvenlik sertleştirme (TASK-063)
- Yeni özellik ekleme — **kesinlikle kapsam dışı**

## Dependencies

- TASK-033, TASK-038, TASK-053, TASK-057

## Requirements

- `REQUIREMENTS.md` — tüm bölümler (entegrasyon doğrulaması)

## Architecture References

- §3 Veri akışı (dört akış) · §6 Task mimarisi · §7 Boot

## Expected Design

### Doğrulanacak dört akış (§3)

```text
  1. Sensör → SensorService → StateStore → Web        (telemetri)
  2. Sensör → SensorService → StateStore → Display
  3. Web/Display komutu → CommandQueue → app_core → Aktüatör
  4. Aktüatör durumu → StateStore → Web + Display     (geri besleme)
```

Her akış için **uçtan uca gecikme** ölçülmeli ve kaydedilmeli.

### Entegrasyonda beklenen sorun alanları

| Alan | Neden riskli |
|---|---|
| Zamanlama etkileşimi | Task'lar tek başına çalışırken görülmeyen gecikmeler |
| Bellek | Tüm alt sistemler açıkken heap yeterli mi |
| Boot sırası | Bir task, hazır olmayan bir servise erişebilir |
| I2C / ADC paylaşımı | Eşzamanlı erişim çakışması |
| Wi-Fi + ADC | Wi-Fi trafiği ADC okumalarını etkiliyor mu |

### Scope creep uyarısı

Bu task sırasında fark edilen eksik özellikler **eklenmez**. `docs/ISSUES.md`'ye kaydedilir.
Yalnızca **entegrasyon kaynaklı hatalar** düzeltilir.

## Implementation Notes

- Entegrasyon sırasında ortaya çıkan her sorun kaydedilmeli: hangi bileşenler arası,
  hangi koşulda.
- Boot senkronizasyonu (EventGroup) doğrulanmalı: hiçbir task hazır olmayan bir servise
  erişmemeli.
- Tüm task'ların gerçek periyotları ölçülmeli ve tasarım hedefleriyle karşılaştırılmalı.
- Heap kullanımı, tüm alt sistemler aktifken ölçülmeli (Wi-Fi + web + WS istemcileri +
  OLED + sensörler).
- Uçtan uca gecikmeler ölçülmeli: butona basmadan röle sesine kadar, sensör değişiminden
  ekranda görünmesine kadar.
- Bu aşamada henüz otomasyon aktif edilmemeli mi? Hayır — TASK-057 tamamlandığı için
  otomasyon dahil tüm sistem test edilir. Ancak **M4 kapısı** (güvenlik doğrulaması)
  daha önce geçilmiş olmalıdır.

## Files

- Entegrasyon düzeltmeleri (hangi dosyalar olacağı entegrasyon bulgularına bağlı)
- `docs/INTEGRATION_REPORT.md` (yeni — ölçümler ve bulgular)

## Acceptance Criteria

- [ ] Beş task birlikte kararlı çalışıyor
- [ ] Dört veri akışı uçtan uca doğrulandı
- [ ] Her akışın gecikmesi ölçülüp kaydedildi
- [ ] Boot senkronizasyonu doğrulandı; erken erişim yok
- [ ] Tüm task periyotları ölçüldü ve hedefte
- [ ] Heap kullanımı tüm sistem aktifken ölçüldü
- [ ] I2C ve ADC paylaşımında çakışma yok
- [ ] Wi-Fi trafiğinin ADC okumalarını etkilemediği doğrulandı
- [ ] Entegrasyon bulguları raporlandı
- [ ] Kapsam dışı sorunlar `ISSUES.md`'ye kaydedildi, çözülmedi

## Test Plan

- [ ] Web'den pompa komutu → röle çalışıyor → web ve OLED'de durum güncelleniyor
- [ ] OLED'den komut → aynı zincir doğrulandı
- [ ] Sensör değişimi → web ve OLED'de görünüyor (gecikme ölçüldü)
- [ ] Otomasyon kuralı tetikleniyor → pompa çalışıyor → arayüzlerde görünüyor
- [ ] Güvenlik vetosu → komut reddediliyor → arayüzlerde neden görünüyor
- [ ] Tüm task periyotları yük altında ölçüldü
- [ ] 24 saat kesintisiz çalışma; WDT reset yok
- [ ] Heap 24 saat boyunca sabit
- [ ] Wi-Fi yoğun trafik altında sensör değerleri kararlı

## Review Checklist

- [ ] Architecture'a uygun mu? (§3, §6, §7)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — entegrasyonda ortaya çıkan gecikmeler
- [ ] Shared state güvenli mi? — gerçek eşzamanlılık altında
- [ ] Memory problemi var mı? — **tüm sistem aktifken heap**
- [ ] Error handling var mı?
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı?

## Definition of Done

Ortak DoD + **24 saat kesintisiz çalışma** + dört akışın gecikmeleri ölçülüp raporlandı +
kapsam dışı bulgular `ISSUES.md`'ye kaydedildi.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 0 — ISSUE-013/018 BU TASK'TA KAPATILIYOR

```text
TASK-060 kapsam maddesi 1: "Tum task'larin birlikte calistirilmasi"
Gercek durum: `main.cpp` iskelet, boot asama tablosu YOK, TaskDef tablosu
              YOK, bes task giris noktasi HICBIR YERDEN cagrilmiyor.

Sistem HIC BOOT ETMEDI. Once 33 task'ta yazilan hicbir sey calismadi.
```

Boot wiring planda **hiçbir task'a atanmamıştı** (ISSUE-013, sonra
ISSUE-018). Ancak "tüm task'ları birlikte çalıştırmak" onsuz **tanım
gereği imkânsız**. Bu yüzden wiring bu task'ın kapsamına alındı — yeni bir
özellik değil, var olan parçaların bağlanması.

**Kapsam sınırı korunuyor:** yeni işlev EKLENMİYOR. Yalnızca TASK-002…059
arasında yazılmış modüller boot sırasına ve task tablosuna bağlanıyor.

## Karar 1 — Aşama tablosu ARCHITECTURE §7.1 ile birebir

```text
0 RESET_AND_WDT    ZORUNLU  reset nedeni + TWDT           (log ALTYAPISINDAN ONCE)
1 GPIO_SAFE_STATE  ZORUNLU  TUM ROLELER KAPALI            (her seyden once)
2 CORE_SERVICES    ZORUNLU  Diagnostics, StateStore, kuyruklar
3 CONFIG_LOAD               NVS → basarisizsa VARSAYILAN
4 FILESYSTEM                LittleFS → basarisizsa web statigi yok
5 DISPLAY_HW                OLED → basarisizsa sistem TAM calisir
6 SENSOR_HW                 ADC + PCNT → basarisizsa sensor kalitesi duser
7 NETWORK_RADIO             Wi-Fi → basarisizsa OFFLINE; GUVENLIK ETKILENMEZ
8 TASK_CREATION    ZORUNLU  bes task
```

**Aşama 1'in Aşama 0'dan hemen sonra gelmesi pazarlıksız:** röleler boot'un
ilk milisaniyelerinde güvenli konuma alınmalı; log altyapısı bile henüz
hazır olmayabilir. Pompanın korunması loglamadan önceliklidir.

## Karar 2 — Zorunlu olmayan aşamalar boot'u DURDURMAZ (P4)

Wi-Fi yoksa sistem sulamaya devam eder. OLED yoksa web çalışır. Dosya
sistemi yoksa API çalışır. Eski sistemde bir başarısızlık akışı sessizce
kesiyordu; burada her aşama sonucu **rapora yazılıyor** ve mod ondan
türetiliyor.

## Karar 3 — `loop()` KULLANILMIYOR

Arduino `loop()` boş kalır ve pasif bekler. Tüm iş FreeRTOS task'larında.
`loopTaskWDTEnabled = false` zaten (TASK-010) — `loop()` beslenmediği için
watchdog tetiklemesin diye.

## Karar 4 — Boot senkronizasyonu: aşama SIRASI, EventGroup DEĞİL

```text
Plan EventGroup oneriyordu. Gerek YOK:
  · Task'lar Asama 8'de olusturuluyor — yani config (3), dosya sistemi (4),
    OLED (5), sensor donanimi (6) ve radyo (7) ZATEN hazir.
  · Bir task olusturuldugu anda bagimliliklari karsilanmis durumda.

EventGroup eklemek, sirali bir akisa senkronizasyon primitifi koymak
olurdu — karmasiklik karsiliginda hicbir sey kazandirmadan.
```

Yine de her servis `begin()` içinde kendi ön koşulunu kontrol ediyor
(örn. `SensorService` config yüklü mü); bu ikinci savunma hattı korunuyor.

## Karar 5 — Ölçümler DONANIM GEREKTİRİYOR

Bu task'ın kabul kriterlerinin çoğu (dört akışın gecikmesi, task
periyotları, 24 saat kararlılık, heap) **çalışan donanım** ister. Kod
tarafı bu turda tamamlanıyor; ölçümler `docs/INTEGRATION_REPORT.md`'de
**açıkça işaretsiz** bırakılıyor. Ölçülmemiş bir değeri raporlamak,
raporlamamaktan kötüdür.

---

# STEP 3 — REVIEW RECORD

- [x] **Boot wiring yazıldı** — ISSUE-013/018 KAPANDI. 9 aşamalık boot
      tablosu + 5 task tablosu + gerçek `main.cpp`
- [x] Aşama tablosu `BootStage` ile birebir — `static_assert` ile kilitli
- [x] Zorunlu/zorunlu-olmayan ayrımı §7.1 ile aynı; hiçbir zorunlu-olmayan
      aşama boot'u durdurmuyor (P4)
- [x] `loop()` kullanılmıyor
- [x] Entegrasyon bulguları `docs/INTEGRATION_REPORT.md`'de
- [x] Kapsam dışı bulgular `ISSUES.md`'ye kaydedildi, çözülmedi
- [ ] **Beş task'ın birlikte çalışması — DONANIM GEREKİYOR**
- [ ] **Dört akışın gecikmesi — ÖLÇÜLMEDİ**
- [ ] **24 saat kararlılık — ÖLÇÜLMEDİ**
- [ ] **Heap, I2C/ADC çakışması, Wi-Fi etkisi — ÖLÇÜLMEDİ**

## En önemli bulgu: önceki tüm boyut ölçümleri anlamsızdı

```text
              ONCE (bagli degil)   SONRA (bagli)
Flash         %27.8   437 253      %65.4  1 028 181
RAM statik    % 7.2    23 580      %22.0     72 208
```

Task giriş noktaları hiçbir yerden çağrılmadığı için linker uygulamanın
**tamamını** ölü kod olarak atıyordu. 33 task boyunca raporladığım flash
rakamları yalnızca `core/` ve birkaç `hal/` dosyasını ölçüyormuş.

Gerçek rakamlar bütçe içinde (uygulama bölümü 1,5 MB, kullanım %65.4).

## Boot wiring'in açığa çıkardığı link hatası

`AsyncCallbackJsonWebHandler` kurucusu çözümlenemedi:
`AsyncJson.cpp` tamamen `#if ASYNC_JSON_SUPPORT == 1` altında ve bu bayrak
`__has_include("ArduinoJson.h")` ile belirleniyor — PlatformIO her
kütüphaneyi kendi include yollarıyla derlediği için ArduinoJson o derleme
birimine görünmüyor.

Bu hata **ancak zincir canlandığında** ortaya çıktı; daha önce linker o
kodu hiç bağlamıyordu. Kütüphanenin sınıfı yerine `JsonBody.cpp` yazıldı;
gövde tamponu `request->_tempObject` içinde (yıkıcı `free()` ediyor).

## Kapsam disiplini

Bu turda **hiçbir yeni özellik eklenmedi.** Eklenenlerin tamamı ya var olan
modüllerin bağlanması ya da `ARCHITECTURE.md`'de tanımlı olup uygulanmamış
davranışlar (§16.3 heap satırı).

**TASK-060: KOD TARAFI TAMAMLANDI** (tüm ölçümler donanım bekliyor).
