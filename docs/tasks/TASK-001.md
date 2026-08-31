# TASK-001 — Project Skeleton & Layer Structure

**Phase:** 0 — Project Foundation · **Priority:** P0

## Objective

Yeni mimarinin katman sınırlarını dosya sistemi düzeyinde fiziksel olarak kurmak. Katman
ihlallerinin klasör yapısına bakılarak fark edilebilmesini sağlamak.

## Scope

- `ARCHITECTURE.md` §17'deki klasör yapısını oluşturmak (`core/`, `hal/`, `services/`, `domain/`, `interfaces/`, `tasks/`)
- Her klasöre, o katmanın sorumluluğunu ve bağımlılık kurallarını anlatan kısa bir `README.md` koymak
- Eski `src/*.cpp` ve `include/*.h` dosyalarının yeni yapıya taşınmayacağını netleştirmek; eskiyi `legacy/` altına referans olarak arşivlemek veya build dışında bırakmak
- Boş ama derlenen bir `main.cpp` iskeleti (yalnızca `setup()`/`loop()` gövdesi, iş mantığı yok)

## Out of Scope

- Herhangi bir modülün implementasyonu
- Build flag'leri, partition tablosu, pin tanımları (TASK-002)
- Kodlama standardı dokümanı (TASK-003)
- Eski kodun herhangi bir parçasının yeni klasörlere kopyalanması

## Dependencies

- Yok (ilk task)

## Requirements

- `REQUIREMENTS.md` — Kritik Problem 6 (ölü kod yükü)

## Architecture References

- §1.2 Bağımlılık kuralları (D1–D6)
- §17 Modül ve klasör yapısı
- §0 P7 (yazılmayan kod yoktur)

## Expected Design

- Klasör yapısı **bağımlılık yönünü** yansıtmalı: `domain/` yalnızca `core/`'a bakabilir,
  `hal/` hiçbir üst katmanı tanımaz.
- `Define.h` benzeri toplayıcı header **oluşturulmayacak**. Bu, mevcut projedeki en büyük
  yapısal problemdir: tek header her şeyi her şeye bağlıyor.
- Eski koda karar: build'den tamamen çıkarmak mı, `legacy/` altına arşivlemek mi?
  Öneri: git geçmişi zaten koruduğu için çalışma ağacından çıkarmak; referans gerektiğinde
  git üzerinden okunur.

## Implementation Notes

- PlatformIO varsayılan olarak `src/` altındaki tüm `.cpp` dosyalarını derler; alt klasörler
  desteklenir. `lib/` altındaki kullanılmayan kütüphaneler (`Adafruit_SH1106-master`,
  `esp32_arduino_sqlite3_lib-master`) build süresini ve binary boyutunu etkiler.
- `data/` ve `Data/` klasör adı tutarsızlığı bu task'ta düzeltilmeli (git `Data/`, dosya
  sistemi `data/`).

## Files

- `src/main.cpp` (yeni, iskelet)
- `src/core/README.md`, `src/hal/README.md`, `src/services/README.md`,
  `src/domain/README.md`, `src/interfaces/README.md`, `src/tasks/README.md` (yeni)
- `lib/` — kullanılmayan kütüphanelerin kaldırılması
- Eski `src/*.cpp`, `include/*.h` — build dışına alınması

## Acceptance Criteria

- [ ] Katman klasörleri oluşturuldu ve her birinde sorumluluk/bağımlılık kuralını anlatan README var
- [ ] Proje boş iskeletle derleniyor ve karta yüklenebiliyor
- [ ] Eski kaynak dosyaları build'e dahil değil
- [ ] Kullanılmayan kütüphaneler (`SH1106`, `sqlite3`) projeden kaldırıldı
- [ ] Toplayıcı header (`Define.h` benzeri) yok
- [ ] `data/` klasör adı tutarlı

## Test Plan

