# TASK-076 — Bağlantı Durumu Ekrana Yansımıyor + AP'ye Düşme Süresi

Kullanıcı bildirimi:

> *"İlk defa yüklüyorum, interneti bağladığımda küçük ekran yenilemeden
> internetin bağlandığını tam olarak göstermiyor, illa ben resetleyeceğim."*
>
> *"İnternete bağlıyorum sonra interneti kapatıyorum, server moda yaklaşık
> 1 dk sonra geçiyor. Bunun süresini 5 saniye yapalım ama arkada da arasın."*

## Bölüm 1 — Ekran, ağ durumu hakkında YALAN SÖYLÜYORDU

Ağ katmanı doğru çalışıyordu: `NetworkFsm` bağlantıyı kuruyor, `publish()`
saniyede bir `StateStore`'a yazıyor, `ViewModelBuilder` IP'yi modele koyuyor.
Sorun **sunum** katmanındaydı — üç ayrı yerde.

### 1.1 Kurulum ekranı orada KALIYORDU

`UiService` kurulum ekranını ilk açılışta bir kez açar ve bir daha
dokunmazdı. Kullanıcı kurulumu **telefonundan** yapıyor, yani encoder'a hiç
dokunmuyor. Cihaz ev ağına bağlandıktan sonra bile OLED kurulum ağını ve
şifresini göstermeye devam ediyordu.

Kontrollü yeniden başlatma bunu normalde örtüyordu (cihaz zaten yeniden
başlıyor). Ama yazma tamamlanmazsa yeniden başlatma **iptal edilir**
(`SETUP_REBOOT_MAX_WAIT_MS`) — ve o zaman ekran kurulum bilgisiyle asılı
kalıyordu. Kullanıcının gördüğü: cihaz ev ağında ama ekran hâlâ "KURULUM".

**Düzeltme:** `nav::onSetupFinished()` — bağlantı kurulduğunda ve bekleyen bir
yeniden başlatma yokken kurulum ekranından `HOME`'a çıkılır. Kullanıcı o sırada
kendisi başka bir sayfaya geçmişse ekran ELİNDEN ALINMAZ.

### 1.2 `HOME`, AP açık diye "KURULUM MODU" diyordu

```cpp
if (m.apActive != 0u && m.apSsid[0] != '\0')   // ← eksik koşul
```

`apActive` tek başına yetmez: cihaz ev ağına bağlandıktan sonra kurulum AP'si
`LINGER_MS`–`HARD_LINGER_MS` (30–90 sn) boyunca **açık kalır**. O pencerede
`HOME` bağlantı özetini değil, kurulum şifresini gösteriyordu.

**Düzeltme:** koşula `m.staConnected == 0u` eklendi. Bağlıyken doğru bilgi
özet ve **IP adresidir**.

### 1.3 Kurulum ekranı "bağlandı"yı `setupReboot`'a bağlamıştı

Ekran, bağlantı bilgisini yalnızca *yeniden başlatma beklerken* gösteriyordu.
Yeniden başlatma iptal edilince `setupReboot` sıfıra dönüyor ve ekran **eski
kurulum şifresine geri dönüyordu**.

**Düzeltme:** koşul artık "bağlandı"dır. Yeniden başlatma bekleniyorsa alt
satır "Yeniden baslatiliyor", değilse "Kurulum tamamlandi" der.

### 1.4 `UiModel::staConnected`

Üç düzeltmenin de dayandığı alan. `apActive` ile **birlikte** okunmak
zorundadır: ikisi aynı anda 1 olabilir ve ekranın hangi bilgiyi göstereceği
buna bağlıdır.

### 1.5 OLED'e giden ASCII olmayan tek dize

`NetState::BOOT` için `"baslıyor"` yazıyordu. Yerleşik 5×7 font ASCII'dir;
`ı` (U+0131) iki bayt olarak iki bozuk glife dönüşüp satırı kaydırıyordu.

## Bölüm 2 — AP'ye düşme süresi 45 sn → 5 sn

```
softap::FALLBACK_AFTER_MS   45000 → 5000
```

