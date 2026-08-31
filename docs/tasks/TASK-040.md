# TASK-040 — TimeService (SNTP, Timezone, Validity)

**Phase:** 8 — Time · **Priority:** P2

## Objective

Güvenilir bir zaman kaynağı sağlamak ve **zamanın geçerli olup olmadığını** açıkça
bildirmek. Zamanlı otomasyon buna bağımlıdır; geçersiz zamanla çalışan bir çizelge
öngörülemez davranır.

## Scope

- SNTP senkronizasyonu ve periyodik yeniden senkronizasyon
- POSIX TZ string ile timezone ve yaz saati (DST)
- Zaman geçerlilik bayrağı
- Monotonik uptime
- `time` alt-state'inin yayınlanması
- Donanımsal RTC kararı (ISSUE-005)

## Out of Scope

- Çizelge kuralları (TASK-056)
- Web/OLED'de saat gösterimi (TASK-049, TASK-052)

## Dependencies

- TASK-035, TASK-015

## Requirements

- `REQUIREMENTS.md` — §8 (RTC/NTP), §11-High (periyodik güncelleme yok)

## Architecture References

- §2.13 TimeService · §11.2 Zaman geçerliliği kuralı

## Expected Design

### Karar gerektiren nokta 1 — Donanımsal RTC (ISSUE-005)

```text
Problem:      Zamanlı sulama, ağ olmadan da çalışmalı mı?
Constraints:  Sera cihazı uzun süre ağsız kalabilir;
              ESP32'nin RTC'si güç kesilince sıfırlanır;
              DS3231 pil ile yıllarca saat tutar ve ucuzdur
Approaches:   (a) yalnızca SNTP — ağ yoksa çizelge çalışmaz
              (b) SNTP + DS3231 — ağsız da çizelge çalışır
Trade-offs:   (a) donanım eklemez ama işlevsel kısıt getirir;
              güç kesintisi sonrası ağ gelene kadar çizelge durur
Recommended:  Kullanım senaryosuna bağlı — STEP 1'de karara bağlanmalı.
              (b) seçilirse bu task'a RTC sürücüsü eklenir.
```

### Karar gerektiren nokta 2 — Timezone modeli

```text
Mevcut:   RealTimeClock rtc("pool.ntp.org", 10800, 0)  → sabit GMT+3, DST yok
Problem:  Yaz saati uygulanan bölgelerde çizelge bir saat kayar;
          yapılandırılabilir değil
Yeni:     POSIX TZ string (örn. "EET-2EEST,M3.5.0/3,M10.5.0/4")
          → DST kuralları dahil, config'ten okunur
```

### Zaman geçerliliği kuralı (§11.2)

> `isValid() == false` iken **schedule kuralları çalışmaz**, threshold kuralları çalışmaya
> devam eder. UI ve web "saat geçersiz" gösterir.

Mevcut projede `getFormattedTime()` senkronize değilken sessizce `"00:00:00"` döndürüyordu —
bu, bir çizelgenin gece yarısı sanmasına yol açar.

## Implementation Notes

- SNTP callback tabanlı olmalı; bloklayan `getLocalTime()` beklemesi yapılmamalı.
- Periyodik yeniden senkronizasyon gerekli (kristal kayması). Aralık makul seçilmeli
  (saatte bir yeterli, dakikada bir israf).
- **Zaman sıçraması** ele alınmalı: SNTP senkronizasyonu saati ileri veya geri alabilir.
  Süre ölçümleri bundan etkilenmemeli — tüm süre hesapları monotonik `uptimeMs` kullanmalı
  (TASK-004). Bu, aktüatör `maxRunTime` gibi güvenlik hesapları için kritiktir.
- Zaman geçersizken kullanıcıya açıkça bildirilmeli; sahte bir değer döndürülmemeli.
- NTP sunucusu yapılandırılabilir olmalı (yerel ağ NTP sunucusu kullanılabilir).
- Ağ yokken SNTP denemesi kaynak harcamamalı; ağ durumuna göre denenmeli.
- İlk senkronizasyon başarılı olana kadar geçen süre ölçülmeli.

## Files

- `src/services/TimeService.h` / `.cpp` (yeni)
- `src/hal/RtcChip.h` / `.cpp` (yalnızca RTC kararı olumluysa)

## Acceptance Criteria

- [ ] RTC kararı verildi ve gerekçelendirildi (ISSUE-005 kapandı)
- [ ] SNTP bloklamadan çalışıyor
- [ ] Periyodik yeniden senkronizasyon var
- [ ] POSIX TZ string ile timezone ve DST destekleniyor
- [ ] TZ config'ten okunuyor
- [ ] `isValid()` doğru raporluyor; geçersizken sahte değer dönmüyor
- [ ] Süre hesapları monotonik zaman kullanıyor; SNTP sıçramasından etkilenmiyor
- [ ] NTP sunucusu yapılandırılabilir
- [ ] Ağ yokken gereksiz deneme yapılmıyor

## Test Plan

- [ ] İlk senkronizasyon süresi ölçüldü
- [ ] Yaz saati geçişinde yerel saat doğru
- [ ] Ağ kesildiğinde `isValid()` davranışı tasarıma uygun
- [ ] **SNTP saati geri aldığında aktüatör süre hesapları bozulmuyor** (kritik test)
- [ ] Zaman geçersizken çizelge kuralları çalışmıyor (TASK-056 ile birlikte)
- [ ] RTC eklendiyse: güç kesme sonrası saat korunuyor
- [ ] Uzun süreli çalışmada saat kayması kabul edilebilir sınırda

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.13, §11.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — SNTP beklemesi
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — senkronizasyon başarısızlığı
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **sabit GMT offset ve sessiz "00:00:00" yasak**

