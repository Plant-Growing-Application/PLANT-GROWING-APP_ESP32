# TASK-074 — Sahadan Gelen Sensör Algoritmaları + Acil Durum Temizleme

Kullanıcı bildirimi: *"Su akışı ve sıcaklık eski projemde çalışıyordu, şu an
çalışmıyor"* ve *"acil durum bildirimini kapatamıyorum, temizle butonu işe
yaramıyor."*

Üç kusur da doğrulandı ve kapatıldı.

---

## 1. Su sıcaklığı (NTC) — yanlış değer üretiyordu

### Kök neden

`WaterTempSensor::sample()` bölücü direncini **kalibre edilmiş milivolttan**
hesaplıyordu:

```cpp
R_ntc = R_series × (VCC_MV − mv) / mv;      // VCC_MV = 3300
```

`mv`, `esp_adc_cal_raw_to_voltage()` ile 12 dB zayıflatmada üretiliyor ve
**ESP32'nin ADC'si o modda ~3,1 V'ta doyuma girer** — 3300 mV'a hiç ulaşmaz.
Yani ADC'nin tam ölçeği besleme gerilimine eşit değil ve `(VCC_MV − mv) / mv`
oranı sistematik olarak kayıyor.

Sahadaki çalışan formül bu tuzağa hiç düşmüyordu çünkü **oransaldır**:

```cpp
1.0 / (log((4095.0 / sensorValue) - 1.0) / 3950.0 + (1.0 / 298.15)) - 273.15
```

`4095/raw − 1` doğrudan `R_ntc / R_series`'tir ve VCC'den bağımsızdır; mutlak
gerilim kalibrasyonuna hiç ihtiyaç duymaz.

### Ölçülen sapma

| Ham ADC | Saha formülü | Yeni kod | Fark | Eski (bozuk) mV yolu |
|---|---|---|---|---|
| 1000 | 1,572 °C | 1,572 °C | **0** | 0,02 °C |
| 2048 | 25,011 °C | 25,011 °C | **0** | 22,30 °C |
| 3000 | 49,549 °C | 49,549 °C | **0** | 43,95 °C |
| 3500 | 71,034 °C | 71,034 °C | **0** | 60,36 °C |

Oda sıcaklığında **2,7 °C**, üst uçta **10 °C** sapma. Isıtıcı eşiği bu değere
bağlı olduğu için doğrudan yanlış kararlar üretirdi.

### Karar — saha formülü + domain korumaları

Hesap ham ADC oranına döndürüldü; sonuç saha formülüyle **birebir aynı**
(fark tam olarak 0,0). Korunan iki şey:

1. **Domain korumaları.** `raw == 0` → `4095/0 = ∞`, `raw == 4095` →
   `log(0) = −∞`. Saha formülünün üç matematiksel hatasından ikisi bunlardı ve
   sonuç sessizce kullanılıyordu (REQUIREMENTS §3.1). Değer üretmek yerine
   ARIZA bildiriliyor.
2. **`R_NOMINAL` ayrı sabit.** Saha formülü `R_series == R_nominal` varsayar
   (ikisi de 10k). İkisi eşitken sonuç aynı; farklı bir NTC takıldığında formül
   doğru kalıyor.

`VCC_MV` sabiti hesaptan çıktı, belge amaçlı duruyor ve **neden kullanılmadığı**
başlıkta yazılı — aynı hatanın geri gelmemesi için.

> pH ve EC `millivolts` kullanmaya devam ediyor ve bu DOĞRUDUR: onlar gerçek
> bir gerilim çıkışı üretir, bölücü oranı değil. Hata VCC varsayımındaydı,
> mV dönüşümünde değil.

---

## 2. Su akışı — hiç darbe saymıyordu

### Kök neden — eksik pull-up

`PulseCounter::begin()` PCNT birimini yapılandırıyor ama **GPIO'nun dahili
pull-up'ını etkinleştirmiyordu.**

YF-S401'in Hall çıkışı **açık kolektördür**: hattı yalnızca GND'ye çeker,
yukarı sürmez. Pull-up olmadan hat boşta kalır — PCNT ya hiç darbe görür ya
gürültü sayar.

