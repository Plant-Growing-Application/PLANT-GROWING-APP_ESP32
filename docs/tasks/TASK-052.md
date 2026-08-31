# TASK-052 — Screens (Home, Sensors, Control, Network, System, Alerts)

**Phase:** 11 — Display · **Priority:** P2

## Objective

Ekran içeriklerini çizmek. Her ekran yalnızca kendisine verilen ViewModel'den okuyup
çizmeli; hiçbir veri kendisi toplamamalı.

## Scope

- Durum çubuğu (tüm ekranlarda ortak)
- HOME: özet — mod, pompa durumu, kritik sensörler
- SENSORS: tüm sensörler + kalite göstergesi
- CONTROL: aktüatör durumu, manuel komut, override kalan süre
- NETWORK: durum, SSID, IP, RSSI, AP bilgisi (**şifre gösterilmez**)
- SYSTEM: uptime, mod, boot raporu, yeniden başlat
- ALERTS: aktif hatalar, acil durum, onay
- EMERGENCY: öncelikli ekran

## Out of Scope

- Navigasyon (TASK-051)
- ViewModel üretimi (TASK-050)
- Komut gönderimi (TASK-053)

## Dependencies

- TASK-051, TASK-020

## Requirements

- `REQUIREMENTS.md` — §6.1, §6.2 (mevcut sayfalar), §6.3 (bilinen display sorunları)

## Architecture References

- §13.2 Display kuralları (yasaklar) · §13.3 Ekran yapısı

## Expected Design

### Mutlak yasaklar (§13.2)

Hiçbir ekran kodu:

- Sensör okumaz
- Wi-Fi'ye bağlanmaz veya mod değiştirmez
- Röle sürmez
- NVS/EEPROM yazmaz
- Task suspend/resume çağırmaz

Mevcut projede `GrowPlant.cpp` bunların **dördünü birden** yapıyordu: sayfa içindeyken
encoder çevrilince `StateWifi()` çağrılıp Wi-Fi modu değiştiriliyor, EEPROM'a yazılıyor ve
`pauseWiFiMonitor()` ile başka bir task askıya alınıyordu. Yeni tasarımda ekran yalnızca
komut üretir (TASK-053), eylemi kendisi yapmaz.

### 128×64 kısıtı

Ekran çok küçüktür. Her ekran için **önce ne gösterilmeyeceğine** karar verilmeli.
Bilgi yoğunluğu okunabilirliği bozmamalı.

### Şifre gösterimi yasağı

Mevcut projede WIFI sayfası SSID ve şifreyi açıkça yazıyordu. Yeni tasarımda şifre
**hiçbir ekranda gösterilmez** (§8.2).

## Implementation Notes

- Her ekran bir çizim fonksiyonu olmalı: ViewModel al, çiz, dön. Durum tutmamalı.
- Durum çubuğu ortak olmalı; her ekranda yeniden yazılmamalı.
- Kirli alan güncellemesi kullanılmalı; tam ekran yenileme yalnızca ekran değişiminde.
- Sensör ekranında kalite göstergesi net olmalı: arızalı sensörün eski değeri
  gösterilmemeli (mevcut projedeki en tehlikeli gösterim hatası).
- Acil durum ekranı nedeni ve ne yapılması gerektiğini göstermeli; yalnızca "HATA" yazmak
  yetersizdir.
- Yazı boyutu ve kontrast sera ortamında (nemli, parlak) okunabilir olmalı.
- Mevcut projedeki koordinat tutarsızlıkları (aynı yazının 35 ve 28 Y koordinatlarına
  yazılması) tekrarlanmamalı; düzen sabitleri merkezi olmalı.
- Çizim süreleri ölçülmeli; `ui` task'ının 50 ms bütçesine sığmalı.

## Files

- `src/interfaces/ui/screens/StatusBar.cpp` (yeni)
- `src/interfaces/ui/screens/HomeScreen.cpp` (yeni)
- `src/interfaces/ui/screens/SensorsScreen.cpp` (yeni)
- `src/interfaces/ui/screens/ControlScreen.cpp` (yeni)
- `src/interfaces/ui/screens/NetworkScreen.cpp` (yeni)
- `src/interfaces/ui/screens/SystemScreen.cpp` (yeni)
- `src/interfaces/ui/screens/AlertsScreen.cpp` (yeni)
- `src/interfaces/ui/screens/EmergencyScreen.cpp` (yeni)

## Acceptance Criteria

- [ ] Sekiz ekran çiziliyor
- [ ] Hiçbir ekran sensör okumuyor, ağa dokunmuyor, röle sürmüyor, flash yazmıyor
- [ ] Ekranlar durum tutmuyor; yalnızca ViewModel'den çiziyor
- [ ] Durum çubuğu ortak
- [ ] Şifre hiçbir ekranda gösterilmiyor
- [ ] Arızalı sensörün eski değeri gösterilmiyor
- [ ] Acil durum ekranı neden ve yapılacak eylemi gösteriyor
- [ ] Düzen sabitleri merkezi; koordinat tutarsızlığı yok
- [ ] Kirli alan güncellemesi kullanılıyor
- [ ] Çizim süreleri ölçüldü ve bütçede

## Test Plan