- [ ] `pio run` başarılı, uyarı yok
- [ ] `pio run -t upload` sonrası kart boot ediyor, seri portta panik/reset döngüsü yok
- [ ] Binary boyutu eski projeye göre ölçüldü ve kaydedildi (sqlite kaldırma etkisi)

## Review Checklist

- [ ] Architecture'a uygun mu? (§17 klasör yapısı)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? (N/A)
- [ ] Memory problemi var mı? (binary boyutu ölçüldü mü)
- [ ] Error handling var mı? (N/A)
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **hiçbir eski dosya taşınmamalı**

## Definition of Done

Ortak DoD (bkz. `IMPLEMENTATION_PLAN.md` §2.3) + boş iskelet kartta çalışıyor +
eski kodun hiçbir satırı yeni klasörlere kopyalanmamış.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Eski kod nereye gidecek?

```text
Problem:      Eski src/ ve include/ dosyaları build dışına alınmalı
Constraints:  TASK-002 pin haritası için eski pin bilgisi gerekli;
              TASK-021/025 encoder ve akış davranışı için referans gerekli;
              git geçmişi zaten koruyor ama çalışma sırasında erişim pratik olmalı
Approaches:   (a) tamamen sil — git'ten okunur
              (b) legacy/ altına taşı — build dışında, elde kalır
              (c) src/legacy/ altına taşı — PlatformIO yine derler, YANLIŞ
Trade-offs:   (a) her referans ihtiyacında git komutu gerekir
              (b) sonraki task'lar (002, 021, 025) dosyaları doğrudan okuyabilir
Selected:     (b) — kök dizinde `legacy/`. PlatformIO yalnızca src/ ve lib/
              derlediği için build'e girmez. TASK-065'te silinecek.
```

## Karar 2 — Kullanılmayan kütüphaneler

```text
Ölçüm:        lib/esp32_arduino_sqlite3_lib-master = 66 MB
              lib/Adafruit_SH1106-master           = 73 KB
Selected:     İkisi de çalışma ağacından SİLİNİR (legacy/'ye taşınmaz).
Gerekçe:      66 MB'lık vendor kodu legacy/ altında taşımak anlamsız;
              ikisi de kamuya açık ve git geçmişinde mevcut.
              ARCHITECTURE §15.2: SQLite kaldırma kararı zaten verilmiş.
```

## Karar 3 — `Data/` vs `data/` klasör adı

```text
Problem:      Git `Data/` takip ediyor, dosya sistemi `data/` gösteriyor.
              Büyük/küçük harf duyarlı sistemde LittleFS imajı üretilemez.
Constraints:  Mevcut web varlıkları (bootstrap 298 KB dahil) TASK-047'de
              sıfırdan üretilecek — korunmalarına gerek yok
Selected:     Data/ içeriği legacy/data/'ya taşınır, index'ten düşer.
              Yeni boş `data/` oluşturulur (.gitkeep ile).
Sonuç:        Case tutarsızlığı ortadan kalkar; 298 KB'lık sıkıştırılmamış
              bootstrap build dışına çıkar.
```

## Karar 4 — Katman klasörleri ve README içeriği

ARCHITECTURE §17'deki yapı birebir uygulanır. Her README **iki soruyu** yanıtlar:
"bu katman neyden sorumlu" ve "hangi katmanları include edebilir". Bu, katman
ihlalinin dosya açılır açılmaz fark edilmesini sağlar.

## Karar 5 — `main.cpp` iskeleti

```text
Constraints:  Boot sırası TASK-010, task oluşturma TASK-011/013 kapsamında
              → bu task'ta iş mantığı YAZILMAZ
              Ancak boş `loop()` sürekli CPU harcar (REQUIREMENTS §10 bulgusu)
Selected:     setup() boş (yalnızca yer tutucu yorum),
              loop() içinde vTaskDelay ile pasif bekleme.
Gerekçe:      Bir sonraki task'ta yazılacak kodun yerini işaretler ama
              o kodu yazmaz; aynı zamanda bilinen bir anti-pattern'i
              baştan engeller.
```

## Kapsam dışı bırakılanlar (bilinçli)