Sahada çalışan kodda bu satır vardı:

```cpp
pinMode(PIN_WATER_FLOW, INPUT_PULLUP);
```

PCNT'ye taşınırken düştü. `pcnt_unit_config()` GPIO'yu PCNT girişine bağlar
ama pull-up'ı kurmak sürücünün işi değildir.

### Karar 1 — Pull-up geri geldi, sıra önemli

`pinMode(pin, INPUT_PULLUP)` **PCNT yapılandırmasından önce** çağrılıyor ki hat,
sayıcı devreye girdiği anda tanımlı bir seviyede olsun.

`BoardPins.h` zaten `isSafePullupInput(FLOW_PULSE)` ile GPIO 34–39'un dahili
pull-up'ı olmadığını derleme zamanında zorluyordu (ISSUE-002) — pin GPIO 4.

### Karar 2 — Düşen kenar

Saha kodu `FALLING` sayıyordu; PCNT yükselen kenardaydı. Temiz bir kare dalgada
iki kenar da aynı sayıyı verir, ama açık kolektör çıkışında anlamlı kenar
hattın GND'ye çekildiği andır. Farkı ortadan kaldırmak, sahadan gelen
katsayının birebir geçerli kalmasını sağlıyor.

### Karar 3 — ISSUE-014 kapandı: 450 değil **270** darbe/L

Saha kodunun çalışan hesabı:

```
L/dk = darbe × 100 / 450        (1 saniyelik pencerede)
```

Buradaki zaman normalize edilmiş hesap:

```
L/dk = darbe / PULSES_PER_LITER × (60000 / pencere_ms)
```

1 saniyelik pencerede eşitlersek:

```
100/450 = 60 / PULSES_PER_LITER   ⇒   PULSES_PER_LITER = 270
```

Yani sahada doğru sonucu veren katsayı **270**'tir. 450 rakamı eski kodun
yorumundan alınmış, **hiçbir zaman ölçülmemiş** bir veri sayfası varsayımıydı —
ISSUE-014 tam olarak bunu işaretliyordu. Eski değer debiyi **1,67 kat düşük**
gösteriyordu, yani kuru çalışma eşiği de yanlış taraftaydı.

### Karar 4 — Zaman normalizasyonu KORUNDU

Saha formülü 1 saniyelik sabit pencere varsayar. Örnekleme periyodu değişirse
(veya bir tur gecikirse) sessizce yanlış debi üretir. Buradaki hesap **gerçek
geçen süreye** böler: aynı sayıyı üretir ama varsayıma bağlı değildir.

Farklı bir sensör veya boru çapı için saha trim'i `SensorConfig.scale` ile
yapılır — artık arayüzden erişilebilir (Gelişmiş → Sensör Kalibrasyonu).

---

## 3. "Acil Durumu Temizle" düğmesi işe yaramıyordu

### Kök neden — sessiz ret

Cihaz tarafındaki mantık **doğruydu**: `SafetyMonitor::acknowledge()` canlı su
seviyesi koşulunu kontrol eder ve düzelmemişse temizlemeyi reddeder. Koşullar
düzelmeden temizlemek, aynı arızayla pompayı yeniden çalıştırmak olurdu.

Sorun **geri bildirimdeydi**:

1. Arayüz `emergencyClear` komutunu kuyruğa atıyor
2. Cihaz `ACCEPTED` (= *kuyruğa alındı*) ack'i döndürüyor
3. `app_core` turunda `acknowledge()` koşulları görüp **reddediyor**
4. Ret yalnızca **olay günlüğüne** yazılıyor

Kullanıcı açısından: düğmeye bas, "kabul edildi" al, hiçbir şey olmasın,
nedenini hiçbir yerde göremeyesin.

İkinci katman: su seviyesi şamandıraları bağlı değilse kalite `OK` olmaz →
`ILK_LEVEL_SENSOR_FAULT` → temizleme **kalıcı olarak** reddedilir. Kullanıcı
sonsuza kadar sıkışır ve çıkış yolunu bilmez.

### Karar 1 — Kilit maskesi ÇÖZÜLÜYOR

