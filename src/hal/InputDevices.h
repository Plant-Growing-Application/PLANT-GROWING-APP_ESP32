#pragma once

// Girdi cihazları: rotary encoder + butonlar — TASK-021
//
// ISR YALNIZCA OLAY ÜRETİR. Debounce yorumlama, detent normalizasyonu ve tüm
// karar mantığı task bağlamındadır (CODING_STANDARDS §6).
//
// GİRDİ OLAYLARI KOMUT KUYRUĞUNA KARIŞMAZ (ARCHITECTURE §5): girdi olayı ham
// donanım olayıdır, komut ise niyet bildirimidir. `ui` task'ı girdiyi alır,
// yorumlar ve gerekiyorsa komut üretir.
//
// DETENT ORANI TAMSAYIDIR. Mevcut sistemde `stepsPerDetent = 1.5` (`double`)
// bir `int` sayaçla karşılaştırılıyordu — tanımsız davranışa yakın bir desen
// (REQUIREMENTS §12).

#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {

/// Ham girdi olayı tipi.
enum class InputEventType : uint8_t
{
    NONE            = 0,
    ENCODER_CW      = 1,  ///< bir detent saat yönünde
    ENCODER_CCW     = 2,  ///< bir detent saat yönünün tersine
    BUTTON_SHORT    = 3,
    BUTTON_LONG     = 4,
};

/// Hangi buton.
enum class ButtonId : uint8_t
{
    ENCODER_PUSH = 0,
    BACK         = 1,
    COUNT        = 2,
};

struct InputEvent
{
    InputEventType type;
    ButtonId       button;  ///< yalnızca BUTTON_* olaylarında anlamlı
    uint8_t        reserved[2];
};

namespace input {

/// Encoder ve butonları yapılandırır, kesmeleri bağlar.
///
/// @param stepsPerDetent bir detent için gereken quadrature adımı.
///                       Tipik encoder'larda 4'tür; **tamsayıdır** ve
///                       sahada doğrulanmalıdır.
core::ErrCode begin(uint8_t stepsPerDetent = 4);

/// Bekleyen bir girdi olayı alır. Bloklamaz.
/// Yalnızca `ui` task'ından çağrılmalıdır.
bool poll(InputEvent& out);

/// Bekleyen olay sayısı.
uint8_t pending();

/// Kuyruk dolduğu için düşürülen olay sayısı.
/// Sıfırdan farklıysa ya `ui` task'ı yavaşlamış ya encoder çok hızlı çevrilmiş.
uint32_t droppedEvents();

/// Uzun basış eşiği. Acil durdurma için kullanılacaksa (ARCHITECTURE §13.3)
/// yanlışlıkla tetiklenmeyecek kadar uzun olmalıdır.
constexpr uint16_t LONG_PRESS_MS = 1200;

/// Buton debounce süresi.
constexpr uint16_t DEBOUNCE_MS = 30;

} // namespace input
} // namespace hal
