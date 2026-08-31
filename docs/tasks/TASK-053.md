# TASK-053 — UiService & ui Task Integration

**Phase:** 11 — Display · **Priority:** P2

## Objective

Display katmanını çalışır hale getirmek: girdi → navigasyon → ViewModel → çizim döngüsünü
`ui` task'ında kurmak ve kullanıcı eylemlerini **komuta** dönüştürmek.

## Scope

- `ui` task döngüsü (50 ms, Core 1)
- Girdi olay kuyruğu tüketimi
- Snapshot alma ve ViewModel üretimi
- Çizim tetikleme (değişim varsa)
- Kullanıcı eylemlerinin `CommandQueue`'ya gönderilmesi
- Wi-Fi durum LED'i (ayrı task açılmadan, §6.4)
- Heartbeat ve watchdog

## Out of Scope

- Ekran içerikleri (TASK-052)
- Komut yürütme (TASK-033)

## Dependencies

- TASK-052, TASK-011, TASK-008

## Requirements

- `REQUIREMENTS.md` — §6, §10 (Task_Display, Task_WifiLed)

## Architecture References

- §2.10 UiService · §13.1 ViewModel akışı · §13.2 Kurallar
- §6.1 ui task satırı · §6.4 LED'in ayrı task olmaması

## Expected Design

### Döngü

```text
  1. girdi olaylarını işle → navigasyon güncelle
  2. snapshot al
  3. ViewModel üret
  4. önceki ViewModel'den farklıysa çiz
  5. Wi-Fi LED durumunu güncelle
  6. heartbeat · watchdog besle
```

### UI'nin tek çıkışı

> UI'nin dış dünyaya **tek çıkışı** `CommandQueue.post()`'tur (§13.2).

Kullanıcı OLED'den acil durdurma yaparsa bu bir komuttur; `ui` task'ı röleye dokunmaz.
Mod değiştirirse bu bir komuttur; `ui` task'ı config'e yazmaz.

### Wi-Fi LED — ayrı task değil

Mevcut projede `Task_WifiLed` 2 KB stack ile ayrı bir task'tı. Bir LED yakıp söndürmek
için task açmak israftır (§6.4). Bu işlev `ui` döngüsü içinde bir sayaçla yapılır.

### Karar gerektiren nokta — Çizim sıklığı

```text
Problem:      Her döngüde (50 ms) çizim gerekli mi?
Constraints:  I2C yazma süresi belirgin; sürekli çizim CPU ve bus harcar;
              kullanıcı gecikme hissetmemeli (encoder çevirisi anında yansımalı)
Approaches:   (a) her döngüde çiz
              (b) yalnızca ViewModel değişince çiz
              (c) girdi olayında anında, aksi halde düşük hızda
Recommended:  (b) — girdi olayı zaten ViewModel'i değiştirir, yani (c) ile aynı
              sonucu daha basit verir
```

## Implementation Notes

- OLED yoksa (`isAvailable() == false`) task **çizmeden** çalışmaya devam etmeli veya
  askıya alınmalı; sistem etkilenmemeli.
- Snapshot her döngüde alınmalı ancak boyutu stack'i zorlamamalı; `ui` task stack'i
  buna göre boyutlandırılmalı.
- Encoder olayları birikmişse hepsi işlenmeli ama ekran bir kez çizilmeli.
- Kullanıcı eylemi komuta dönüşürken `reqId` üretilmeli; sonucu ekranda gösterilebilmeli
  (örneğin "reddedildi: güvenlik kilidi").
- Komut sonucu ekrana yansımalı — mevcut projede kullanıcı bir şey yaptığında geri
  bildirim yoktu.
- Watchdog beslemesi döngü sonunda.
- Döngü süresi ölçülmeli; en kötü durum (tam ekran çizim) 50 ms'i aşmamalı.

## Files

- `src/interfaces/ui/UiService.h` / `.cpp` (yeni)
- `src/tasks/UiTask.cpp` (yeni)

## Acceptance Criteria

- [ ] `ui` task'ı 50 ms periyotla, Core 1'de çalışıyor
- [ ] Girdi → navigasyon → ViewModel → çizim akışı çalışıyor
- [ ] Yalnızca ViewModel değiştiğinde çiziliyor
- [ ] Kullanıcı eylemleri **yalnızca** `CommandQueue`'ya gidiyor
- [ ] UI hiçbir donanıma (OLED hariç) dokunmuyor
- [ ] Komut sonucu kullanıcıya gösteriliyor
- [ ] Wi-Fi LED ayrı task olmadan yönetiliyor
- [ ] OLED yokken sistem etkilenmiyor
- [ ] Heartbeat ve watchdog doğru sırada
- [ ] Döngü süresi ölçüldü

## Test Plan