`SafetyStatus.interlockMask` her telemetri paketinde yayınlanıyordu ama arayüz
onu **hiç okumuyordu**. Kullanıcının "neden çalışmıyor" sorusunun cevabı
telemetride hazır duruyor ve gösterilmiyordu.

`ILK` bit sözlüğü eklendi (`SafetyState.h` ile aynı değerler) ve Kontrol
ekranında aktif kilitler **basmadan önce** listeleniyor.

### Karar 2 — Üç aşamalı temizleme

```
1. ÖNCE KONTROL — engel varsa komut HİÇ gönderilmez, ne yapılacağı yazılır
2. gönder
3. SONRA DOĞRULA — 2 sn içinde temizlenmediyse bunu söyle
```

Üçüncü aşama kritik: komutun kuyruğa alınması temizlendiği anlamına gelmez.
Arayüz durum üretmez, **doğrular** (ARCHITECTURE P5).

### Karar 3 — Engelleyen küme cihazla AYNI

Arayüz yalnızca `LEVEL_LOW | LEVEL_FAULT` bitlerini engel sayar — çünkü
`acknowledge()` de yalnızca `evaluateLevel()` sonucunu kontrol eder. Kuru
çalışma ve süre aşımı mandalları **temizlenecek olanlardır**, engel değil.

Farklı bir küme kullanmak, kullanıcıya cihazın uygulamadığı bir kural
anlatmak olurdu.

### Karar 4 — Çıkış yolu SÖYLENİYOR

Seviye sensörü takılı değilse mesaj şunu yazıyor:

> Su seviyesi sensörü takılı değilse Gelişmiş → Güvenlik Eşikleri bölümünden
> "su seviyesi sensörü okunamıyorsa pompa kilitli kalsın" seçeneğini
> kapatabilirsiniz.

Var olan bir ayarı işaret ediyor; yeni bir kapı açmıyor.

---

## 4. PC test rehberi

Kullanıcı isteği: *"telefona bağlı kalıyorum sürekli, PC'de test edebilmem
için her şeyi tek tek anlatan bir dosya."*

`docs/PC_TEST_REHBERI.md` yazıldı: tek komutla başlatma, arayüzü düzenleyip
F5 ile görme, dokuz adımlık uçtan uca senaryo (giriş → sensör açma → ürün
profili → otomatik mod → komut yolu → acil durdurma), simülatörün ne yapıp ne
yapmadığı, firmware derleme, sık karşılaşılan durumlar.

---

## Doğrulama

- [x] NTC: saha formülüyle **birebir aynı** (7 ADC değerinde fark = 0,0);
      eski mV yolunun sapması ölçüldü ve belgelendi
- [x] Akış: katsayı türetimi aritmetik olarak gösterildi (100/450 ≡ 270 ppl)
- [x] Acil durum: normal koşulda temizleniyor ("✔ Acil durum kilidi
      temizlendi"); seviye düşükken **reddediliyor ve nedeni yazılıyor**
- [x] Kilit listesi Kontrol ekranında görünüyor
- [x] `pio run` temiz, 0 uyarı · Flash %66,9 · RAM %23,9

## Dokunulan dosyalar

```
src/services/sensors/AnalogSensors.{h,cpp}   NTC oransal hesap
src/services/sensors/FlowSensor.h            PULSES_PER_LITER 450 -> 270
src/hal/PulseCounter.cpp                     INPUT_PULLUP + dusen kenar
frontend/js/10-core.js                       ILK bit sozlugu, interlockList
frontend/js/30-views.js                      clearEmergency, renderInterlocks
frontend/js/50-init.js                       dugme baglantilari
frontend/index.html                          kilit listesi + estopMsg
tools/mock_device.py                         temizleme reddi firmware ile ayni
docs/PC_TEST_REHBERI.md                      YENI
```

## Açık kalan

- Üçü de **donanımda doğrulanmadı**. NTC ve akış matematiği kanıtlandı ama
  gerçek sensörle okuma yapılmadı.
- 270 darbe/L değeri sahadan geliyor; kesin doğrulama için bilinen bir hacim
  akıtılıp sayım karşılaştırılmalı (`docs/HARDWARE.md §5`).