Eşik 90 sn ile başlamış, 45 sn'ye çekilmiş, hâlâ uzundu. Kullanıcı açısından
anlamı: internet gittiğinde cihaza **ulaşılamayan bir dakika**.

**Erken açmanın maliyeti yok.** Kurtarma AP'si STA denemesini durdurmaz:

```
AP_FALLBACK durumu → radyo AP_STA
                   → conn::beginConnect() backoff takvimiyle sürüyor
                   → ağ dönünce STA_GOT_IP → CONNECTED
                   → AP linger süresiyle kendiliğinden kapanır
```

Yani AP yalnızca **erişilebilirlik ekler**; bağlantının geri gelmesini
geciktirmez. Kullanıcının istediği "arkada da arasın" davranışı zaten vardı,
tek eksik erken açılmaktı.

Arka plan denemesi backoff takvimini kullanır: 1 → 2 → 4 → 8 → 16 → 20 sn
(tavan), ±%20 jitter. Yani ağ döndüğünde en kötü ihtimalle 20 saniyede
bağlanılır.

### Sayaç, kopma BİLDİRİLDİKTEN sonra başlar

Eşiği 5 saniyeye çekmek tek başına yetmez. Yönlendirici kapandığında istasyon
beacon'ları kaçırmaya başlar ve sürücü kopmayı ancak **inactive time** dolunca
bildirir — varsayılanı 6 saniyedir. Kullanıcının kronometresi şunu ölçüyordu:

```
6 sn (kopma fark edilir)  +  5 sn (fallback eşiği)  =  11 sn
```

`esp_wifi_set_inactive_time(WIFI_IF_STA, 3)` ile ilk terim ESP-IDF'in izin
verdiği alt sınıra çekildi; toplam ~8 saniye. Daha kısası sürücüde yok.

Ayar başarısız olursa **sistem çalışmaya devam eder**: varsayılan 6 saniyeyle
AP biraz geç açılır, log'a uyarı düşer.

### Yan etki: açılıp kapanma titremesi

Yönlendirici yeniden başlatması gibi 20–40 saniyelik kopmalarda AP artık
açılıp kapanacaktır. Bu, "kapalı kalıp erişilemez olmaktan" iyidir ve
`LINGER_MS` (30 sn) titremeyi zaten damperler: AP, STA bağlandıktan sonra
en az bu kadar açık kalır ve bağlı istemci varsa daha da uzun.

## Dokunulan dosyalar

```
src/services/network/SoftApManager.h    FALLBACK_AFTER_MS 45000 → 5000
src/hal/WifiRadio.cpp                   sta inactive time 6 → 3 sn
src/services/network/NetworkFsm.h       FSM şeması: "45 sn" → "5 sn"
src/interfaces/ui/ViewModels.h          UiModel::staConnected
src/interfaces/ui/ViewModelBuilder.cpp  staConnected doldurma, "baslıyor" → "basliyor"
src/interfaces/ui/Navigation.{h,cpp}    onSetupFinished()
src/interfaces/ui/UiService.cpp         bağlanınca kurulum ekranından çıkış
src/interfaces/ui/screens/Screens.cpp   HOME ve SETUP ekranlarının koşulları
ARCHITECTURE.md                         §8 AP fallback satırı
```

## Definition of Done

- [x] Bağlantı kurulduğunda ekran kurulum bilgisini göstermeyi BIRAKIYOR
- [x] AP linger'ı sürerken `HOME` "KURULUM MODU" demiyor
- [x] Yeniden başlatma iptal edilse bile ekran "Baglandi + adres" gösteriyor
- [x] Kopmadan 5 sn sonra AP açılıyor, STA denemesi arka planda sürüyor
- [x] Kopmanın fark edilme süresi 6 → 3 sn (toplam ~8 sn)
- [ ] **`pio run` KOŞULMADI** — bu makinede derleyici yok (ISSUE-033)
- [ ] Donanımda doğrulama: `docs/HARDWARE_TEST_PROCEDURE.md` §1.8
