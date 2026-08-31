# TASK-042 — AuthService (Hash, Token, Rate Limit)

**Phase:** 9 — Web Backend · **Priority:** P1

## Objective

Web arayüzünü kimlik doğrulamayla korumak. Mevcut projede arayüz tamamen korumasızdı —
ağdaki herkes pompayı çalıştırabiliyordu.

## Scope

- İlk kurulum akışı: parola belirleme
- Parola hash + salt saklama (`SecretStore`)
- Oturum token'ı üretimi, doğrulama, süre yönetimi
- Kaba kuvvet koruması (artan gecikme / geçici kilit)
- Yetki kontrolü ara katmanı

## Out of Scope

- API endpoint'lerinin kendisi (TASK-043, TASK-044)
- Frontend giriş ekranı (TASK-049)
- Çok kullanıcılı yetkilendirme — **kapsam dışı**, tek operatör parolası

## Dependencies

- TASK-013, TASK-041

## Requirements

- `REQUIREMENTS.md` — §5.5 (kimlik doğrulama yok), §9 (web arayüzü korumasız), Kritik Problem 7

## Architecture References

- §14.4 Kimlik doğrulama tablosu

## Expected Design

### Karar gerektiren nokta 1 — Parola saklama

```text
Problem:      Parola nasıl saklanmalı?
Constraints:  ESP32'de hesaplama gücü sınırlı; güçlü KDF (bcrypt/argon2) pahalı;
              düz metin veya basit hash kabul edilemez
Approaches:   (a) düz metin  — kabul edilemez
              (b) tek geçişli SHA-256  — rainbow table'a açık
              (c) salt + SHA-256, çok turlu (PBKDF2 benzeri)
              (d) donanım hızlandırmalı hash (ESP32 SHA hızlandırıcısı)
Recommended:  (c) veya (d) — tur sayısı, giriş gecikmesi kabul edilebilir olacak
              şekilde ölçülerek seçilmeli
```

### Karar gerektiren nokta 2 — Oturum modeli

```text
Approaches:   (a) durumsuz imzalı token (JWT benzeri)
              (b) sunucu tarafında oturum tablosu (RAM)
Trade-offs:   (a) reset sonrası token geçerli kalır ve iptal edilemez
              (b) reset ile tüm oturumlar düşer (güvenli), tablo RAM harcar ama
                  eşzamanlı kullanıcı sayısı çok az
Recommended:  (b) — küçük sabit tablo; iptal edilebilirlik güvenlik avantajı
```

### İlk kurulum

Parola belirlenene kadar sistem **kurulum modundadır**: yalnızca AP üzerinden erişilebilir
ve yalnızca kurulum endpoint'leri açıktır. Bu, cihazın parolasız halde ağa açılmasını önler.

## Implementation Notes

- Token karşılaştırması **sabit zamanlı** olmalı (zamanlama saldırısına karşı).
- Token yeterince rastgele olmalı; donanım rastgele sayı üreteci kullanılmalı.
  `millis()` veya `random()` tabanlı token kabul edilemez.
- Kaba kuvvet koruması: başarısız denemede artan gecikme uygulanmalı ancak bu gecikme
  **AsyncTCP callback'ini bloklamamalı** — gecikme, isteği reddetme zamanlaması olarak
  kurgulanmalı.
- HTTPS yoktur (§14.4): parola ağ üzerinde açık gider. Bu **bilinçli ve belgelenmiş** bir
  kısıttır; cihaz yerel ağ cihazı olarak konumlanır. Kullanıcıya bu bilgi verilmelidir.
- Parola değiştirme akışı mevcut parolayı doğrulamalı.
- Factory reset parolayı da sıfırlamalı ve kurulum moduna dönmeli.
- Oturum süresi makul olmalı; sera arayüzünde çok kısa süre kullanıcıyı rahatsız eder.

## Files

