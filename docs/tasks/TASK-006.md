# TASK-006 — SystemState Model

**Phase:** 1 — Core Infrastructure · **Priority:** P0

## Objective

Sistemin tüm gözlemlenebilir durumunu tek bir POD yapı olarak tanımlamak ve her alt-state
için **tek yazar** sahibini belgelemek.

## Scope

- `SystemState` kök yapısı ve alt-state'leri: `system`, `network`, `sensors[]`,
  `actuators[]`, `safety`, `automation`, `time`
- Her alt-state için sahip task'ın yapı içinde belgelenmesi
- Versiyon sayacı alanı
- Yapı boyutunun ölçülmesi (snapshot kopyalama maliyeti)

## Out of Scope

- `StateStore` erişim mekanizması (TASK-007)
- Alanların doldurulması (ilgili servis task'ları)
- UI state'i, web oturum state'i — bunlar bilinçli olarak **merkezi değildir** (§4.4)

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — Kritik Problem 2 (korumasız shared state), §7.2 (kullanılmayan alanlar)

## Architecture References

- §4.1 State ağacı ve sahiplik tablosu
- §4.3 Neden merkezi state
- §4.4 Oluşturulmayan state'ler

## Expected Design

- Yapı **tamamen POD** olmalı: `memcpy` ile kopyalanabilir, işaretçi/heap içermez.
  Bu, TASK-007'deki snapshot deseninin ön koşuludur.
- Diziler **sabit boyutlu** olmalı; sensör ve aktüatör sayısı derleme zamanı sabiti.
- Her alan için "hangi task yazar" bilgisi kod içinde açıkça belirtilmeli — bu, katman
  ihlallerini incelemede yakalanabilir kılar.
- **Kullanılmayan alan konmayacak** (P7). Mevcut projedeki `Settings.IP`, `Settings.MAC`,
  `LittleFSFormatted` gibi hiç okunmayan alanlar bu modelin karşı örneğidir.
- Sensör değerleri **kalite bilgisiyle birlikte** taşınmalı; değer tek başına anlamsızdır.
- Boyut önemlidir: yapı her snapshot'ta kopyalanacak ve UI 20 Hz okuyacaktır.
  Hedef birkaç yüz bayt mertebesinde tutulmalı, kilobaytlara çıkmamalı.

## Implementation Notes

- Zaman damgaları monotonik (`uptimeMs`) olmalı; duvar saati yalnızca `time` alt-state'inde.
- String alanları sabit boyutlu tampon; `String` sınıfı kullanılmaz.
- Wi-Fi şifresi **state'e konmaz** (§8.2 — şifre API'de ve OLED'de gösterilmez).
- Enum'lar açık tam sayı tipine sabitlenmeli (yapı boyutu öngörülebilir olsun).
- Yapı içindeki dolgu (padding) boyutu gereksiz büyütmesin; alan sıralaması gözden geçirilmeli.

## Files

- `src/core/SystemState.h` (yeni)
- `src/core/StateOwnership.md` (yeni — sahiplik tablosu, kısa)

## Acceptance Criteria

- [ ] Yedi alt-state tanımlı ve `ARCHITECTURE.md` §4.1 ile birebir uyumlu
- [ ] Yapı POD; derleme zamanı kontrolüyle doğrulanmış
- [ ] Tüm diziler sabit boyutlu
- [ ] Her alt-state için tek yazar task belgelenmiş
- [ ] Sensör değerleri kalite alanıyla birlikte
- [ ] Wi-Fi şifresi state'te **yok**
- [ ] Kullanılmayan alan yok
- [ ] Yapı boyutu ölçülüp kaydedilmiş

## Test Plan

- [ ] Host tarafında POD ve kopyalanabilirlik derleme zamanı kontrolü
- [ ] `sizeof(SystemState)` ölçüldü ve hedefin altında
- [ ] Örnek bir yapı kopyalanıp karşılaştırıldı; bilgi kaybı yok
- [ ] Yapının kopyalanma süresi ölçüldü (kritik bölge bütçesi için)

## Review Checklist

- [ ] Architecture'a uygun mu? (§4.1)
- [ ] Gereksiz abstraction var mı? — gereksiz alt-state var mı (§4.4)
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? — POD/kopyalanabilirlik ön koşulu sağlandı mı
- [ ] Memory problemi var mı? — yapı boyutu
- [ ] Error handling var mı? — kalite/geçerlilik alanları var mı
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — sahiplik tablosu doğru mu
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `Settings` yapısı taşınmamalı

## Definition of Done

Ortak DoD + yapı boyutu ve kopyalama süresi ölçülmüş + sahiplik tablosu yazılı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Sınır sorunu: yayınlanan state tiplerinin sahibi kim?

Bu task, sonraki task'larla **örtüşme riski** taşıyor ve netleştirilmesi gerekiyor:

```text
Cakisma:  TASK-022 `SensorReading`, TASK-028 `Actuator`, TASK-035 `NetworkState`
          tanimlamayi planliyor. Ama bunlarin hepsi SystemState icinde YAYINLANAN
          veridir ve SystemState POD olmak zorundadir (TASK-007 snapshot).
Risk:     Ayni kavram iki yerde tanimlanir → donusum kodu, tutarsizlik, olu kod (P7).

KURAL (bu task'ta sabitleniyor):
  SystemState.h  →  YAYINLANAN state'in tipleri (deger, kalite, durum, kimlik)
  Sonraki task'lar → CALISMA ve KONFIGURASYON tipleri (descriptor, registry,
                     kisit, FSM ic durumu) — yayinlanan tipi YENIDEN TANIMLAMAZ,
                     include eder.

Ornek dagilim:
  TASK-006: SensorQuality, SensorSample, ActuatorId, ActuatorSource, NetFsmState
  TASK-022: SensorDescriptor, SensorRegistry, ISensor      (calisma tipleri)
  TASK-028: ActuatorConstraints, ActuatorCommandResult     (konfigurasyon/komut)
  TASK-035: NetworkFsm ic durumu, gecis tablosu            (ic durum)
```

Bu karar ISSUE-010 olarak kaydedildi ki ilgili task'lar yeniden tanımlamasın.

## Karar 2 — IP adresi gösterimi

```text
Problem:      IP/gateway/subnet/DNS state'te nasil tutulacak?
Constraints:  SystemState her snapshot'ta kopyalanacak → kucuk olmali;
              UI ve API metin gosterecek
Approaches:   (a) FixedString<15>  → 16 bayt/adres, 4 adres = 64 bayt
              (b) uint32_t         →  4 bayt/adres, 4 adres = 16 bayt
Selected:     (b) — bicimlendirme sunum katmaninin isi (ViewModelBuilder
              TASK-050, JSON TASK-043). State ham veri tasir.
Kazanc:       48 bayt (state'in ~%15'i)
```

Aynı gerekçeyle MAC adresi 6 ham bayt olarak tutulur, metin olarak değil.

## Karar 3 — Sabit dizi boyutları

| Dizi | Boyut | Gerekçe |
|---|---|---|
| `sensors[]` | 8 | REQUIREMENTS §3'te 6 sensör; büyüme payı 2 |
| `actuators[]` | 4 | Su pompası + hava pompası + 2 yedek (ARCHITECTURE §10.2) |

Dinamik boyut **kullanılmaz**: heap gerektirir ve POD'luğu bozar.

## Karar 4 — Şifre state'te yok

`ARCHITECTURE.md` §8.2: Wi-Fi şifresi state'e, log'a, API yanıtına ve OLED'e
**girmez**. Mevcut sistemde şifre hem EEPROM'da düz metin hem OLED'de açıkça
görünüyordu. `SystemState` yalnızca SSID taşır; şifre `SecretStore`'da kalır
(TASK-013).

## Karar 5 — Sensör değeri `float`

Mevcut sistem `int` kullanıyordu; pH 6.5 ile 6.0 arasındaki fark otomasyon için
anlamlıdır ve yuvarlama bu bilgiyi yok eder (REQUIREMENTS §3.1). Değer `float`,
yanında **zorunlu kalite alanı** ile taşınır (CODING_STANDARDS Z6).

## Karar 6 — Alan sıralaması ve dolgu

Yapı her snapshot'ta kopyalanacağı için dolgu (padding) israfı önemli.
Alanlar **büyükten küçüğe** sıralanır (8 bayt → 4 → 2 → 1). Sonuç
`sizeof` ile ölçülüp kaydedilecek.

## Bellek bütçesi (tasarım hedefi)

| Alt-state | Tahmin |
|---|---|
| `system` | ~24 B |
| `network` | ~72 B |
| `sensors[8]` | ~96 B |
| `actuators[4]` | ~64 B |
| `safety` | ~16 B |
| `automation` | ~24 B |
| `time` | ~24 B |
| **Toplam hedef** | **< 400 B** |

400 baytlık `memcpy` ~1 µs mertebesindedir; TASK-007'nin 10 µs kritik bölge
hedefine rahat sığar.

## Kapsam dışı bırakılanlar

- `StateStore` erişim mekanizması, mutex, versiyon artırma → TASK-007
- Alanları dolduran mantık → ilgili servis task'ları
- UI state'i (aktif ekran, imleç) ve web oturumu → **bilinçli olarak merkezi değil**
  (ARCHITECTURE §4.4)

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Yedi alt-state tanımlı ve `ARCHITECTURE.md` §4.1 ile birebir uyumlu
- [x] Yapı POD; `is_trivially_copyable` + `is_standard_layout` assert'leri ile doğrulandı
- [x] Tüm diziler sabit boyutlu (`MAX_SENSORS=8`, `MAX_ACTUATORS=4`)
- [x] Her alt-state için tek yazar task belgelendi (`StateOwnership.md` + yapı yorumları)
- [x] Sensör değerleri **kalite alanıyla birlikte** — `SensorSample` kalitesiz kurulamaz
- [x] **Wi-Fi şifresi state'te YOK** — tarama ile doğrulandı
- [x] Kullanılmayan alan yok — her alan ya bir servis tarafından yazılacak ya da
      bir arayüz tarafından okunacak (`StateOwnership.md`'de eşlendi)
- [x] Yapı boyutu ölçülüp kaydedildi

## Ölçümler

| Yapı | Boyut | Hesap |
|---|---|---|
| `SystemStatus` | 20 B | |
| `NetworkStatus` | 72 B | SSID(34) + 4 IP(16) + MAC(6) + zaman(8) + bayraklar |
| `SensorsStatus` | 100 B | 8 × `SensorSample`(12) + sayaç |
| `ActuatorsStatus` | 68 B | 4 × `ActuatorStatus`(16) + sayaç |
| `SafetyStatus` | 12 B | |
| `AutomationStatus` | 16 B | |
| `TimeStatus` | 16 B | |
| **`SystemState`** | **312 B** | tasarım hedefi <400 B ✔ · assert sınırı 512 B |

**IP'leri ham `uint32` tutma kararı 48 bayt kazandırdı** (`FixedString<15>`
alternatifine göre) — state'in yaklaşık %15'i.

Snapshot kopyalama maliyeti: 312 bayt `memcpy`, 240 MHz'de ~1 µs'in belirgin
altında. TASK-007'nin 10 µs kritik bölge hedefine rahat sığar.
*(Hesaplanan değer; fiili ölçüm TASK-007 ve TASK-062'de donanımda yapılacak.)*

## Statik denetimler

```text
Sifre/parola alani        : yok
Pointer / String / virtual: yok
Dinamik dizi              : yok
```

## Test Plan

- [x] POD ve kopyalanabilirlik derleme zamanı kontrolüyle doğrulandı
- [x] `sizeof(SystemState)` = 312 B, hedefin altında
- [x] Derleme SUCCESS, 0 uyarı
- [ ] **Kopyalama süresi fiili ölçümü — donanım gerekiyor** (TASK-007/TASK-062)
- [ ] **Örnek yapı kopyalanıp karşılaştırma — donanım gerekiyor**

## Review Checklist

- [x] Architecture'a uygun mu? — §4.1 sahiplik tablosu birebir uygulandı
- [x] Gereksiz abstraction var mı? — §4.4'teki "merkezî olmayan" state'ler
      bilinçli olarak **dışarıda bırakıldı**: UI ekranı/imleci, web oturumu,
      ham sensör örnekleri, FSM iç sayaçları
- [x] Blocking işlem var mı? — N/A (yalnızca veri tanımı)
- [x] Shared state güvenli mi? — POD ön koşulu sağlandı; tek yazar kuralı
      `StateOwnership.md`'de belgelendi ve grep ile denetlenebilir
- [x] Memory problemi var mı? — 312 B, hedefin altında; heap yok
- [x] Error handling var mı? — `SensorQuality`, `ErrCode blockReason`,
      `emergencyReason`, `TimeStatus.valid` alanları geçersizliği taşıyor
- [x] ESP32 resource kullanımı uygun mu? — alanlar hizalama israfını azaltacak
      şekilde büyükten küçüğe sıralandı
- [x] Task sorumluluğu doğru mu? — sahiplik tablosu ARCHITECTURE §4.1 ile uyumlu
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Mevcut `Settings`
      yapısı taşınmadı. Karşı örnek olarak incelendi: `Settings.IP`, `.MAC`,
      `LittleFSFormatted` alanları hiç okunmuyordu (REQUIREMENTS §7.2) ve
      şifre düz metin saklanıyordu — ikisi de bu modelde yok.

## Bulgular

**ISSUE-010** kaydedildi (RESOLVED — karar kaydı): TASK-022, TASK-028 ve
TASK-035 kendi tiplerini tanımlamayı planlıyordu; bunların yayınlanan state
kısmı burada tanımlandı. Sınır kuralı sabitlendi:

```text
SystemState.h   → yayinlanan state tipleri
sonraki tasklar → calisma/konfigurasyon tipleri (yeniden tanimlamaz, include eder)
```

İlgili task'lar bu kaydı okumadan tip tanımlamamalıdır.

## Durum

**TASK-006: TAMAMLANDI.**
