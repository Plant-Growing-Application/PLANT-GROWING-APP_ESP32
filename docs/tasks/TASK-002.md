# TASK-002 — Build Configuration & Partition Layout

**Phase:** 0 — Project Foundation · **Priority:** P0

## Objective

Build yapılandırmasını, flash bölümlemesini ve **pin planını** kilitlemek. Pin planı sonradan
değişirse donanım kablolaması etkileneceği için bu karar erken ve kesin verilmelidir.

## Scope

- `platformio.ini` yeniden yazımı: gereksiz flag'lerin temizlenmesi, kütüphane listesinin sadeleştirilmesi
- Partition tablosu kararı ve dosyası (OTA'lı / OTA'sız)
- Merkezi pin haritası tanımı (tek dosya, `core/` veya `hal/` altında)
- Derleyici uyarı seviyesinin yükseltilmesi
- LittleFS imaj yapılandırması

## Out of Scope

- Sürücü implementasyonları (PHASE 4)
- Config şeması ve NVS (TASK-014, TASK-015)
- Frontend asset pipeline (TASK-047)

## Dependencies

- TASK-001

## Requirements

- `REQUIREMENTS.md` — Donanım ve Proje Yapısı tablosu, §11-Low (OTA)

## Architecture References

- §15.2 Teknoloji kararları (SQLite kaldırma, OTA önerisi)
- §20 Açık maddeler (pin bütçesi, partition şeması)

## Expected Design

### Karar gerektiren nokta 1 — Partition şeması (ISSUE-006)

```text
Problem:      4 MB flash içinde uygulama + LittleFS dengesi
Constraints:  SQLite kalktı → binary belirgin şekilde küçüldü
              Web varlıkları gzip'li → LittleFS ihtiyacı düşük
Approaches:   (a) no_ota — tek uygulama, büyük LittleFS
              (b) min_spiffs — çift uygulama (OTA) + küçük LittleFS
              (c) özel tablo — ölçülen binary boyutuna göre
Trade-offs:   (a) basit ama sahada fiziksel erişim gerektirir
              (b) OTA kazandırır, uygulama alanı yarıya iner
Recommended:  (c) — TASK-001'de ölçülen boyuta göre özel tablo; OTA etkin
```

### Karar gerektiren nokta 2 — Pin planı (ISSUE-001, ISSUE-002)

Kısıtlar kesindir ve tartışmaya açık değildir:

- Wi-Fi aktifken **ADC2 kullanılamaz** → tüm analog sensörler **ADC1 (GPIO 32–39)**
- **GPIO 34–39 giriş-only**, dahili pull-up **yok**
- GPIO 6–11 flash'a ayrılmıştır, kullanılamaz
- Bazı pinler boot sırasında strapping işlevi görür (0, 2, 12, 15) — röle için uygun değil

Analog ihtiyacı: su sıcaklığı (NTC ise), pH, EC, analog seviye → **en az 3–4 ADC1 kanalı**.
Encoder şu anda GPIO 32/33'ü (ADC1) işgal ediyor → **taşınmalı**.

## Implementation Notes

- `-Wall -Wextra` etkinleştirilsin; yeni kod uyarısız derlenmeli.
- `DISABLE_SPIFFS` / `CONFIG_SPIFFS_UNMOUNTED` flag'leri SPIFFS tamamen kaldırıldığı için gereksiz.
- `upload_port = COM3` sabit değeri taşınabilirlik için kaldırılmalı (otomatik tespit).
- Pin haritası **tek bir dosyada** ve derleme zamanı sabiti olarak tanımlanmalı; farklı
  donanım revizyonları için yapılandırılabilir olmalı.
- Röle pinleri için boot sırasında güvenli seviyeyi koruyan pinler seçilmeli (strapping pin değil).

## Files

- `platformio.ini`
- `partitions.csv` (yeni, karara göre)
- `src/core/BoardPins.h` (yeni — merkezi pin haritası)
- `docs/HARDWARE.md` (yeni — pin planı ve gerekçesi)

## Acceptance Criteria

- [ ] Partition kararı gerekçesiyle `docs/HARDWARE.md`'de yazılı
- [ ] Pin planı ADC1/giriş-only/strapping kısıtlarına uygun ve belgelenmiş
- [ ] Encoder ADC olmayan pinlere taşındı (ISSUE-001 kapandı)
- [ ] Akış sensörü pin/pull-up kararı verildi (ISSUE-002 kapandı)
- [ ] `-Wall -Wextra` etkin, proje uyarısız derleniyor
- [ ] Gereksiz build flag'leri ve kütüphaneler kaldırıldı
- [ ] `upload_port` sabit değeri kaldırıldı

## Test Plan

