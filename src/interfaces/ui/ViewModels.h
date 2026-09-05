#pragma once

// OLED görünüm modeli — TASK-050
//
// ── TEK BİRLEŞİK MODEL ──────────────────────────────────────────────────────
// Ekran başına ayrı tip 7 × (tip + dönüştürücü + karşılaştırıcı) = 21 birim
// kod demekti. 128×64 bir ekranın gösterdiği veri kümesi zaten küçük ve
// ekranlar arasında büyük ölçüde ORTAK (durum çubuğu her ekranda aynı).
//
// ── POD, SABİT TAMPON, HEAP YOK ─────────────────────────────────────────────
// `String` kullanılmaz. `memcmp` ile karşılaştırılabilir olması kirli alan
// tespitinin temelidir: aynı model → ÇİZME (I2C yükü ve titreme önlenir).
//
// ── ŞİFRE YOK ───────────────────────────────────────────────────────────────
// Wi-Fi şifresi için bu yapıda ALAN YOKTUR; sızması için önce birinin alan
// eklemesi gerekir. Eski projede WIFI sayfası şifreyi açıkça yazıyordu.
//
// `apPassword` bir istisnadır ve gösterilmek ZORUNDADIR: cihazın kendi
// ürettiği KURULUM şifresidir (TASK-038) ve kullanıcı ona bağlanmak için
// ekrandan okur. Kullanıcının ev ağı şifresi değildir.

#include <stdint.h>

#include "core/SystemState.h"

namespace interfaces {
namespace ui {

constexpr uint8_t TEXT_MAX  = 22;   ///< 128 px / 6 px ≈ 21 karakter + sonlandırıcı

/// Modelde taşınan sensör satırı sayısı — **sistemdeki tüm sensörler**.
///
/// 6 idi ve `MAX_SENSORS` 8'e çıkınca (TASK-066) nem ile ışık OLED'den
/// sessizce düşmüştü: taşma yoktu (döngüler sınırlıydı) ama iki ölçüm ekranda
/// hiç görünmüyor ve kimse nedenini bilmiyordu (ISSUE-036).
constexpr uint8_t UI_SENSORS = core::MAX_SENSORS;

/// Modelde taşınan aktüatör satırı sayısı — **tüm röleler**.
///
/// 2 idi ve `ViewModelBuilder` su/hava pompası dışındakileri filtreliyordu.
/// O filtre `AUX_1`/`AUX_2` fiziksel pini olmadığı için doğruydu; TASK-066
/// ile beşinin de pini var ve büyütme ışığı, ısıtıcı ve dozaj pompası OLED'de
/// ne görülebiliyor ne kontrol edilebiliyordu.
constexpr uint8_t UI_ACTS    = core::MAX_ACTUATORS;

/// Bir ekranda aynı anda çizilebilen satır sayısı (128×64, durum çubuğu hariç).
///
/// Sensör ve kontrol ekranları bundan fazla öğe taşır; ikisi de **kayan
/// pencere** kullanır (bkz. `Screens.cpp`).
constexpr uint8_t UI_VISIBLE_ROWS = 5;

/// Ekran kimliği. `EMERGENCY` öncelikli ekrandır.
///
/// Makro çakışma taraması yapıldı: `HOME`, `SENSORS`, `CONTROL`, `NETWORK`,
/// `SYSTEM`, `ALERTS`, `EMERGENCY` — Arduino/ESP-IDF başlıklarında karşılığı
/// yok (ISSUE-009 kuralı).
enum class ScreenId : uint8_t
{
    HOME      = 0,
    SENSORS   = 1,
    CONTROL   = 2,
    NETWORK   = 3,
    SYSTEM    = 4,
    ALERTS    = 5,
    EMERGENCY = 6,

    /// Ürün seçimi ve programı başlatma — telefonsuz kurulumun ikinci yarısı.
    CROP      = 7,

    /// İlk açılış: kurulum ağının adı, şifresi ve açılacak adres.
    ///
    /// `EMERGENCY` gibi gezinme sırasında YOKTUR: cihaz kurulmamışken bir
    /// kez kendiliğinden açılır, kullanıcı encoder'ı çevirince çıkılır ve
    /// bir daha kendiliğinden gelmez. Aynı bilgiler `NETWORK` ekranında
    /// kalıcı olarak durur.
    SETUP     = 8,