- `platformio.ini` içeriği, partition, pin haritası → TASK-002
- Kodlama standardı dokümanı → TASK-003
- `test/` klasörü içeriği → TASK-064

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Katman klasörleri oluşturuldu, her birinde sorumluluk + bağımlılık kuralı README'si var
- [ ] **Proje boş iskeletle derleniyor — BLOKE (ISSUE-007)**
- [x] Eski kaynak dosyaları build'e dahil değil (`legacy/` altında, PlatformIO yalnızca `src/` ve `lib/` derler)
- [x] Kullanılmayan kütüphaneler kaldırıldı (SH1106 73 KB + sqlite3 **66 MB**)
- [x] Toplayıcı header yok — `src/` altında hiç `.h` dosyası yok (0 adet)
- [x] `data/` klasör adı tutarlı; `Data/` index'ten düştü

## Test Plan

- [ ] `pio run` başarılı — **BLOKE (ISSUE-007)**
- [ ] Kart boot ediyor — build bloke olduğu için yapılamadı
- [ ] Binary boyut karşılaştırması — build bloke olduğu için yapılamadı
      (referans: eski `firmware.bin` = 961 152 bayt, sqlite dahil)

## Review Checklist

- [x] Architecture'a uygun mu? — §17 klasör yapısı birebir uygulandı
- [x] Gereksiz abstraction var mı? — hayır, yalnızca klasör + README
- [x] Blocking işlem var mı? — `loop()` içinde `vTaskDelay`, boş döngü yok
- [x] Shared state güvenli mi? — N/A (henüz state yok)
- [x] Memory problemi var mı? — 66 MB vendor kodu kaldırıldı
- [x] Error handling var mı? — N/A
- [x] ESP32 resource kullanımı uygun mu? — N/A
- [x] Task sorumluluğu doğru mu? — N/A
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — HAYIR.** `src/` altında yalnızca
      `main.cpp` (24 satır, sıfırdan yazıldı) ve 6 README var. Eski dosyaların
      hiçbiri yeni katman klasörlerine taşınmadı; tamamı `legacy/` altına git
      rename olarak (geçmiş korunarak) alındı.

## Bulgular

**ISSUE-007** kaydedildi: `me-no-dev/ESPAsyncWebServer.git` URL'i artık
ESP32Async/ESPAsyncWebServer 3.6.0'a yönleniyor ve framework'ün senkron
`WebServer` kütüphanesini bağımlılık grafiğine sokarak derlemeyi kırıyor.

Bu durum **TASK-001 değişikliklerinden kaynaklanmıyor**; son başarılı derlemeden
(12 Haziran) beri oluşan sürüm kaymasının sonucudur — eski kod da şu an derlenmez.
Düzeltme `platformio.ini` gerektirir; bu dosya TASK-001'in Files listesinde değil,
TASK-002 scope'undadır. Scope creep protokolü uyarınca çözülmedi, kaydedildi.

## Durum

**TASK-001: KOŞULLU TAMAMLANDI** — yapısal işin tamamı bitti; derleme doğrulaması
TASK-002 ISSUE-007'yi çözdükten sonra geriye dönük yapılacak.

---

## Geriye dönük doğrulama (TASK-002 sonrası)

ISSUE-007 TASK-002'de çözüldükten sonra bloke olan kriterler doğrulandı:

- [x] Proje boş iskeletle derleniyor — `pio run` SUCCESS, 0 uyarı
- [x] `pio run` başarılı
- [x] Binary boyutu ölçüldü ve kaydedildi:
      **eski (SQLite dahil) 961 152 bayt → yeni iskelet 266 973 bayt**
      Depo boyutundan ayrıca 66 MB vendor kodu (sqlite3) düştü.
- [ ] Kart boot doğrulaması — fiziksel karta yükleme gerektirir, donanım erişimi
      olduğunda yapılacak (`pio run -t upload`)

**TASK-001: TAMAMLANDI** (karta yükleme dışında).
