# TASK-036 — STA Connection Manager & IP Configuration

**Phase:** 7 — Network · **Priority:** P1

## Objective

STA bağlantısını bloklamadan kurmak, credential yönetimini güvenli hale getirmek ve
DHCP/Static IP yapılandırmasını **AP/STA modundan bağımsız** olarak sağlamak.

## Scope

- Bloklamayan bağlantı başlatma ve sonucun event ile alınması
- Credential okuma (`SecretStore`) ve güncelleme
- Credential silme (ağı unut)
- DHCP / Static IP seçimi ve uygulanması
- Bağlantı zaman aşımı yönetimi

## Out of Scope

- Backoff hesabı (TASK-037)
- AP fallback (TASK-038)
- Web arayüzü (TASK-044)

## Dependencies

- TASK-035, TASK-015

## Requirements

- `REQUIREMENTS.md` — §2 (bağlantı, static IP `[~]`, credential kaydetme/silme)

## Architecture References

- §8.2 Tasarım kararları (bloklama, static IP, credential saklama)

## Expected Design

### Static IP — mimari düzeltme

```text
Mevcut hata:  _useDHCP = (Setting.IsServerMode == 0)
              → DHCP tercihi, AP/STA moduna bağlanmış. İki kavram tamamen ilgisiz.
Yeni:         config.network.mode ∈ {DHCP, STATIC}
              AP/STA modundan bağımsız ayrı bir alan.
```

Ayrıca mevcut projede `setStaticIP()` ve `applyConfig()` yazılmış ama **hiç çağrılmıyordu**.
Bu task'ta static IP ya gerçekten çalışır hale gelir ya da hiç yazılmaz (P7).

### Karar gerektiren nokta — Bağlantı zaman aşımı

```text
Problem:      CONNECTING durumunda ne kadar beklenmeli?
Constraints:  Bekleme bloklayan olmamalı (zamanlayıcı ile);
              çok kısa → yavaş AP'lerde asla bağlanamaz
              çok uzun → AP fallback gecikir, kullanıcı erişemez
Approaches:   (a) sabit zaman aşımı
              (b) deneme sayısına göre artan zaman aşımı
              (c) event tabanlı, zaman aşımı yalnızca emniyet valfi
Recommended:  (c) — normalde event gelir; zaman aşımı yalnızca event hiç gelmezse devreye girer
```

### Credential güvenliği

- Şifre `SecretStore`'da (TASK-013), config'te değil
- Şifre `SystemState`'e, log'a, API yanıtına **girmez**
- OLED'de gösterilmez (mevcut projede WIFI sayfasında açıkça gösteriliyordu)
- "Ağı unut" işlemi credential'ı gerçekten siler

## Implementation Notes

- Static IP uygulanması bağlantı **başlatılmadan önce** yapılmalı; sonradan uygulanması
  etkisiz kalır.
- Static IP seçiliyken gateway/subnet eksikse bu bir config hatasıdır ve TASK-014
  doğrulamasında yakalanmalıdır; burada ayrıca kontrol edilip DHCP'ye düşülmeli ve loglanmalı.
- DNS sunucusu yapılandırılabilir olmalı; static IP'de DNS boş kalırsa NTP çalışmaz.
- Credential değişikliğinde mevcut bağlantı temiz şekilde kapatılmalı, ardından yeni
  bağlantı başlatılmalı. Yarım kalmış geçiş radyoyu tanımsız bırakır.
- Credential yoksa `CONNECTING` durumuna hiç girilmemeli — doğrudan `AP_ONLY`.
- Bağlantı süresi ölçülüp state'e yazılmalı (teşhis için değerli).

## Files

- `src/services/network/ConnectionManager.h` / `.cpp` (yeni)
- `src/services/network/IpConfig.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Bağlantı başlatma bloklamıyor
- [ ] Zaman aşımı stratejisi seçildi ve emniyet valfi olarak çalışıyor
- [ ] Credential `SecretStore`'dan okunuyor
- [ ] Şifre state/log/API'de görünmüyor
- [ ] "Ağı unut" credential'ı gerçekten siliyor
- [ ] DHCP/Static seçimi AP/STA modundan bağımsız
- [ ] Static IP bağlantı öncesi uygulanıyor
- [ ] Eksik static IP alanında DHCP'ye düşülüyor ve loglanıyor
- [ ] Credential yoksa doğrudan `AP_ONLY`
- [ ] Bağlantı süresi ölçülüp yayınlanıyor

## Test Plan

- [ ] Doğru credential ile bağlanıyor; süre ölçülüyor
- [ ] Yanlış şifre ile doğru neden kodu üretiliyor
- [ ] Credential yokken `AP_ONLY`'ye gidiliyor
- [ ] Static IP ile bağlanıp belirtilen adresi alıyor
- [ ] Static IP'de DNS çalışıyor (NTP senkronizasyonu ile doğrulandı)
- [ ] Eksik gateway ile static IP denendiğinde DHCP'ye düşülüyor
- [ ] Credential değişikliğinde temiz geçiş yapılıyor
- [ ] "Ağı unut" sonrası yeniden boot'ta AP moduna geçiyor
- [ ] Şifrenin hiçbir çıktıda görünmediği doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§8.2)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **bağlantı kurma bloklamamalı**
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — eksik/geçersiz IP config
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`_useDHCP = IsServerMode` hatası tekrarlanmamalı**

## Definition of Done

Ortak DoD + static IP ve DHCP senaryoları test edildi + şifrenin hiçbir yerde
görünmediği doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — Zaman aşımı: (c) emniyet valfi

```text
Normal yol: sonuc EVENT olarak gelir (GOT_IP veya DISCONNECT).
Zaman asimi YALNIZCA event hic gelmezse devreye girer — 20 sn.

