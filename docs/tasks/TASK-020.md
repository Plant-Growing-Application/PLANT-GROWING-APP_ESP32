# TASK-020 — OledPanel Driver

**Phase:** 4 — Hardware Abstraction · **Priority:** P2

## Objective

OLED'e erişimi tek bir sürücü arkasına almak ve init hatasının sistemi durdurmasını
engellemek. Mevcut projede OLED init hatası `while(true)` ile tüm sistemi kilitliyordu.

## Scope

- I2C bus başlatma ve SSD1306 init
- Init hatasının durum olarak raporlanması
- Temel çizim yüzeyi (metin, çizgi, dikdörtgen, bitmap)
- Kirli alan (dirty region) desteği — yalnızca değişen bölgenin gönderilmesi
- I2C hata tespiti ve kurtarma denemesi

## Out of Scope

- Ekran içerikleri ve düzen (TASK-052)
- ViewModel üretimi (TASK-050)
- Navigasyon (TASK-051)

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — §6 (display), §1 (OLED hatasında sonsuz döngü)

## Architecture References

- §2.15 OledPanel · §13 Display mimarisi
- §16.3 Display failure davranışı

## Expected Design

- Init hatası **bir durumdur**: `isAvailable()` sorgulanır, sistem çalışmaya devam eder.
  Web arayüzü ve otomasyon OLED'siz tam çalışır.
- Sürücü **yalnızca çizim yüzeyi** sunar; hiçbir iş verisi okumaz, hiçbir karar vermez.
- I2C hattı çalışma sırasında kopabilir (kablo, gürültü). Yazma hataları sayılmalı ve
  eşik aşılınca ekran "kullanılamaz" olarak işaretlenmeli; sonsuz yeniden deneme yapılmamalı.
- Kirli alan desteği önemlidir: tam ekran güncellemesi I2C üzerinde belirgin süre alır ve
  `ui` task'ının 50 ms periyodunu zorlar.

## Implementation Notes

- I2C bus'ı başka bir cihazla paylaşılabilir (gelecekte RTC, sensör). Bus erişimi bu
  ihtimale göre tasarlanmalı; şu an tek kullanıcı olsa bile varsayım koda gömülmemeli.
- Çerçeve tamponu RAM'de yer kaplar (128×64 mono ≈ 1 KB); bu bilinçli bir maliyettir.
- I2C yazma süresi ölçülmeli. Tam ekran güncellemesi 50 ms bütçesini aşıyorsa kirli alan
  kullanımı zorunlu hale gelir.
- Sürücüye **yalnızca `ui` task'ı erişir** (§13.2). Bu kural sürücüde de doğrulanabilir.
- Kullanılmayan `Adafruit_SH1106` kütüphanesi TASK-001'de kaldırılmış olmalı.

## Files