- [ ] Encoder çevirisi anında ekrana yansıyor (gecikme ölçüldü)
- [ ] OLED'den verilen komut cihazda uygulanıyor
- [ ] Güvenlik vetolu komutta ekranda ret nedeni gösteriliyor
- [ ] OLED kablosu çıkarıldığında sistem çalışmaya devam ediyor
- [ ] Wi-Fi LED durumu doğru yanıp sönüyor
- [ ] Döngü süresi en kötü durumda ölçüldü
- [ ] Stack watermark ölçüldü
- [ ] Uzun süreli çalışmada heartbeat kesintisiz
- [ ] Kod taramasıyla UI'nin röle/ağ/flash'a dokunmadığı doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.10, §13.2, §6.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — çizim süresi
- [ ] Shared state güvenli mi? — snapshot kullanımı
- [ ] Memory problemi var mı? — stack, snapshot boyutu
- [ ] Error handling var mı? — OLED yokluğu
- [ ] ESP32 resource kullanımı uygun mu? — LED için ayrı task açılmamış mı
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`Task_WifiLed` ayrı task olarak taşınmamalı**

## Definition of Done

Ortak DoD + UI'nin tek çıkışının komut kuyruğu olduğu kod taramasıyla doğrulandı +
döngü süresi ve stack ölçüldü.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Çizim sıklığı: (b) yalnızca ViewModel değişince

```text
(a) her donguda (50 ms) ciz → I2C busunu ve CPU'yu bosuna harcar; 128×64
    bir cerceve gondermek olculebilir sure alir.
(c) girdi olayinda aninda, aksi halde dusuk hizda → GEREKSIZ:
    girdi olayi ZATEN ViewModel'i degistirir (imlec, ekran, onay durumu),
    yani (b) ayni sonucu daha basit verir.

SECILDI (b): `memcmp(UiModel)` farkliysa ciz.
```

## Karar 2 — UI'nin TEK ÇIKIŞI `CommandQueue.post()`

```text
Kullanici OLED'den acil durdurma yaparsa → KOMUT. `ui` roleye dokunmaz.
Kullanici mod degistirirse              → KOMUT. `ui` config'e yazmaz.

Acil durdurma GARANTILI YOL'u kullanir (`postEmergencyStop`) — kuyruk
doluyken bile ulasir (TASK-008).
```

Tarama ile denetlenir: `interfaces/ui/` içinde `hal::relay`, `hal::wifi`,
`nvsstore`, `vTaskSuspend` **bulunmamalıdır**.

## Karar 3 — Wi-Fi LED ayrı task DEĞİL

```text
Eski projede `Task_WifiLed` 2 KB stack ile AYRI BIR TASK'ti. Bir LED yakip
sondurmek icin task acmak israftir (ARCHITECTURE §6.4).

Yeni: `ui` dongusu icinde bir sayac.
   bagli      → surekli yanik
   baglaniyor → 500 ms yanip soner
   AP modu    → 1500 ms'de bir kisa cakma
   bagli degil→ sonuk
```

## Karar 4 — OLED yoksa task ÇALIŞMAYA DEVAM EDER

`isAvailable() == false` ise çizim atlanır ama döngü döner: girdi olayları
yine işlenir (encoder/butonlar çalışır), komutlar yine üretilir, heartbeat
yine beslenir. Ekranın ölmesi cihazın kontrol edilemez hâle gelmesi
demek olmamalı (P4 — fail-degraded).

## Karar 5 — Acil duruma geçişte ekran OTOMATİK değişir

`safety.emergencyLatched` 0→1 kenarında `nav::onEmergency()` çağrılır.
Operatörün uyarıyı görmek için ekran değiştirmesi gerekmez.

---

# STEP 3 — REVIEW RECORD

- [x] `ui` döngüsü: girdi → snapshot → ViewModel → (değiştiyse) çiz → LED
- [x] Çizim **yalnızca `UiModel` değişince**
- [x] **UI'nin tek çıkışı `CommandQueue.post()`** — tarama: `interfaces/ui/`
      içinde röle/Wi-Fi/NVS/task-askıya-alma **0 eşleşme**
- [x] Acil durdurma garantili yolu kullanıyor (kuyruk doluyken bile ulaşır)
- [x] Aktüatör aç/kapa **gerçek duruma göre** tersine çeviriyor; istenen
      durumu UI kendisi üretmiyor
- [x] Wi-Fi LED'i **ayrı task değil**, döngü içinde sayaç
- [x] OLED yoksa döngü devam ediyor: girdi işleniyor, komut üretiliyor,
      heartbeat besleniyor (P4)
- [x] Acil duruma geçiş kenarında ekran otomatik değişiyor
- [x] Heartbeat ve watchdog `TaskRunner` tarafından doğru sırada
- [ ] **Döngü süresi, stack watermark, çizim oranı — donanım gerekiyor**

## Eski sistemin üç deseni kaldırıldı

| Eski | Yeni |
|---|---|
| `Task_WifiLed` (2 KB stack, tek iş: LED) | `ui` döngüsünde bir sayaç |
| Ekran kodundan `StateWifi()` + EEPROM + `pauseWiFiMonitor()` | Ekran yalnızca çizer; eylem `CommandQueue`'ya gider |
| OLED'e iki task birden yazıyor (`Task_Display` + `SensorValues()`) | OLED'in tek sahibi `ui` |

**TASK-053: TAMAMLANDI** (donanım ölçümleri bekliyor).
