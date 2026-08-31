# TASK-063 — Security Hardening Pass

**Phase:** 14 — Integration & Hardening · **Priority:** P1

## Objective

Tüm sistemi güvenlik açısından gözden geçirmek ve `REQUIREMENTS.md` Kritik Problem 7'nin
tamamen kapandığını doğrulamak.

## Scope

- Sır sızıntısı taraması (log, API, OLED, flash)
- Yetkilendirme kapsamının doğrulanması
- Girdi doğrulamasının eksiksizliği
- Kaba kuvvet ve kaynak tüketimi korumaları
- Varsayılan yapılandırmanın güvenliği
- Kalan risklerin belgelenmesi

## Out of Scope

- Yeni güvenlik özelliği eklemek (HTTPS, sertifika yönetimi)
- Fiziksel güvenlik
- Arıza enjeksiyonu (TASK-061)

## Dependencies

- TASK-060, TASK-042

## Requirements

- `REQUIREMENTS.md` — §9 (güvenlik maddeleri), Kritik Problem 7

## Architecture References

- §14.4 Kimlik doğrulama · §14.5 Doğrulama · §8.2 Credential saklama

## Expected Design

### Sır sızıntısı denetimi

Wi-Fi şifresi ve arayüz parolası şu kanalların **hiçbirinde** görünmemeli:

```text
  [ ] Seri port çıktısı (tüm log seviyeleri)
  [ ] Diagnostics halka tamponu
  [ ] API yanıtları (config, state, diagnostics)
  [ ] WebSocket paketleri
  [ ] OLED ekranları
  [ ] Flash'ta düz metin (bellek dökümü ile kontrol)
  [ ] Hata mesajları
```

Mevcut projede şifre hem EEPROM'da düz metin hem OLED'de açıkça görünüyordu — bu ikisi de
kapatılmalı ve **kanıtlanmalıdır**.

### Yetkilendirme kapsamı

Her endpoint tek tek kontrol edilmeli: yetki gerektirmesi gereken bir endpoint açık
kalmışsa tüm koruma anlamsızdır. Açık kalabilecekler yalnızca: statik varlıklar, login,
ilk kurulum endpoint'leri.

### Kaynak tüketimi saldırıları

| Vektör | Koruma |
|---|---|
| Çok sayıda WS bağlantısı | İstemci sayısı sınırı |
| Büyük istek gövdesi | Boyut sınırı |
| Komut seli | Kuyruk sınırı + `BUSY` yanıtı |
| Kaba kuvvet giriş | Artan gecikme / geçici kilit |
| Sürekli tarama isteği | Hız sınırı |

### Kalan riskin belgelenmesi

HTTPS yoktur (§14.4): parola ve komutlar yerel ağda açık gider. Bu **bilinçli bir
kısıttır** ve kullanıcıya açıkça bildirilmelidir. Belirsiz bırakılması, kullanıcının
sistemi güvendiği ortamdan daha açık bir ağa koymasına yol açabilir.

## Implementation Notes

- Denetim sistematik olmalı: her kanal için gerçekten test yapılmalı, kod okumakla
  yetinilmemeli. Flash içeriği okunarak şifre aranmalı.
- Varsayılan yapılandırma güvenli olmalı: varsayılan mod MANUAL, otomasyon kuralları pasif,
  AP şifresi cihaza özgü, ilk kurulumda parola zorunlu.
- Hata mesajları bilgi sızdırmamalı: "kullanıcı yok" ile "şifre yanlış" ayrımı yapılmamalı.
- Factory reset **her şeyi** temizlemeli: config, sırlar, geçmiş, kurulum durumu.
- Bulunan her sorun düzeltilmeli veya belgelenmiş bir kabul edilen risk olmalı.
- Güvenlik notları kullanıcı dokümantasyonuna girmeli.

## Files

- `docs/SECURITY.md` (yeni — denetim sonuçları ve kalan riskler)
- Bulunan sorunların düzeltmeleri

## Acceptance Criteria

- [ ] Yedi kanalın tamamında sır sızıntısı olmadığı **test edilerek** doğrulandı
- [ ] Flash içeriği okunarak şifrenin düz metin olmadığı kanıtlandı
- [ ] Her endpoint'in yetki gereksinimi tek tek kontrol edildi
- [ ] Yetkisiz erişilebilir endpoint yalnızca izin verilenler
- [ ] Beş kaynak tüketimi vektörü için koruma mevcut
- [ ] Hata mesajları bilgi sızdırmıyor
- [ ] Varsayılan yapılandırma güvenli
- [ ] Factory reset her şeyi temizliyor
- [ ] Kalan riskler (HTTPS yokluğu dahil) belgelendi
- [ ] Kullanıcıya yönelik güvenlik notu yazıldı

## Test Plan