    COUNT     = 9,
};

/// Sayfa göstergesinde yeri olmayan ekran (`EMERGENCY`, `SETUP`).
constexpr uint8_t UI_NO_PAGE = 0xFFu;

/// Navigasyonun ekrana bildirdiği her şey — TASK-075.
///
/// Ayrı ayrı yedi parametre yerine tek yapı: `build()` çağrısında `bool`
/// sırası karışırsa derleyici bunu YAKALAMAZ, ekran sessizce yanlış modda
/// çizilirdi.
struct NavState
{
    ScreenId screen;
    uint8_t  cursor;
    uint8_t  focus;       ///< 1 = sayfanın İÇİNDEYİZ (öğe gezinme)
    uint8_t  confirming;  ///< 1 = onay bekleniyor
    uint8_t  pageIndex;   ///< sıradaki konum; `UI_NO_PAGE` = sırada değil
    uint8_t  pageCount;   ///< gezinilebilir sayfa sayısı
    uint8_t  enterable;   ///< 1 = bu sayfanın girilebilir içeriği var
};

/// Bir sensör satırı — biçimlendirilmiş, çizime hazır.
struct SensorLine
{
    char    label[10];
    char    value[12];   ///< kalite OK değilse "—" / "yok" / "12.3!"
    uint8_t degraded;    ///< 1 = değer güvenilmez, vurgulanmalı
    uint8_t present;     ///< 0 = bu satır hiç çizilmez
};

struct ActuatorLine
{
    char    label[12];
    uint8_t on;
    uint8_t blocked;     ///< 1 = güvenlik/kısıt engeli var
    char    why[TEXT_MAX];
};

/// Katalogda taşınabilecek en fazla ürün.
///
/// `core::cropCount()` bugün 6 döndürüyor. Sabit burada, katalogda büyüme
/// olursa taşma yerine KIRPILMA olsun diye: model sabit boyutlu bir POD ve
/// `memcmp` ile karşılaştırılıyor.
constexpr uint8_t UI_CROPS = 8;

/// Ürün seçimi ekranındaki bir satır.
struct CropLine
{
    char    name[12];
    uint8_t active;      ///< 1 = şu an uygulanmış profil
};

/// Tüm ekranların okuduğu model.
struct UiModel
{
    // --- Durum çubuğu (her ekranda ortak) ---
    char    clock[8];        ///< "13:45" veya "--:--" (SAHTE "00:00" DEĞİL)
    char    modeText[10];
    int8_t  rssi;
    uint8_t wifiBars;        ///< 0–4; 0 = bağlı değil
    uint8_t apActive;
    uint8_t faultCount;
    uint8_t emergency;

    // --- İçerik ---
    SensorLine   sensors[UI_SENSORS];
    ActuatorLine actuators[UI_ACTS];

    char ssid[TEXT_MAX];
    char ip[16];
    char apSsid[TEXT_MAX];
    char apPassword[16];     ///< KURULUM şifresi — gösterilmek zorunda
    char netState[16];
    char uptime[12];

    char alertText[TEXT_MAX];    ///< en ciddi aktif hata
    char emergencyWhy[TEXT_MAX];

    // --- Ürün ve program (telefonsuz kurulum) ---
    CropLine crops[UI_CROPS];
    uint8_t  cropCount;
    uint8_t  cropSelected;       ///< 1 = bir profil uygulanmış

    /// "Salatalik Ciceklenme" — en uzun olası birleşim 20 karakter (120 px),
    /// 128 px'lik satıra tam oturur. `TEXT_MAX` (22) burada YETMEZ: ayıraçla
    /// birlikte 22 karakteri aşıp hem tamponda kırpılır hem ekrandan taşardı.
    char     cropText[24];
    uint8_t  autoMode;           ///< 1 = otomasyon AÇIK (program yürüyor)
    char     setupUrl[16];       ///< kurulum modunda tarayıcıya yazılacak adres

    // --- Navigasyon ---
    ScreenId screen;
    uint8_t  cursor;         ///< ekran içi seçim
    uint8_t  editing;        ///< 1 = onay bekleniyor

    /// 1 = sayfanın İÇİNDEYİZ; encoder satırları geziyor.
    ///
    /// Ekran katmanı bunu bilmeden seçim çubuğunu çizemez: sayfa modunda
    /// imleç kullanıcıya AİT DEĞİLDİR (nereye basacağını bilmediği bir satır
    /// vurgulanmış görünürdü).
    uint8_t  navFocus;

    /// Sayfa göstergesi: kaçıncı sayfa / kaç sayfa.
    ///
    /// Ekranların başlığı yok: kullanıcı sayfayı yalnızca içeriğinden
    /// tanıyor, sıranın nerede bittiğini bilmiyordu.
    uint8_t  pageIndex;      ///< `UI_NO_PAGE` = gösterge çizilmez
    uint8_t  pageCount;
    uint8_t  pageEnterable;  ///< 1 = "gir" ipucu gösterilir

    /// Kurulum tamamlandı, cihaz kontrollü biçimde yeniden başlıyor (§8.4).
    ///
    /// Ekranda söylenmesi ZORUNLUDUR: kullanıcı telefonu elinde, kurulum
    /// AP'sinin kaybolmasını izleyecek. Habersiz yeniden başlayan bir cihaz,
    /// bozulmuş bir cihazdan ayırt edilemez.
    uint8_t  setupReboot;
};

} // namespace ui
} // namespace interfaces