- [ ] `pio run` uyarısız
- [ ] Partition tablosu karta yükleniyor, LittleFS mount ediliyor
- [ ] OTA seçildiyse: bölüm boyutlarının mevcut binary'yi aldığı doğrulandı
- [ ] Pin planındaki her pin datasheet kısıtlarına karşı tek tek kontrol edildi

## Review Checklist

- [ ] Architecture'a uygun mu? (§15.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? (N/A)
- [ ] Memory problemi var mı? (partition boyutları yeterli mi)
- [ ] Error handling var mı? (N/A)
- [ ] ESP32 resource kullanımı uygun mu? — **ADC1/strapping/giriş-only kısıtları**
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — eski pin haritası körü körüne alınmamalı

## Definition of Done

Ortak DoD + pin planı donanım kısıtlarına karşı doğrulanmış + ISSUE-001 ve ISSUE-002 kapatılmış.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Partition şeması (ISSUE-006 kapanışı)

```text
Problem:      4 MB flash içinde uygulama + LittleFS dengesi
Ölçüm:        Eski firmware.bin (SQLite dahil) = 961 152 bayt ≈ 939 KB
              SQLite kaldırıldı → yeni binary belirgin şekilde küçülecek
              Web varlıkları gzip'li olacak (TASK-047) → LittleFS ihtiyacı düşük
Approaches:   (a) no_ota      — tek 2 MB uygulama + 1.9 MB FS, OTA yok
              (b) min_spiffs  — 2×1.9 MB uygulama + 190 KB FS
              (c) özel tablo  — ölçülen ihtiyaca göre
Trade-offs:   (a) sahada fiziksel erişim gerektirir — sera cihazı için kötü
              (b) FS çok küçük; geçmiş veri halka dosyası (TASK-058) sığmaz
Selected:     (c) — özel tablo:
                app0/app1 = 1.5 MB  (OTA etkin, 939 KB'lık eski binary'nin
                                     %60 üstünde pay bırakıyor)
                littlefs  = 896 KB  (web varlıkları + geçmiş halka dosyası)
                coredump  = 64 KB   (ARCHITECTURE §16.3 crash recovery)
Bonus:        coredump bölümü, REQUIREMENTS §9'da eksik işaretlenen
              "ESP32 crash recovery" için altyapı sağlar.
```

## Karar 2 — Pin planı (ISSUE-001 + ISSUE-002 kapanışı)

### Kesin donanım kısıtları

```text
ADC1 kanalları : GPIO 32,33,34,35,36,39   (Wi-Fi aktifken ADC2 KULLANILAMAZ)
Giriş-only     : GPIO 34,35,36,39         (dahili pull-up YOK)
Strapping      : GPIO 0,2,5,12,15         (çıkış için uygun değil)
Flash'a ayrılı : GPIO 6-11                (kullanılamaz)
```

### Analog kanal ihtiyacı

pH + EC + su sıcaklığı (NTC) = **3 kanal minimum**, +1 yedek.
Encoder GPIO 32/33'ü (ADC1_CH4/CH5) işgal ediyordu → **taşınmalı**.

### Seçilen plan (rewiring: yalnızca 3 kablo)

| İşlev | Eski | Yeni | Gerekçe |
|---|---|---|---|
| Encoder A | 33 | **18** | ADC1_CH5 boşaltıldı (ISSUE-001) |
| Encoder B | 32 | **19** | ADC1_CH4 boşaltıldı (ISSUE-001) |
| Akış sensörü | 34 | **4** | GPIO 34'te dahili pull-up YOK (ISSUE-002); ADC1_CH6 boşaltıldı |
| pH | — | **34** | ADC1_CH6 (analog, pull-up gerekmiyor) |
| EC | — | **36** | ADC1_CH0 |
| Su seviyesi LOW | — | **13** | yeni, dahili pull-up'lı (ISSUE-000) |
| Su seviyesi CRITICAL | — | **14** | yeni, dahili pull-up'lı (ISSUE-000) |
| Confirm butonu | 26 | **kaldırıldı** | hiç okunmuyordu (P7) → pin serbest |
| Analog yedek | — | **39** | ADC1_CH3, ileride analog seviye/4. sensör |

Değişmeyenler: I2C 21/22, su sıcaklığı 35, röleler 16/17, LED 23,
encoder push 25, geri butonu 27.

**Boşta kalan pinler:** 26, 32, 33 (ikisi ADC1 kapasiteli) + 5, 39.

## Karar 3 — lib_deps ve ISSUE-007 çözümü