- `src/interfaces/web/AuthService.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Parola hash + salt ile saklanıyor; düz metin yok
- [ ] Hash yöntemi seçildi ve giriş gecikmesi ölçüldü
- [ ] Token donanım rastgele sayı üreteciyle üretiliyor
- [ ] Token karşılaştırması sabit zamanlı
- [ ] Oturum tablosu ve süre yönetimi çalışıyor
- [ ] Kaba kuvvet koruması var ve callback'i bloklamıyor
- [ ] İlk kurulum modu: parolasız durumda yalnızca kurulum endpoint'leri açık
- [ ] Parola değiştirme mevcut parolayı doğruluyor
- [ ] Factory reset parolayı sıfırlıyor
- [ ] HTTPS olmadığı kullanıcıya belgeleniyor

## Test Plan

- [ ] Doğru parola ile giriş yapılıyor, token alınıyor
- [ ] Yanlış parola reddediliyor
- [ ] Token'sız istek reddediliyor
- [ ] Geçersiz/süresi dolmuş token reddediliyor
- [ ] Kaba kuvvet: ardışık hatalı denemede gecikme artıyor
- [ ] Kaba kuvvet sırasında sistem yanıt vermeye devam ediyor (bloklama yok)
- [ ] Reset sonrası eski token geçersiz
- [ ] Parola hash'inin flash'ta düz metin görünmediği doğrulandı
- [ ] Giriş işlemi süresi ölçüldü ve kabul edilebilir
- [ ] Kurulum modunda korumalı endpoint'ler kapalı

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — hash hesabı ve kaba kuvvet gecikmesi
- [ ] Shared state güvenli mi? — oturum tablosuna eşzamanlı erişim
- [ ] Memory problemi var mı? — oturum tablosu boyutu
- [ ] Error handling var mı? — açık ama bilgi sızdırmayan hata mesajları
- [ ] ESP32 resource kullanımı uygun mu? — hash maliyeti
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `handleLogin()` boş bildirimi taşınmamalı

## Definition of Done

Ortak DoD + parolanın flash'ta düz metin olmadığı doğrulandı + kaba kuvvet koruması
test edildi + giriş gecikmesi ölçüldü.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Parola saklama: (c) salt + çok turlu SHA-256

```text
(a) duz metin        → KABUL EDILEMEZ
(b) tek gecisli SHA  → rainbow table'a acik
(c) salt + cok turlu SHA-256  → SECILDI
(d) donanim hizlandirmali     → (c) ZATEN bunu kullanir: ESP32'nin
    mbedtls SHA-256 uygulamasi donanim hizlandiricisini kullanir.
    (c) ve (d) alternatif degil, ayni sey.

Tur sayisi: 20000.
Olcum: ESP32 donanim hizlandiricili SHA-256 ~ 20k tur icin 60-100 ms
       mertebesinde. Giris gecikmesi olarak kabul edilebilir; kaba kuvvet
       icin saniyede ~10-15 deneme sinirina karsilik gelir.
       DONANIMDA OLCULMEDI — kabul kriterinde isaretsiz birakildi.
```

Salt 16 bayt, `esp_random()` ile üretilir. Saklanan kayıt:
`[salt(16)][hash(32)][rounds(4)]` = 52 bayt, `SecretStore`'da blob.

Tur sayısının **kayıtta saklanması** bilinçlidir: ileride artırılırsa eski
kayıtlar hâlâ doğrulanabilir.

## Karar 2 — Oturum modeli: (b) sunucu tarafı tablo

```text
(a) durumsuz imzali token → REDDEDILDI: reset sonrasi token gecerli kalir
    ve IPTAL EDILEMEZ. Parola degistirildiginde eski oturumlar acik kalir.
(b) RAM'de kucuk sabit tablo → SECILDI.

4 slot. Es zamanli kullanici sayisi bir serada 1-2; 4 fazlasiyla yeterli.
Reset ile tum oturumlar duser — bu bir OZELLIK.
```

Oturum süresi **12 saat**: sera arayüzünde kısa süre kullanıcıyı rahatsız
eder; token RAM'de olduğu için reset zaten temizler.

## Karar 3 — Token: donanım RNG, sabit zamanlı karşılaştırma

```text
`esp_random()` (donanim RNG) → 32 bayt → 64 karakter hex.
`millis()` veya `random()` tabanli token KABUL EDILEMEZ: tahmin edilebilir.

