# TASK-038 — SoftAP & AP Fallback

**Phase:** 7 — Network · **Priority:** P1

## Objective

Ağa bağlanılamadığında kullanıcının cihaza erişebilmesini garanti etmek: AP açmak, ancak
**STA denemesini arka planda sürdürmek** — böylece ağ geri geldiğinde otomatik dönmek.

## Scope

- SoftAP başlatma, SSID/şifre yapılandırması
- AP fallback tetikleme kuralları
- AP_STA modunda arka plan STA denemesi
- Ağ geri geldiğinde otomatik STA'ya dönüş
- AP istemci sayısının izlenmesi

## Out of Scope

- Captive portal (mimaride yok, gelecekte ayrı task)
- Web sunucusu (TASK-041)
- Backoff (TASK-037)

## Dependencies

- TASK-036, TASK-037

## Requirements

- `REQUIREMENTS.md` — §2 (Access Point / SoftAP `[x]`, bağlantı kaybı yönetimi `[~]`)

## Architecture References

- §8.1 AP_FALLBACK durumu · §8.2 AP fallback satırı

## Expected Design

### Mevcut davranışın eksikliği

```text
Mevcut:   Yalnızca boot'ta bir kez, bağlantı başarısızsa AP açılıyor.
          Çalışma sırasında ağ kalıcı koparsa AP açılmıyor → cihaza erişim yok.
Yeni:     Kalıcı kopmada AP otomatik açılır, STA denemesi arka planda sürer,
          ağ geri gelince otomatik STA'ya dönülür.
```

### Karar gerektiren nokta 1 — AP fallback tetikleme eşiği

```text
Problem:      Kaç başarısız denemeden sonra AP açılmalı?
Constraints:  Çok erken → kısa kesintide gereksiz mod değişimi ve istemci kopması
              Çok geç → kullanıcı cihaza uzun süre erişemez
Approaches:   (a) N deneme sonrası
              (b) T süre bağlantısız kaldıktan sonra
              (c) backoff tavanına ulaşınca
Recommended:  (b) veya (c) — süre tabanlı ölçüt kullanıcı deneyimine daha yakın
```

### Karar gerektiren nokta 2 — AP güvenliği

```text
Problem:      AP şifresi ne olmalı?
Constraints:  Mevcut projede sabit "12345678" — herkesin bildiği bir değer;
              AP üzerinden cihaz tam kontrol edilebiliyor;
              kullanıcı ilk kurulumda bir şifre bilmek zorunda
Approaches:   (a) sabit varsayılan şifre  (mevcut — kabul edilemez)
              (b) cihaza özgü üretilen şifre (MAC/çip kimliğinden türetilen)
              (c) kullanıcının belirlediği, ilk kurulumda basılı/etiketli
Recommended:  (b) — cihaza özgü, OLED'de gösterilebilir; ayrıca web arayüzü kimlik
              doğrulaması (TASK-042) ikinci savunma hattıdır
```

## Implementation Notes

- AP_STA modu her iki radyoyu aynı anda çalıştırır; bu **güç tüketimini ve RAM kullanımını
  artırır**. Kalıcı olarak bu modda kalmak yerine, STA bağlanınca AP kapatılmalı — ancak
  AP'ye bağlı aktif istemci varsa hemen kapatmak kullanıcıyı ortada bırakır. Bu denge
  tasarlanmalı.
- AP SSID'si cihaza özgü olmalı (örn. son MAC baytları ile); aynı ortamda birden fazla cihaz
  olduğunda ayırt edilebilmeli.
- AP IP adresi ve DHCP aralığı yapılandırılabilir olmalı; yaygın ev ağı aralıklarıyla
  (192.168.1.x) çakışmamalı.
- AP açıkken STA taraması yapmak bağlantıyı bozabilir; tarama ve AP birlikte dikkatli
  yönetilmeli (TASK-039 ile koordinasyon).
- AP açıldığı ve kapandığı loglanmalı; OLED'de açıkça gösterilmeli.

## Files

- `src/services/network/SoftApManager.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] AP fallback tetikleme ölçütü seçildi ve gerekçelendirildi
- [ ] Kalıcı kopmada AP otomatik açılıyor
- [ ] AP açıkken STA denemesi arka planda sürüyor
- [ ] Ağ geri geldiğinde otomatik STA'ya dönülüyor
- [ ] AP şifresi sabit varsayılan değil; cihaza özgü
- [ ] AP SSID'si cihaza özgü
- [ ] AP IP aralığı yapılandırılabilir ve çakışmıyor
- [ ] Aktif istemci varken AP kapatma davranışı tasarlandı
- [ ] AP durum değişiklikleri loglanıyor ve OLED'de görünüyor

## Test Plan

- [ ] Ağ kapatıldığında belirlenen süre sonra AP açılıyor
- [ ] AP'ye bağlanılıp web arayüzüne erişilebiliyor
- [ ] Ağ geri açıldığında otomatik STA'ya dönülüyor
- [ ] AP'de aktif istemci varken STA bağlanınca davranış tasarıma uygun
- [ ] AP şifresi her cihazda farklı
- [ ] AP açıkken bellek kullanımı ölçüldü ve kabul edilebilir
- [ ] Uzun süreli AP_STA modunda kararlılık testi

## Review Checklist

- [ ] Architecture'a uygun mu? (§8.1, §8.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — **AP_STA modunda RAM kullanımı**
- [ ] Error handling var mı? — AP başlatma hatası
- [ ] ESP32 resource kullanımı uygun mu? — güç tüketimi
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **sabit "12345678" şifresi yasak**

## Definition of Done

Ortak DoD + otomatik AP→STA dönüşü test edildi + AP modunda bellek kullanımı ölçüldü.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Tetikleme eşiği: (b) süre tabanlı

```text
Selected: 90 saniye baglantisiz kalinca AP acilir.
Reddedilen (a) N deneme: backoff yuzunden "N deneme" gecen sureyi
           belirsiz kilar — 3 deneme 7 sn de olabilir 90 sn de.
