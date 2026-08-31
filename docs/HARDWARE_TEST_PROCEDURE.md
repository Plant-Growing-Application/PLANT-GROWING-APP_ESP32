# Donanım Test Prosedürü — TASK-065

**Amaç:** Firmware değişikliğinden sonra tekrar çalıştırılabilecek yazılı bir
prosedür. Sözlü bilgi kaybolur.

**Ön koşul:** ISSUE-003 (röle polaritesi) kapatılmış olmalı. Güvenlik
zincirinin tamamı `RELAY_ACTIVE_LOW` varsayımına dayanıyor; yanlışsa
aşağıdaki testlerin hiçbiri anlamlı değildir.

---

## 0. Hazırlık — ÖNCE YAPILMASI ZORUNLU

### T0.1 Röle polaritesi (ISSUE-003)

1. ESP32'yi **röle modülü BAĞLI DEĞİLKEN** programla
2. `RELAY_WATER_PUMP` (GPIO 16) pinini multimetreyle ölç
3. Boot sonrası pinin seviyesini not et → `RELAY_SAFE_LEVEL` ile karşılaştır
4. Röle modülünü ayrı besleyip giriş hattına 0 V ve 3,3 V uygula; hangisinde
   çektiğini gözle

> **Röle boot sırasında çekiyorsa DURUN.** `docs/HARDWARE.md` §8'deki harici
> pull-up/pull-down zorunlu maddesi uygulanmadan devam edilmemeli.

### T0.2 İlk boot ve boot raporu

```bash
pio run -t upload && pio device monitor
```

Beklenen: 9 aşamanın sonucu seri porta basılır. Not edilecekler:
- Hangi aşamalar başarısız
- Türetilen mod (`RUNNING` / `DEGRADED` / `SAFE`)
- Reset nedeni

### T0.3 Varlık yükleme

```bash
python tools/build_assets.py
pio run -t uploadfs
```

---

## 1. Entegrasyon — dört veri akışı (TASK-060)

| # | Test | Beklenen | Ölçülecek |
|---|---|---|---|
| 1.1 | Web'den pompa AÇ | Röle çeker, web ve OLED güncellenir | tıklama→röle gecikmesi |
| 1.2 | OLED'den pompa AÇ (encoder + onay) | Aynı zincir | basış→röle gecikmesi |
| 1.3 | Sensör değeri değiştir | Web ve OLED'de görünür | değişim→ekran gecikmesi |
| 1.4 | Güvenlik vetosu (hazne boş) | Komut reddedilir, **neden görünür** | — |

### 1.5 Task periyotları

`GET /api/diagnostics` → her task için `maxLoopUs` ve `overruns`.

Hedefler: `app_core` < 30 ms · `io_sense` < 100 ms · `ui` < 20 ms

### 1.6 24 saat kesintisiz

24 saat çalıştır, sonra `GET /api/diagnostics`:
- `overruns` makul mü
- WDT reset olmuş mu (`resetReason`)
- `minFreeHeap` düşüş eğilimi var mı (sızıntı)

---

## 2. M4 GÜVENLİK KAPISI — otomasyondan ÖNCE

> **Bu bölüm geçilmeden `AUTO` moduna GEÇİLMEZ.**

| # | Test | Yöntem | Beklenen |
|---|---|---|---|
| 2.1 | Seviye kilidi | Hazneyi boşalt, pompayı AÇ dene | `SAFETY_LEVEL_INSUFFICIENT`, röle çekmez |
| 2.2 | Seviye sensörü arızası | Şamandıra kablosunu çıkar | `SAFETY_LEVEL_SENSOR_FAULT`, pompa kilitli |
| 2.3 | Çalışan pompa + seviye düşüşü | Pompa çalışırken hazneyi boşalt | **Aynı döngüde** durur, `minRunMs` tanınmaz |
| 2.4 | Kuru çalışma | Emme hortumunu havada bırak, pompayı AÇ | `flowVerifyDelayMs` sonra `SAFETY_DRY_RUN`, **mandal** |
| 2.5 | Akış sensörü arızası | Akış sensörü kablosunu çıkar, pompayı AÇ | `SAFETY_FLOW_VERIFY_FAILED` (DRY_RUN **değil**) |
| 2.6 | Maks süre | `maxRunMs`'i 30 sn yap, pompayı AÇ | 30 sn + pay sonra zorla kapanır, sayaç artar |
| 2.7 | Tekrarlı aşım | 2.6'yı `maxRuntimeViolations` kadar tekrarla | Acil duruma geçer |
| 2.8 | Mandal kalıcılığı | Acil durumdayken **gücü kes**, aç | Mandal SÜRER, boot raporunda görünür |
| 2.9 | Onay reddi | Koşul düzelmeden acil durumu temizle | Reddedilir + neden |
| 2.10 | Onaylı kurtarma | Hazneyi doldur, temizle | Temizlenir, pompa tekrar açılabilir |

