# TASK-037 — Retry & Backoff Strategy

**Phase:** 7 — Network · **Priority:** P1

## Objective

Yeniden bağlanma denemelerini akıllı bir stratejiyle yönetmek: radyo ve güç israfını
önlemek, kalıcı hataları (yanlış şifre) sonsuz denemeden ayırt etmek.

## Scope

- Üstel backoff hesabı ve tavan değeri
- Jitter (rastgele sapma)
- Disconnect neden koduna göre farklı davranış
- Deneme sayacı ve sıfırlama kuralları
- Sonraki deneme zamanının state'e yayınlanması

## Out of Scope

- FSM durumları (TASK-035)
- AP fallback tetiklemesi (TASK-038)
- Bağlantı kurma (TASK-036)

## Dependencies

- TASK-036

## Requirements

- `REQUIREMENTS.md` — §2 (reconnect, bağlantı kaybı yönetimi `[~]`)

## Architecture References

- §8.2 Backoff satırı ve disconnect neden satırı

## Expected Design

### Karar gerektiren nokta — Yeniden deneme stratejisi

Bu, `IMPLEMENTATION_PLAN.md` §2.1'de örnek olarak verilen karardır. Eski kodun yaklaşımı
(sabit 1 saniye, sonsuz) **varsayılan çözüm değildir**.

```text
Problem:      Bağlantı koptuğunda ne sıklıkta yeniden denenmeli?
Constraints:  Her deneme radyo enerjisi ve CPU harcar;
              AP geçici olarak kapalıysa hızlı dönüş istenir;
              AP kalıcı gitmişse sürekli deneme israftır;
              yanlış şifrede deneme hiçbir zaman başarılı olmaz
Approaches:   (a) sabit aralıklı deneme (mevcut proje: 1 sn)
              (b) üstel backoff (1→2→4→8→16→32→60 sn, tavanlı)
              (c) tamamen event güdümlü (yalnızca AP göründüğünde dene)
              (d) üstel backoff + jitter + neden koduna göre farklılaştırma
Trade-offs:   (a) israf ve gereksiz radyo yükü
              (c) AP taraması da maliyetlidir, tek başına yetmez
              (d) en dengeli; jitter, çoklu cihazın aynı anda denemesini önler
Recommended:  (d)
```

### Neden koduna göre davranış

| Neden | Davranış |
|---|---|
| Kimlik doğrulama hatası (yanlış şifre) | **Sınırlı deneme** (örn. 3), sonra dur ve kullanıcıya bildir. Sonsuz deneme anlamsızdır. |
| AP bulunamadı | Backoff ile denemeye devam |
| Bağlantı koptu (sinyal) | İlk deneme hızlı, sonra backoff |
| AP tarafından reddedildi | Backoff ile devam |

### Sayaç sıfırlama

Başarılı bağlantıdan sonra deneme sayacı sıfırlanmalı — ancak **hemen değil**. Bağlantı
kurulup 1 saniye sonra tekrar kopuyorsa bu "başarılı" sayılmamalı; aksi halde backoff hiç
devreye girmez ve sistem sürekli hızlı deneme yapar. Bağlantının **stabil kaldığı**
bir süre (örn. 30 sn) beklenmelidir.

## Implementation Notes

- Backoff hesabı saf fonksiyon olmalı; host tarafında test edilebilsin.
- Jitter oranı (%20 civarı) tavan değerini aşmamalı.
- Kullanıcı arayüzünden "şimdi dene" komutu backoff'u atlayabilmeli — kullanıcı sorunu
  düzelttiğinde 60 saniye beklemek zorunda kalmamalı.
- Sonraki deneme zamanı state'e yayınlanmalı ki arayüz "8 sn sonra tekrar denenecek"
  gösterebilsin. Sessiz bekleme kullanıcıya "bozuk" izlenimi verir.
- Kimlik doğrulama hatasında durma kararı **kalıcı olmamalı**: yeni credential girilince
  sayaç sıfırlanmalı.
- Yanlış şifre tespiti güvenilir olmalı; bazı AP'ler kimlik hatasında farklı kod döner.

## Files

- `src/services/network/RetryPolicy.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Strateji seçildi ve gerekçesi yazıldı
- [ ] Üstel backoff tavanlı ve jitter'lı
- [ ] Neden koduna göre farklı davranış uygulanıyor
- [ ] Kimlik doğrulama hatasında sınırlı deneme sonrası duruluyor
- [ ] Yeni credential girilince sayaç sıfırlanıyor
- [ ] Sayaç sıfırlama stabil bağlantı süresine bağlı
- [ ] "Şimdi dene" komutu backoff'u atlıyor
- [ ] Sonraki deneme zamanı yayınlanıyor
- [ ] Backoff hesabı host'ta test edilebilir

## Test Plan

- [ ] AP kapatıldığında deneme aralıkları ölçüldü; üstel artış doğrulandı
- [ ] Tavan değeri aşılmıyor
- [ ] Jitter uygulanıyor (ardışık denemelerde aralık birebir aynı değil)
- [ ] Yanlış şifre ile sınırlı deneme sonrası duruluyor
- [ ] Yeni credential sonrası deneme yeniden başlıyor
- [ ] Kısa süreli bağlantı sonrası kopmada backoff sıfırlanmıyor
- [ ] "Şimdi dene" komutu anında deneme başlatıyor
- [ ] Host tarafında backoff hesabı sınır değerlerde test edildi

## Review Checklist

- [ ] Architecture'a uygun mu? (§8.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — neden kodu sınıflandırması eksiksiz mi
- [ ] ESP32 resource kullanımı uygun mu? — radyo/güç israfı önlendi mi
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **sabit 1 sn sonsuz deneme taşınmamalı**

## Definition of Done

Ortak DoD + deneme aralıkları ölçümle doğrulandı + yanlış şifre senaryosunda sonsuz
deneme yapılmadığı kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — (d) üstel backoff + jitter + neden koduna göre farklılaştırma

Eski kodun yaklaşımı (sabit 1 sn, sonsuz) **varsayılan çözüm olarak
alınmadı** — IMPLEMENTATION_PLAN §2.1'in örnek kararı budur.

```text
(a) sabit 1 sn, sonsuz  → REDDEDILDI: yanlis sifrede sonsuz deneme radyo
    enerjisi ve CPU harcar, hicbir zaman basarili olmaz