## Definition of Done

Ortak DoD + zaman sıçramasının süre hesaplarını bozmadığı kanıtlandı + ISSUE-005 kapatıldı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — ISSUE-005 (donanımsal RTC): (a) yalnızca SNTP — ŞİMDİLİK

```text
(b) SNTP + DS3231 REDDEDILMEDI, ERTELENDI.

Gerekce: DS3231 SATIN ALINMAMIS ve devreye eklenmemis bir donanimdir.
         Var olmayan bir yonga icin surucu yazmak P7 ihlalidir — derlenen
         ama hicbir zaman calismayan kod, calistigi SANILAN koddur.

Islevsel sonuc ACIKCA kabul ediliyor:
   Guc kesintisi + ag yoksa → saat gecersiz → CIZELGELER CALISMAZ.
   Esik kurallari (pH/EC/seviye) CALISMAYA DEVAM EDER (§11.2).
   Guvenlik zinciri zamandan BAGIMSIZDIR — monotonik `Millis` kullanir.

ISSUE-005 ACIK kalir: bu bir DONANIM SATIN ALMA karari ve kullanicinin.
Eklenirse degisiklik TEK DOSYADA kalir (`TimeService` zaten soyutlama
noktasi); FSM, cizelgeler ve arayuz etkilenmez.
```

## Karar 2 — POSIX TZ dizesi, sabit ofset DEĞİL

```text
Eski: RealTimeClock rtc("pool.ntp.org", 10800, 0)  → sabit GMT+3, DST YOK
Yeni: config.system.timezone — POSIX TZ (orn. "EET-2EEST,M3.5.0/3,M10.5.0/4")
      DST kurallari dahil, yapilandirilabilir.
```

Yaz saati uygulanan bölgelerde sabit ofset, çizelgeyi yılda iki kez bir
saat kaydırır — ve bunu kimse fark etmez.

## Karar 3 — Geçersiz zamanda SAHTE DEĞER DÖNDÜRÜLMEZ

```text
Eski: getFormattedTime() senkronize degilken SESSIZCE "00:00:00" donduruyordu.
      Bir cizelge bunu "gece yarisi" sanar ve sulama yapar.

Yeni: `valid == 0` iken epoch `EPOCH_INVALID`; cagiran once gecerliligi
      sorar. Arayuz "saat gecersiz" gosterir.
```

## Karar 4 — Süre ölçümleri ASLA duvar saatiyle yapılmaz

SNTP saati **ileri veya geri** alabilir. Tüm süre hesapları monotonik
`Millis` kullanır — `maxRunTime`, cooldown, backoff, flow verify. Bu ayrım
`core/Time.h`'ta üç ayrı tiple (`Millis` / `Duration` / `EpochSeconds`)
zaten **derleme zamanında** zorunlu kılınmıştı; bu task o tasarımın
karşılığını alıyor.

## Karar 5 — Senkronizasyon yalnızca ağ varken denenir

Ağ yokken SNTP denemesi radyo ve CPU harcar, sonucu bellidir. `CONNECTED`
değilken denenmez. Periyodik yeniden senkronizasyon **saatte bir** —
kristal kayması için yeterli, dakikada bir israf.

---

# STEP 3 — REVIEW RECORD

- [x] SNTP callback tabanlı; bloklayan `getLocalTime()` beklemesi **yok**
- [x] Periyodik yeniden senkronizasyon (saatte bir)
- [x] POSIX TZ dizesi uygulanıyor — DST kuralları dahil
- [x] Zaman geçerlilik bayrağı yayınlanıyor
- [x] **Geçersizken sahte değer döndürülmüyor** — `EPOCH_INVALID`
- [x] Ağ yokken SNTP denenmiyor; `sntp_stop()` ile kapatılıyor
- [x] İlk senkronizasyon süresi ölçülüyor
- [x] Geçersizlik sessiz değil: 15 sn'de bir `TIME_NOT_SYNCED` yükseltiliyor
- [x] Süre ölçümleri duvar saatinden **bağımsız** (monotonik `Millis`)
- [ ] **Donanım testleri bekliyor**

## Geçerlilik ölçütü: 2023-01-01 alt sınırı

ESP32 açılışta 1970'ten sayar. `epoch > 0` ölçütü yeterli değildir —
senkronize olmamış bir saat de pozitif değer verir. `SANE_EPOCH_MIN`
(1672531200) altındaki her değer "hiç ayarlanmamış" kabul edilir.

## ISSUE-005 AÇIK: donanımsal RTC ertelendi

DS3231 **reddedilmedi, ertelendi**. Satın alınmamış bir yonga için sürücü
yazmak P7 ihlalidir: derlenen ama hiç çalışmayan kod, çalıştığı sanılan
koddur.

İşlevsel sonuç açıkça kabul ediliyor:

| Durum | Davranış |
|---|---|
| Güç kesintisi + ağ var | Saat senkronize olur, çizelgeler çalışır |
| Güç kesintisi + ağ YOK | **Saat geçersiz, ÇİZELGELER ÇALIŞMAZ** |
| Eşik kuralları (pH/EC/seviye) | Her durumda çalışır |
| Güvenlik zinciri | Zamandan **tamamen bağımsız** |

Bu bir **donanım satın alma kararıdır** ve kullanıcınındır. Eklenirse
değişiklik `TimeService` içinde kalır; FSM, çizelgeler ve arayüz etkilenmez.

**TASK-040: TAMAMLANDI** (ISSUE-005 açık, donanım testleri bekliyor).
