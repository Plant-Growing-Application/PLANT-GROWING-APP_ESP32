#pragma once

// Ekran çerçevesi ve düzen sabitleri — TASK-051 / TASK-052
//
// ── EKRANLAR YALNIZCA ÇİZER ─────────────────────────────────────────────────
// İmza bunu YAPISAL olarak garanti eder:
//
//     using DrawFn = void (*)(const UiModel&);
//
// `const` referans, `void` dönüş, başka parametre yok. Bir ekranın komut
// üretmesi, donanıma dokunması veya durum değiştirmesi için önce bu imzanın
// değişmesi gerekir.
//
// Eski `GrowPlant.cpp` sayfa içindeyken encoder çevrilince `StateWifi()`
// çağırıyor, EEPROM'a yazıyor ve `pauseWiFiMonitor()` ile BAŞKA BİR TASK'İ
// askıya alıyordu (REQUIREMENTS §6.3). Bu yol artık kapalı.
//
// ── DÜZEN SABİTLERİ MERKEZİ ─────────────────────────────────────────────────
// Eski projede aynı yazı bir yerde Y=35, başka yerde Y=28'e yazılıyordu ve
// ekran geçişlerinde satırlar kayıyordu. Hiçbir ekran çıplak koordinat
// kullanmaz.

#include <stdint.h>

#include "interfaces/ui/ViewModels.h"

namespace interfaces {
namespace ui {

/// 128×64 mono ekran düzeni. 6×8 piksel varsayılan font.
namespace layout {

constexpr int16_t W          = 128;
constexpr int16_t H          = 64;
constexpr int16_t CHAR_W     = 6;
constexpr int16_t LINE_H     = 10;

constexpr int16_t STATUS_H   = 11;   ///< durum çubuğu yüksekliği
constexpr int16_t STATUS_Y   = 0;
constexpr int16_t SEP_Y      = 12;   ///< ayırıcı çizgi

constexpr int16_t BODY_Y     = 16;   ///< içerik ilk satırı
constexpr int16_t ROW0       = BODY_Y;
constexpr int16_t ROW1       = BODY_Y + LINE_H;
constexpr int16_t ROW2       = BODY_Y + LINE_H * 2;
constexpr int16_t ROW3       = BODY_Y + LINE_H * 3;
constexpr int16_t ROW4       = BODY_Y + LINE_H * 4;

constexpr int16_t COL_LABEL  = 2;
constexpr int16_t COL_VALUE  = 62;   ///< değer sütunu — sağa hizalı bölge başı
constexpr int16_t CURSOR_X   = 0;

}  // namespace layout

/// Ekran çizim fonksiyonu. **Tek yetkisi çizmektir.**
using DrawFn = void (*)(const UiModel&);

/// Kullanıcı eyleminin sonucu — `UiService` bunu komuta çevirir.
///
/// Ekranlar bu tipi ÜRETMEZ; navigasyon üretir. Ekranlar yalnızca çizer.
enum class UiAction : uint8_t
{
    NONE            = 0,
    TOGGLE_ACTUATOR = 1,  ///< `param` = ActuatorId
    EMERGENCY_STOP  = 2,
    EMERGENCY_CLEAR = 3,
    RESTART         = 4,

    /// `param` = KATALOG İNDEKSİ (CropId değil).
    ///
    /// Navigasyon katalogu tanımaz; yalnızca "listedeki kaçıncı satır"
    /// bilgisini taşır. İndeksi kimliğe çevirmek `UiService`'in işidir —
    /// aynı ayrım aktüatörlerde de var ve navigasyonu ürün tablosundan
    /// bağımsız tutuyor.
    APPLY_CROP        = 5,

    /// Sulama programını başlat/durdur (otomasyon modu).
    TOGGLE_AUTOMATION = 6,
};

struct ActionRequest
{
    UiAction action;
    uint8_t  param;
};

// --- Ekran çizicileri (TASK-052) -------------------------------------------

void drawStatusBar(const UiModel& m);   ///< tüm ekranlarda ortak
void drawHome(const UiModel& m);
void drawSensors(const UiModel& m);
void drawControl(const UiModel& m);
void drawNetwork(const UiModel& m);
void drawSystem(const UiModel& m);
void drawAlerts(const UiModel& m);
void drawEmergency(const UiModel& m);
void drawCrop(const UiModel& m);
void drawSetup(const UiModel& m);

/// Bir ekranı tam olarak çizer (durum çubuğu + gövde) ve panele gönderir.
void renderScreen(const UiModel& m);

} // namespace ui
} // namespace interfaces