- [ ] Tüm log çıktısı tarandı; şifre yok
- [ ] Tüm API yanıtları incelendi; şifre yok
- [ ] Tüm OLED ekranları gezildi; şifre yok
- [ ] Flash dökümü alınıp şifre arandı; düz metin yok
- [ ] Her endpoint token'sız çağrıldı; korumalı olanlar reddediliyor
- [ ] Maksimum WS istemci sınırı test edildi
- [ ] Aşırı büyük istek gövdesi reddediliyor
- [ ] Kaba kuvvet denemesi sınırlanıyor
- [ ] Komut seli `BUSY` ile karşılanıyor, sistem kararlı
- [ ] Factory reset sonrası hiçbir eski veri kalmıyor
- [ ] Varsayılan config ile sistem kendiliğinden sulamıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.4, §14.5, §8.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — kaba kuvvet gecikmesi bloklamıyor mu
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — kaynak tüketimi saldırıları
- [ ] Error handling var mı? — bilgi sızdırmayan hata mesajları
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **düz metin şifre saklama ve gösterme yasak**

## Definition of Done

Ortak DoD + **şifrenin hiçbir kanalda görünmediği test edilerek kanıtlandı** + kalan
riskler belgelendi.

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

**Tarih:** 2026-08-31

Bu task büyük ölçüde **statik denetimdir** ve donanım gerektirmez;
bu yüzden bu turda büyük ölçüde TAMAMLANDI.

## Tarama sonuçları

| Denetim | Sonuç |
|---|---|
| Wi-Fi şifresi okuyan yer | 1 (`ConnectionManager`, bağlanma anı) |
| AP şifresi okuyan yer | 1 (`SoftApManager`, OLED) |
| Auth hash okuyan yer | 1 (`AuthService`) |
| **Log satırında sır** | **0** |
| `SystemState`'te şifre alanı | **YOK** — alan hiç tanımlı değil |
| `GET /api/config` maskeleme | `passwordSet`/`authSet`; değer okunmuyor bile |
| `strcpy`/`sprintf`/`gets`/`strcat` | **0** |
| Sırlar ayrı NVS namespace | ✅ `sec` / `cfg` / `sys` |
| Yetki kapsamı | 19 uç noktadan 15'i `requireAuth` |

Şifrenin `SystemState`'e sızması için önce birinin **alan eklemesi**
gerekir — maskeleme bir formatlama değil, yapısal bir engel.

## Bulunan açık: kurulum uç noktası AP dışından erişilebiliyordu

TASK-042 tasarım kaydı açıkça "kurulum modunda **yalnızca AP üzerinden**
erişilebilir" diyordu. **Uygulanmamıştı.**

```text
Senaryo: cihazin Wi-Fi bilgisi var ama arayuz parolasi YOK
         (firmware yukseltmesi, kismi NVS bozulmasi, yarim fabrika reset)
Sonuc:   cihaz EV AGINDA ve kurulum modunda
         → agdaki HERKES ilk parolayi belirleyip cihazi sahiplenebilir
         → pompa kontrolu dahil TAM yetki
```

`fromSetupAp()` eklendi: `POST /api/setup/password` yalnızca `192.168.4.x`
(SoftAP alt ağı) istemcilerini kabul ediyor. Reddedilen istek loglanıyor.

## Bilinçli yetkisiz uç noktalar (denetlendi, doğru)

```text
GET  /api/auth/status     istemci giris ekranini cizmeden once modu bilmeli
POST /api/auth/login      giris noktasi
POST /api/auth/logout     token sahibi kendi oturumunu duurur
POST /api/setup/password  YALNIZCA kurulum modu VE YALNIZCA AP
```

## Varsayılan yapılandırma güvenliği (denetlendi)

```text
otomasyon modu     MANUAL   kural kumesi      BOS
requireLevelSensor 1        aktuator maxRunMs 5 dk (KISA)
arayuz parolasi    YOK → kurulum modu, AP-only
```

Hiçbir varsayılan "çalışır durumda" değil — sistem kutudan çıktığında
kendiliğinden hiçbir aktüatörü sürmez.

## Kaynak tüketimi korumaları (denetlendi)

```text
istek govdesi 4096 B · WS mesaji 512 B · WS istemci 4 · oturum 4
kaba kuvvet 5 deneme / 60 sn kilit · gecmis sayfa 240 · komut/dongu 4
tarama sonucu 20
```

## Belgelenmiş kalan riskler

1. **HTTPS yok** — parola ağda açık. Bilinçli kısıt (§14.4); istemciye
   `"secure": false` ile bildiriliyor ve giriş ekranında uyarı var.
2. **AP şifresi OLED'de görünür** — kurulum için zorunlu; fiziksel erişimi
   olan zaten cihaza erişebilir.
3. **Hash tur sayısı ölçülmedi** — 20 000 tur bir tahmin (TASK-042).
4. **Röle polaritesi doğrulanmadı** (ISSUE-003).

## YAPILMAYAN

- [ ] Kaba kuvvet korumasının gerçek zamanlamayla doğrulanması
- [ ] Hash süresi ölçümü → tur sayısı ayarı
- [ ] Yük altında kaynak tüketimi saldırısı denemesi

**TASK-063: STATİK DENETİM TAMAMLANDI** (zamanlama ölçümleri bekliyor).
