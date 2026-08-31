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
constexpr uint8_t UI_SENSORS = 6;
constexpr uint8_t UI_ACTS    = 2;   ///< OLED'de yalnızca gerçek aktüatörler

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
    COUNT     = 7,
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

    // --- Navigasyon ---
    ScreenId screen;
    uint8_t  cursor;         ///< ekran içi seçim
    uint8_t  editing;        ///< 1 = onay bekleniyor
    uint8_t  reserved;
};

} // namespace ui
} // namespace interfaces