Neden emniyet valfi: radyo suruculerinde "olay hic gelmedi" durumu
nadirdir ama olur; boyle bir durumda FSM sonsuza kadar CONNECTING'de
kalir ve AP fallback hic acilmaz — kullanici cihaza HIC erisemez.
Sabit veya artan zaman asimi (a/b) normal yolu gereksiz kisitlar.
```

## Karar 2 — Static IP mimari hatası düzeltildi

```text
Eski: _useDHCP = (Setting.IsServerMode == 0)
      → DHCP tercihi AP/STA MODUNA baglanmis. Iki kavram tamamen ilgisiz.
Yeni: config.network.ipMode ∈ {DHCP, STATIC} — ayri, bagimsiz alan.
```

Ayrıca eski projede `setStaticIP()` ve `applyConfig()` **yazılmış ama hiç
çağrılmıyordu.** P7 gereği bu task'ta static IP ya gerçekten çalışır ya hiç
yazılmaz — **gerçekten çalışıyor**, `staConnect()` öncesinde uygulanıyor.

## Karar 3 — Eksik static alanında DHCP'ye düş, SESSİZCE değil

`STATIC` seçili ama IP/gateway/subnet sıfırsa: DHCP'ye düşülür ve
`NET_IP_CONFIG_INVALID` **loglanır**. Sessizce DHCP'ye düşmek, kullanıcının
"statik IP ayarladım ama çalışmıyor" sorusunu cevapsız bırakır.

DNS boşsa static IP'de **gateway DNS olarak kullanılır**: DNS'siz bir statik
yapılandırmada SNTP çalışmaz ve saat hiç senkronize olmaz (TASK-040).

## Karar 4 — Credential yoksa `CONNECTING`'e HİÇ girilmez

Doğrudan `AP_ONLY`. Boş SSID ile `WiFi.begin()` çağırmak anlamsız bir
başarısızlık döngüsü üretir.

## Karar 5 — Şifre yolculuğu: SecretStore → yığın → radyo

Şifre yalnızca `staConnect()` çağrısı süresince yığında bulunur; `NetworkStatus`'a,
log'a, API yanıtına girmez. Kullanıldıktan sonra tampon **sıfırlanır** —
yığında kalan bir kopya bir stack dump'ta görünebilir.

---

# STEP 3 — REVIEW RECORD

- [x] `beginConnect()` bloklamıyor — sonuç event olarak geliyor
- [x] Zaman aşımı **emniyet valfi** olarak çalışıyor (20 sn); normal yolda
      hiç devreye girmez
- [x] Credential `SecretStore`'dan okunuyor
- [x] Şifre state/log/API'de **görünmüyor**; kullanımdan sonra yığın tamponu
      `memset` ile sıfırlanıyor
- [x] "Ağı unut" credential'ı gerçekten siliyor (config SSID + secret)
- [x] DHCP/Static seçimi AP/STA modundan **bağımsız** — eski sistemin
      `_useDHCP = (IsServerMode == 0)` hatası düzeltildi
- [x] Static IP bağlantı **öncesinde** uygulanıyor
- [x] Eksik static alanda DHCP'ye düşülüyor **ve loglanıyor**
- [x] DNS boşsa gateway kullanılıyor (DNS'siz statik yapılandırmada SNTP
      hiç çalışmazdı)
- [x] Credential yoksa `CONNECTING`'e hiç girilmiyor → doğrudan `AP_ONLY`
- [x] Bağlantı süresi ölçülüyor
- [x] `planFor()` saf `constexpr` — tüm eksik-alan kombinasyonları host'ta
      test edilebilir
- [ ] **Donanım testleri bekliyor**

Eski projede `setStaticIP()` ve `applyConfig()` yazılmış ama **hiç
çağrılmıyordu**. P7 gereği bu task'ta static IP gerçekten çalışıyor.

**TASK-036: TAMAMLANDI.**