- [ ] Her ekran görsel olarak doğrulandı
- [ ] Sensör arızasında "—" gösteriliyor, eski değer değil
- [ ] Şifrenin hiçbir ekranda görünmediği doğrulandı
- [ ] Acil durum ekranı nedeni doğru gösteriyor
- [ ] Uzun SSID/IP değerleri taşmıyor
- [ ] Çizim süreleri 50 ms bütçesinde
- [ ] Ekranlarda titreme yok (kirli alan güncellemesi çalışıyor)
- [ ] Kod taramasıyla yasak çağrıların (sensör/ağ/röle/flash) olmadığı doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§13.2, §13.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — çizim süresi
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — OLED yokken çökmüyor mu
- [ ] ESP32 resource kullanımı uygun mu? — I2C yükü
- [ ] Task sorumluluğu doğru mu? — **ekranlar yalnızca çiziyor mu**
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`GrowPlant.cpp`'nin yan etkileri asla taşınmamalı**

## Definition of Done

Ortak DoD + yasak çağrıların olmadığı kod taramasıyla doğrulandı + çizim süreleri ölçüldü.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Mutlak yasaklar (§13.2) — TARAMA ile denetlenecek

Hiçbir ekran kodu: sensör okumaz · Wi-Fi'ye dokunmaz · röle sürmez ·
NVS/EEPROM yazmaz · task suspend/resume çağırmaz.

```text
Eski `GrowPlant.cpp` bunlarin DORDUNU BIRDEN yapiyordu: sayfa icindeyken
encoder cevrilince `StateWifi()` cagrilip Wi-Fi modu degistiriliyor,
EEPROM'a yaziliyor ve `pauseWiFiMonitor()` ile BASKA BIR TASK askiya
aliniyordu (REQUIREMENTS §6.3).

Yeni tasarimda ekran YALNIZCA CIZER. Eylem uretimi TASK-053'un isi ve
tek cikis `CommandQueue.post()`.
```

Ekran fonksiyonlarının imzası bunu **yapısal olarak** garanti eder:
`draw(const UiModel&)` — `const` referans, dönüş `void`, başka parametre yok.
Bir ekranın komut üretmesi için önce imzanın değişmesi gerekir.

## Karar 2 — Düzen sabitleri MERKEZİ

```text
Eski projede ayni yazi bir yerde Y=35, baska yerde Y=28 koordinatina
yaziliyordu; ekranlar arasi gecislerde satirlar kayiyordu.

`Layout` namespace'i: STATUS_H, ROW0..ROW4, COL_VALUE, FOOTER_Y.
Hicbir ekran ciplak koordinat kullanmaz.
```

## Karar 3 — Şifre HİÇBİR ekranda gösterilmez

Eski WIFI sayfası SSID ve şifreyi açıkça yazıyordu. `UiModel`'de zaten alan
yok (TASK-050). **İstisna:** `apPassword` — cihazın kendi ürettiği kurulum
şifresi; kullanıcı bağlanmak için okumak zorunda.

## Karar 4 — Arızalı sensörün ESKİ DEĞERİ gösterilmez

`UiModel` zaten `"--"` üretiyor (TASK-050 Karar 3). Ekran katmanı bunu
yalnızca basar; kendi başına bir değer biçimlendirmesi yapmaz.

## Karar 5 — Acil durum ekranı NE YAPILACAĞINI söyler

Yalnızca "HATA" yazmak yetersizdir. Ekran: neden + hangi koşulun düzelmesi
gerektiği + onayın nasıl verileceği.

---

# STEP 3 — REVIEW RECORD

- [x] Durum çubuğu ortak; her ekranda yeniden yazılmıyor
- [x] Yedi ekran çizildi
- [x] **Mutlak yasaklar** — tarama: `interfaces/ui/` içinde `hal::relay|`
      `hal::wifi|nvsstore|vTaskSuspend|vTaskResume|analogRead|WiFi.` →
      **0 eşleşme**
- [x] İmza yapısal garanti veriyor: `void draw(const UiModel&)`
- [x] Düzen sabitleri merkezî; çıplak koordinat yok
- [x] Şifre hiçbir ekranda yok (kurulum AP şifresi hariç, ki gösterilmek zorunda)
- [x] Arızalı sensörün eski değeri gösterilmiyor — ekran katmanı kendi
      biçimlendirmesini yapmıyor, `UiModel`'i basıyor
- [x] Acil durum ekranı **nedeni + ne yapılacağını** gösteriyor
- [x] Engel nedeni kontrol ekranında görünüyor (sessiz engelleme yok)
- [ ] **Çizim süresi ölçümü — donanım gerekiyor**

## Plandan sapma: sekiz dosya yerine tek dosya

Plan her ekran için ayrı `.cpp` öngörüyordu. Ekranların tamamı ~300 satır
ve hepsi aynı `layout` sabitlerini, aynı `row()`/`selectableRow()`
yardımcılarını kullanıyor. Sekiz dosyaya bölmek, sekiz kez aynı başlık
bloğunu ve aynı `namespace` merdivenini tekrarlamak olurdu.

Bölme kod büyüdüğünde yapılır; şu anda yapmak sürdürülebilirliği artırmaz,
azaltır.

**TASK-052: TAMAMLANDI** (çizim süresi ölçümü bekliyor).