(c) yalnizca event gudumlu → REDDEDILDI: AP taramasi da maliyetlidir
(d) SECILDI

Taban 1 sn, ×2, TAVAN 60 sn:  1 → 2 → 4 → 8 → 16 → 32 → 60 → 60 ...
Jitter ±%20, tavani ASMAZ.
```

**Jitter neden gerekli:** aynı ortamdaki birden fazla cihaz aynı anda güç
alırsa (elektrik kesintisi sonrası) hepsi aynı saniyelerde dener ve AP'yi
gereksiz yükler. Jitter denemeleri dağıtır.

## Karar 2 — Neden koduna göre davranış

| Sınıf | Davranış | Gerekçe |
|---|---|---|
| `AUTH_FAILED` | 3 denemeden sonra **DUR** | Şifre yanlışsa 1000. deneme de başarısız olur |
| `AP_NOT_FOUND` | backoff ile devam | AP geri gelebilir |
| `LINK_LOST` | **ilk deneme hızlı** (500 ms), sonra backoff | Geçici sinyal kaybı çoğu zaman hemen düzelir |
| `REJECTED` | backoff ile devam | AP meşgul; ısrar etmek işe yaramaz |
| `UNKNOWN` | backoff ile devam | Güvenli varsayılan |

## Karar 3 — Kimlik hatasında durma KALICI DEĞİL

Yeni credential girildiğinde sayaç sıfırlanır (`onCredentialsChanged()`).
Aksi hâlde kullanıcı şifreyi düzeltir ama sistem hâlâ durmuş olur — ve
bunu anlamanın hiçbir yolu olmaz.

Ayrıca "şimdi dene" komutu (`NETWORK_RETRY_NOW`, TASK-008'de zaten tanımlı)
backoff'u **ve** durma kararını atlar: kullanıcı sorunu düzelttiğinde
60 saniye beklemek zorunda kalmamalı.

## Karar 4 — Sayaç sıfırlama STABİLİTE bekler

```text
Naif: baglandi → sayaci sifirla.
Problem: baglanti kurulup 1 saniye sonra tekrar kopuyorsa bu "basarili"
         sayilir; backoff HIC devreye girmez ve sistem sonsuz hizli
         deneme dongusune girer — tam olarak kacinmak istedigimiz sey.

Selected: baglanti STABLE_MS (30 sn) boyunca ayakta kalirsa sayac sifirlanir.
```

## Karar 5 — Hesap SAF fonksiyon, rastgelelik DIŞARIDAN

`delayFor(sinif, deneme, rastgele)` — `esp_random()` çağırmaz, rastgele
baytı parametre alır. Böylece TASK-064 backoff eğrisini ve jitter sınırlarını
**deterministik** olarak test edebilir. Rastgelelik kaynağı çağıranın işidir.

---

# STEP 3 — REVIEW RECORD

- [x] Üstel backoff + tavan + jitter uygulandı
- [x] Neden koduna göre farklılaşma (`AUTH_FAILED` sınırlı, `LINK_LOST` hızlı ilk deneme)
- [x] Deneme sayacı **stabilite** bekleyerek sıfırlanıyor (30 sn)
- [x] `nextRetryAt` yayınlanıyor — kullanıcı "8 sn sonra" görebiliyor
- [x] Kimlik hatasında durma **kalıcı değil**: yeni credential veya
      "şimdi dene" komutu sıfırlıyor
- [x] Hesaplar **saf**; rastgelelik parametre olarak geliyor
- [x] **11 `static_assert` derlemeyi geçti** — backoff eğrisi, tavan, jitter
      sınırları ve taşma davranışı derleme zamanında kanıtlandı:

```text
baseDelayMs(200) == 60000     → buyuk deneme sayisinda TASMA YOK
applyJitter(10000, 0)   == 8000    → alt sinir -%20
applyJitter(10000, 255) == 12000   → ust sinir +%20
applyJitter(60000, 255) == 60000   → jitter TAVANI ASAMAZ
delayFor(AUTH_FAILED, 0, x) != FAST_RETRY_MS  → kimlik hatasinda hizli deneme YASAK
shouldStop(AP_NOT_FOUND, 99) == false          → AP icin ASLA durmaz
```

`delayFor(AUTH_FAILED, ...)` üzerindeki negatif iddia bilinçli: kimlik
hatasının hızlı deneme yoluna düşmesi sonsuz döngü üretirdi ve bu, testle
değil **derlemeyle** engellendi.

**TASK-037: TAMAMLANDI.**