**M4 kapanma ölçütü:** 2.1–2.10 arasında **hepsi** beklenen sonucu vermeli.

---

## 3. Arıza enjeksiyonu (TASK-061)

| # | Arıza | Yöntem | Beklenen (§16.3) |
|---|---|---|---|
| 3.1 | Sensör kopuk | pH/EC kablosunu çıkar | Kalite `FAULT`, değer `—`, otomasyonda kullanılmaz |
| 3.2 | Ağ kaybı | Router'ı kapat | `BACKOFF` → 90 sn → AP açılır; **pompa kontrolü çalışmaya devam eder** |
| 3.3 | Ağ dönüşü | Router'ı aç | Otomatik STA'ya döner, AP linger sonra kapanır |
| 3.4 | Yanlış şifre | Hatalı parola kaydet | 3 denemede durur, `NET_AUTH_FAILED`; düzeltince devam |
| 3.5 | Dosya sistemi | `pio run -t erasefs` sonra boot | `DEGRADED`, API çalışır, statik varlık yok |
| 3.6 | OLED sökme | I2C kablosunu çıkar | **Sistem tam çalışır**, `UI_DISPLAY_UNAVAILABLE` |
| 3.7 | Watchdog reset | Geçici olarak bir task'a sonsuz döngü koy | WDT reset, boot'ta `SYS_WATCHDOG_RESET`, röleler kapalı |
| 3.8 | Heap baskısı | 4 WS istemcisi + sürekli tarama | `SYS_LOW_HEAP`, telemetri yavaşlar, geçmiş durur |

---

## 4. Kaynak profilleme (TASK-062)

### 4.1 Stack watermark → stack düzeltmesi

24 saat sonra `GET /api/diagnostics` → her task `minStack`.

```text
minStack < 512  → stack YETERSIZ, artir
minStack > 2048 → stack FAZLA, azalt (RAM israfi)
```

Düzeltilecek yer: `src/tasks/TaskConfig.h`. ISSUE-024 bu adımla kapanır.

### 4.2 Heap kararlılığı — 72 saat

`GET /api/state` → `minFreeHeap`. Sürekli düşüyorsa **sızıntı var**.

### 4.3 Flash aşınma

`GET /api/diagnostics` → `storage.total`. 24 saatte ~1440 olmalı
(60 sn periyot). Belirgin fazlaysa gereksiz yazma var.

---

## 5. Güvenlik doğrulaması (TASK-063)

| # | Test | Beklenen |
|---|---|---|
| 5.1 | Yetkisiz API | Token'sız `GET /api/state` → **401** |
| 5.2 | Yetkisiz WS | Token'sız `/ws` → el sıkışma **reddedilir** |
| 5.3 | Kaba kuvvet | 6 yanlış parola → 6.'da kilit, 60 sn |
| 5.4 | Kurulum AP dışından | STA IP'sinden `POST /api/setup/password` → **401** |
| 5.5 | Sır sızıntısı | `GET /api/config` yanıtında şifre **yok**, yalnızca `passwordSet` |
| 5.6 | OLED sır | Hiçbir ekranda Wi-Fi şifresi **yok** (AP kurulum şifresi hariç) |
| 5.7 | Hash süresi | Giriş yanıt süresini ölç → tur sayısını ayarla |

---

## 6. Otomasyon — YALNIZCA M4 KAPANDIKTAN SONRA

| # | Test | Beklenen |
|---|---|---|
| 6.1 | Eşik kuralı | EC eşiğin altına düşünce pompa açılır |
| 6.2 | Histerezis | Eşik civarında gürültüde röle **çırpınmaz** |
| 6.3 | Çizelge penceresi | Tanımlı saatte açılır, bitişte kapanır |
| 6.4 | Gece yarısı penceresi | 22:00–02:00 penceresi doğru çalışır |
| 6.5 | Saat geçersiz | NTP'siz boot → çizelgeler durur, **eşikler çalışır** |
| 6.6 | Manuel override | AUTO'da manuel komut → otomasyon susar, süre sonunda **devralır** |
| 6.7 | Güvenlik üstünlüğü | Otomasyon açmak isterken hazne boş → açılmaz |

---

## Sonuç kaydı

Her koşumda doldurulacak:

```text
Tarih:            Firmware:            Test eden:
M4 kapisi:        [ ] GECTI  [ ] KALDI
Basarisiz testler:
Olculen degerler: app_core __ ms · io_sense __ ms · ui __ ms
                  minStack: app_core __ · io_sense __ · net __ · ui __ · store __
                  minFreeHeap 24s sonra: __
```