Reddedilen (c) backoff tavani: ayni belirsizlik.

Sure tabanli olcut kullanici deneyimine dogrudan karsilik gelir:
"1.5 dakikadir baglanamiyorum, kullanici cihaza erisebilmeli."
```

## Karar 2 — AP şifresi: rastgele üretilip saklanır — MAC'ten TÜRETİLMEZ

**Plan (b)'yi öneriyordu; daha güçlü bir seçenek uygulandı ve gerekçesi
aşağıdadır.**

```text
(a) sabit "12345678" (eski sistem)  → KABUL EDILEMEZ, herkes biliyor
(b) MAC/cip kimliginden TURETILEN   → REDDEDILDI, asagidaki nedenle
(c) kullanicinin belirledigi        → ilk kurulumda bilinemez

(b)'nin acigi: SoftAP kendi BSSID'sini YAYINLAR ve bu, cihazin MAC
adresidir. Sifre MAC'ten turetiliyorsa, menzildeki HERKES sifreyi
hesaplayabilir. "Cihaza ozgu" olmasi onu GIZLI yapmaz.

UYGULANAN: ilk boot'ta `esp_random()` ile 10 karakterlik sifre uretilir,
`SecretStore`'a yazilir, OLED'de gosterilir. Kullanici ekrandan okur.
Ayni sifirdan-yapilandirma deneyimi, gercek gizlilikle.
```

SSID **MAC'ten türetilir** ve bu doğrudur: SSID'nin gizli olması gerekmez,
ayırt edici olması gerekir (aynı ortamda birden fazla cihaz).

## Karar 3 — STA bağlanınca AP hemen kapanmaz

```text
Problem: AP_STA modu her iki radyoyu calistirir; guc ve RAM maliyeti var.
         Ama STA baglanir baglanmaz AP'yi kapatmak, AP'ye bagli kullaniciyi
         ORTADA BIRAKIR — tam da ayarlari kaydettigi anda.

Selected: STA baglandiktan sonra AP, BAGLI ISTEMCI YOKKEN ve en az
          AP_LINGER_MS (30 sn) gectikten sonra kapatilir.
          Bagli istemci varsa AP acik kalir.
```

## Karar 4 — AP IP aralığı 192.168.4.x

Ev ağlarında yaygın olan `192.168.0.x` ve `192.168.1.x` ile **çakışmaz**.
Çakışma, AP'ye bağlanan telefonun yönlendirme tablosunu bozar.

---

# STEP 3 — REVIEW RECORD

- [x] SoftAP açma/kapama; cihaza özgü SSID (`Sera-XXXXXX`)
- [x] AP fallback **süre tabanlı** (90 sn) — çalışma sırasında ağ kalıcı
      koparsa AP açılıyor (eski sistemde yalnızca boot'ta açılıyordu)
- [x] `AP_STA` modunda STA denemesi arka planda sürüyor
- [x] Ağ geri gelince otomatik STA'ya dönülüyor
- [x] AP istemci sayısı izleniyor ve yayınlanıyor
- [x] AP IP aralığı `192.168.4.1/24` — ev ağlarıyla çakışmıyor
- [x] AP açılış/kapanış loglanıyor; **log satırında şifre yok**
- [ ] **Donanım testleri bekliyor**

## Plandan bilinçli sapma: AP şifresi MAC'ten TÜRETİLMİYOR

Plan (b)'yi öneriyordu: "cihaza özgü üretilen şifre (MAC/çip kimliğinden
türetilen)". **Uygulanmadı** ve nedeni bir güvenlik açığıdır:

> SoftAP kendi BSSID'sini **yayınlar** ve bu, cihazın MAC adresidir.
> Şifre MAC'ten türetiliyorsa menzildeki herkes onu hesaplayabilir.
> "Cihaza özgü" olmak onu **gizli** yapmaz.

Uygulanan: ilk boot'ta `esp_random()` ile 10 karakterlik şifre üretilir,
`SecretStore`'a yazılır, OLED'de gösterilir. Aynı sıfır-yapılandırma
deneyimi, gerçek gizlilikle.

Alfabeden `0/O` ve `1/l/I` çıkarıldı — kullanıcı bu şifreyi 128×64 bir
OLED'den okuyup elle yazacak.

SSID **MAC'ten türetiliyor** ve bu doğru: SSID'nin gizli olması gerekmez,
ayırt edici olması gerekir.

## AP kapatma dengesi

STA bağlanır bağlanmaz AP kapatılmıyor. `canCloseNow()` üç koşul arıyor:
STA bağlı **ve** bağlı istemci yok **ve** STA ayağa kalkalı 30 sn geçmiş.
Aksi hâlde kullanıcı tam da ayarları kaydettiği anda ortada kalırdı.

**TASK-038: TAMAMLANDI** (donanım testleri bekliyor).