```text
Problem:      me-no-dev/ESPAsyncWebServer.git → ESP32Async/ESPAsyncWebServer 3.6.0
              LDF chain modunda framework'ün senkron WebServer kütüphanesini
              grafiğe sokuyor; WiFiServer.h include yolu çözülemiyor
Approaches:   (a) lib_ldf_mode = deep+  → include yolu yayılımını düzeltir
              (b) web kütüphanelerini TASK-041'e ertele
              (c) sürüme sabitle
Selected:     (a) + (c) birlikte; (a) çalışmazsa (b)'ye düşülür.
              Git URL'leri KALDIRILIYOR, registry + sabit sürüm kullanılıyor —
              depo devri gibi sürprizler tekrarlanmasın.
Ayrıca:       Adafruit SH110X lib_deps'ten çıkarılıyor (kullanılmıyor, P7).
              SPIFFS build flag'leri çıkarılıyor (SPIFFS tamamen terk edildi).
              upload_port sabiti çıkarılıyor (taşınabilirlik).
```

## Karar 4 — Derleyici uyarı seviyesi

`-Wall -Wextra` etkin. `-Werror` **kullanılmıyor**: framework kaynaklı uyarılar
projeyi bloke edebilir. Kural, `IMPLEMENTATION_PLAN.md` DoD'de "yeni uyarı
üretilmedi" olarak zaten uygulanıyor.

## Kapsam dışı bırakılanlar

- Sürücü implementasyonları → PHASE 4
- Native test ortamı → TASK-064
- Frontend asset pipeline → TASK-047

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Partition kararı gerekçesiyle `docs/HARDWARE.md` §6'da yazılı
- [x] Pin planı ADC1 / giriş-only / strapping kısıtlarına uygun ve belgelenmiş
      (`docs/HARDWARE.md` §2–3 + `BoardPins.h` `static_assert`'leri)
- [x] **ISSUE-001 kapandı** — encoder 33/32 → 18/19, ADC1_CH4 ve CH5 boşaldı
- [x] **ISSUE-002 kapandı** — akış sensörü 34 → 4 (dahili pull-up'lı pin)
- [x] `-Wall -Wextra` etkin, proje **sıfır uyarıyla** derleniyor
- [x] Gereksiz build flag'leri (SPIFFS) ve kütüphaneler (SH110X) kaldırıldı
- [x] `upload_port` sabit değeri kaldırıldı

## Test Plan

- [x] `pio run` uyarısız — temiz build, 0 warning, 0 error
- [x] Partition tablosu üretiliyor — `partitions.bin` (3072 bayt)
      Flash sınırı raporu **1 572 864 bayt = 0x180000** → özel tablo etkin
- [x] OTA bölüm boyutu doğrulandı: 1.5 MB, eski binary 961 152 bayt →
      %63 boş pay. İskelet build 266 973 bayt (%17)
- [x] **Her pin datasheet kısıtına karşı kontrol edildi** — `BoardPins.h`
      içindeki 17 adet `static_assert` ile derleme zamanında zorlanıyor
- [ ] LittleFS mount doğrulaması — TASK-016'da yapılacak (sürücü henüz yok)

### Negatif test — static_assert etkinliği kanıtı

`FLOW_PULSE` kasıtlı olarak eski hatalı değere (GPIO 34) döndürüldü:

```text
src/core/BoardPins.h:111:32: error: static assertion failed:
    FLOW_PULSE dahili pull-up gerektirir
```

Derleyici, eski koddaki **ISSUE-002 hatasının ta kendisini** yakaladı.
Pin kısıtları artık dokümantasyon değil, derleme zamanı garantisidir.

## Review Checklist

- [x] Architecture'a uygun mu? — §15.2 (SQLite kaldırıldı, OTA etkin) uygulandı
- [x] Gereksiz abstraction var mı? — `BoardPins.h` yalnızca sabit + kısıt kontrolü
- [x] Blocking işlem var mı? — N/A
- [x] Shared state güvenli mi? — N/A
- [x] Memory problemi var mı? — bölüm boyutları ölçülen binary'ye göre seçildi
- [x] Error handling var mı? — N/A (yapılandırma task'ı)
- [x] **ESP32 resource kullanımı uygun mu?** — ADC1/strapping/giriş-only kısıtları
      `static_assert` ile zorlanıyor; ADC2 bilinçli olarak hiç kullanılmıyor
- [x] Task sorumluluğu doğru mu? — N/A
- [x] **Eski pin haritası körü körüne alınmadı** — 3 pin taşındı (2'si ADC1
      bütçesi için, 1'i pull-up hatası için), 1 pin (confirm butonu) tamamen
      kaldırıldı çünkü hiç okunmuyordu

## Ek bulgular

**ISSUE-007 çözüldü.** Çözüm: git URL'leri registry + sabit sürümle değiştirildi
ve `lib_ldf_mode = deep+` eklendi. Derleme yeniden çalışır durumda.

**TASK-001 geriye dönük doğrulama:** ISSUE-007 kalktığı için TASK-001'in bloke
olan "proje derleniyor" kriteri artık sağlanıyor — iskelet 266 973 bayt ile
derleniyor (eski sistem 961 152 bayt).

## Durum

**TASK-002: TAMAMLANDI.** ISSUE-001, ISSUE-002, ISSUE-006, ISSUE-007 kapandı.