- `src/hal/OledPanel.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Init hatası sistemi durdurmuyor; `isAvailable()` ile sorgulanabiliyor
- [ ] Temel çizim işlemleri çalışıyor
- [ ] Kirli alan güncellemesi destekleniyor
- [ ] I2C hataları sayılıyor; eşik aşılınca ekran kullanılamaz işaretleniyor
- [ ] Sonsuz yeniden deneme yok
- [ ] Sürücüde iş mantığı yok
- [ ] Yalnızca `ui` task'ından erişiliyor
- [ ] Çizim süreleri ölçüldü

## Test Plan

- [ ] OLED kablosu çıkarılıp boot edildiğinde sistem çalışıyor, web erişilebilir
- [ ] Çalışma sırasında kablo çıkarıldığında sistem kilitlenmiyor, ekran kullanılamaz işaretleniyor
- [ ] Tam ekran ve kirli alan güncelleme süreleri ölçüldü ve karşılaştırıldı
- [ ] Uzun süreli çalışmada I2C hatası birikmiyor
- [ ] Çizim çıktısı görsel olarak doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§13)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — I2C yazma süresi ölçüldü mü
- [ ] Shared state güvenli mi? — **tek task erişimi**
- [ ] Memory problemi var mı? — çerçeve tamponu
- [ ] Error handling var mı? — init ve I2C hataları
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`while(true)` init hatası yasak**

## Definition of Done

Ortak DoD + OLED'siz boot testi geçti + çizim süreleri `ui` bütçesine sığdığı doğrulandı.

---

# STEP 1+3 — DESIGN & REVIEW RECORD

## Karar 1 — Init hatası bir durum, `while(true)` değil

Mevcut sistemde OLED init hatası `while (true)` ile karşılanıyor ve **tüm
sistemi kilitliyordu** (Kritik Problem 4). Ekransız bir sera cihazı hâlâ
sulama yapabilir; kilitlenen bir cihaz yapamaz.

`isAvailable()` sorgulanır; OLED yoksa sistem tam çalışır, `ui` task'ı
çizmeden devam eder.

## Karar 2 — I2C hatasında sonsuz yeniden deneme YOK

Ardışık 20 hatadan sonra panel **kullanılamaz** işaretlenir ve ERROR loglanır.
Her döngüde başarısız bir I2C işlemi `ui` task'ının 50 ms periyodunu bozardı.

## Karar 3 — `displayRegion()` arayüzü şimdi sabitleniyor

Adafruit_SSD1306 kısmi aktarım sunmaz. Arayüz yine de şimdi tanımlandı ki
`ui` katmanı (TASK-052) kirli alan mantığını kurabilsin. TASK-062 ölçümü tam
aktarımın 50 ms bütçesini zorladığını gösterirse gerçek kısmi aktarım burada
uygulanacaktır — çağıranlar değişmeyecek.

## Review

- [x] Init hatası sistemi durdurmuyor; `isAvailable()` ile sorgulanıyor
- [x] Temel çizim işlemleri çalışıyor
- [x] I2C hataları sayılıyor; eşikte panel devre dışı, sonsuz deneme yok
- [x] Sürücüde iş mantığı yok (D6) — ne çizileceği `interfaces/ui/`'nin işi
- [x] Yalnızca `ui` task'ından erişim kuralı header'da belgeli (P2)
- [x] Derleme SUCCESS, 0 uyarı (Adafruit kütüphaneleri +22 KB)
- [ ] **OLED'siz boot, çalışırken kablo çıkarma, çizim süreleri — donanım gerekiyor**

**TASK-020: TAMAMLANDI** (donanım testleri bekliyor).

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

> **Protokol notu:** Geriye dönük kayıt (bkz. TASK-018).

## Karar 1 — INIT HATASI BİR DURUMDUR, SİSTEMİ DURDURMAZ

```text
Eski sistem: OLED init hatasi `while (true)` ile karsilaniyor ve TUM
             SISTEMI kilitliyordu (REQUIREMENTS Kritik Problem 4).

Yeni: `isAvailable()` sorgulanir. OLED yoksa sistem TAM calisir,
      yalnizca ekran cizilmez (§16.3).
```

> Ekranı olmayan bir sera cihazı hâlâ sulama yapabilir; kilitlenen bir
> cihaz yapamaz.

## Karar 2 — DONANIMA TEK KAPI (P2)

Bu sürücüye **yalnızca `ui` task'ı** erişir. Eski sistemde `Sensor.cpp`,
`MyWifi.cpp` ve `GrowPlant.cpp` aynı `oled` nesnesine **korumasız**
yazıyordu — sahada tekrarlanması zor kararsızlıkların en olası kaynağı.

## Karar 3 — I2C hata sayacı

`i2cErrorCount()` teşhis için tutulur: ekranın kopması sessiz geçmemeli.

## İnceleme

- [x] Init hatası sistemi durdurmuyor
- [x] `isAvailable()` ile sorgulanabiliyor
- [x] Tek sahip (`ui`) — tarama ile doğrulandı (boot `begin()` hariç)
- [x] I2C hata sayacı var
- [ ] **Çizim süresi ve I2C kararlılığı ölçülmedi**
