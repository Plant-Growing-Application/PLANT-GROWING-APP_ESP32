#pragma once

// I2C hattının tek sahibi — TASK-066
//
// ── NEDEN AYRI BİR MODÜL ────────────────────────────────────────────────────
// Hat eskiden `OledPanel::begin()` içinde açılıyordu ve tek kullanıcısı vardı.
// TASK-066 ile aynı hatta iki ortam sensörü daha bağlandı (AHT20, BH1750);
// hattın kurulumu artık TEK BİR cihazın başlatma yolunda saklı olamaz:
//
//   · OLED takılı değilse `OledPanel::begin()` erken döner — ama Wire yine de
//     kurulmuş olmalıdır, yoksa sensörler sessizce okunamaz hâle gelirdi
//   · Boot aşama sırası (DISPLAY_HW → SENSOR_HW) bir kurulum bağımlılığını
//     taşıyamaz; aşamalar birbirinin ön koşulu DEĞİLDİR (ARCHITECTURE P4)
//
// Bu modül ARCHITECTURE P2'nin (donanıma tek kapı) hat düzeyindeki
// karşılığıdır: `Wire.begin()` çağrısı projede YALNIZCA burada bulunur.
//
// ── EŞ ZAMANLILIK ───────────────────────────────────────────────────────────
// Hattı iki task paylaşır: `ui` (OLED) ve `io_sense` (ortam sensörleri).
// Arduino `TwoWire` sınıfı kendi mutex'ini taşır (Wire.cpp, `lock`), bu yüzden
// eş zamanlı işlemler SERİLEŞTİRİLİR — yarış yoktur.
//
// Bekleme `portMAX_DELAY`'dir; süresi karşı tarafın işlem uzunluğuyla
// sınırlıdır. Bu yüzden I2C işlemleri KISA tutulur: sensör sürücüleri ölçüm
// bitmesini beklemez, durum makinesiyle ilerler (bkz. `Aht20.h`).

#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {
namespace i2cbus {

/// Hattı kurar. **Birden çok kez çağrılabilir** — ilk çağrıdan sonrakiler
/// işlemsizdir. Her kullanıcı kendi `begin()`'inde çağırır ve hangi modülün
/// önce başladığını bilmek zorunda kalmaz.
core::ErrCode begin();

/// Hat kuruldu mu?
bool isReady();

} // namespace i2cbus
} // namespace hal