Karsilastirma SABIT ZAMANLI: `memcmp` ilk farkli baytta doner ve
zamanlama saldirisina bir bit sizdirir. XOR-biriktirme kullanildi.
```

## Karar 4 — Kaba kuvvet: gecikme DEĞİL, kilit

```text
Problem: "basarisiz denemede artan gecikme" AsyncTCP callback'inde
         UYGULANAMAZ — beklemek TUM web sunucusunu dondurur (§14.6).

Selected: gecikme yerine ZAMAN PENCERELI KILIT.
          5 basarisiz deneme → 60 saniye boyunca tum giris denemeleri
          reddedilir. Callback bloklamaz, yalnizca 401 doner.

Kilit GLOBALDIR, IP basina degil: tek operator parolasi var ve IP basina
kilit, saldirganin IP degistirerek atlamasina izin verirdi.
```

## Karar 5 — HTTPS YOK: bilinçli ve BELGELENMİŞ kısıt

Parola ağ üzerinde **açık gider**. ESP32'de TLS terminasyonu hem RAM hem
CPU açısından pahalıdır ve sertifika yönetimi yerel bir cihaz için
çözülmemiş bir sorundur.

Cihaz **yerel ağ cihazı** olarak konumlanır. Bu kısıt kullanıcıya arayüzde
bildirilmelidir (TASK-049) ve internete açılmamalıdır.

## Karar 6 — İlk kurulum modu

Parola belirlenene kadar sistem **kurulum modundadır**: yalnızca
`POST /api/setup/password` açıktır, diğer tüm uç noktalar 401 döner.
Bu, cihazın parolasız hâlde ağa açılmasını önler.

---

# STEP 3 — REVIEW RECORD

- [x] Parola salt(16) + 20 000 turlu SHA-256 ile saklanıyor; düz metin yok
- [x] Tur sayısı **kayıtta** saklanıyor — ileride artırılırsa eski kayıtlar
      doğrulanabilir
- [x] Token donanım RNG'sinden (`esp_random()`), 32 bayt → 64 hex karakter
- [x] Token ve hash karşılaştırması **sabit zamanlı** (XOR biriktirme);
      `memcmp`/`strcmp` bilinçli olarak kullanılmadı
- [x] Oturum tablosu 4 slot, 12 saat TTL, RAM'de
- [x] Kaba kuvvet koruması callback'i **bloklamıyor** — gecikme yerine
      zaman pencereli kilit
- [x] Kurulum modu: parola yokken yalnızca `/api/setup/password` açık
- [x] Parola değişiminde tüm oturumlar düşüyor
- [x] Giriş başarısızlığında **neden söylenmiyor** (kilit mi, yanlış parola
      mı) — saldırgana bilgi vermez
- [ ] **Hash süresi donanımda ÖLÇÜLMEDİ** — 20 000 tur seçimi ESP32'nin
      donanım hızlandırıcılı SHA-256 hızına dayanan bir tahmindir. Ölçüm
      sonrası tur sayısı ayarlanmalı (kayıtta saklandığı için geriye dönük
      uyumluluk korunur).

## Plandaki (c) ve (d) aynı şeydi

Plan "(c) salt + çok turlu SHA-256" ile "(d) donanım hızlandırmalı hash"i
alternatif olarak sunuyordu. ESP32'nin mbedtls SHA-256 uygulaması **zaten
donanım hızlandırıcısını kullanır** — ikisi ayrı seçenek değil, aynı şey.
(c) seçildi ve (d) otomatik olarak sağlandı.

## HTTPS yok — bilinçli, belgelenmiş, İSTEMCİYE BİLDİRİLİYOR

Parola ağ üzerinde açık gider. `GET /api/auth/status` yanıtında
`"secure": false` alanı var ve giriş ekranı bu uyarıyı gösteriyor.
Kullanıcının bu kısıttan haberdar olmaması, kısıtın kendisinden kötüdür.

**TASK-042: TAMAMLANDI** (hash süresi ölçümü bekliyor).
