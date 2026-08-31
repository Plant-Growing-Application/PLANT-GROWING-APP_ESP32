#pragma once

// Darbe sayıcı — TASK-019
//
// DONANIMSAL PCNT BİRİMİ kullanılır, yazılım ISR'si DEĞİL.
//
// NEDEN: akış doğrulaması bir GÜVENLİK kararı üretir (kuru çalışma tespiti,
// TASK-031). Sayımın güvenilir olması gerekir.
//
//   ISR yöntemi (mevcut sistemin yöntemi):
//     · her darbede kesme → CPU yükü ve jitter
//     · yoğun Wi-Fi trafiğinde darbe kaçırma riski
//     · okuma ile sıfırlama arasında yarış durumu
//   PCNT:
//     · donanım sayar, CPU karışmaz
//     · kayıp darbe olmaz
//
// ── KRİTİK: OKUMA SAYIMLA BİRLİKTE GEÇEN SÜREYİ DE DÖNDÜRÜR ────────────────
// Mevcut sistemin en büyük hatası buydu: `(pulses * 100) / 450` hesabı SABİT
// bir zaman penceresi varsayıyordu, ama fonksiyon 500 ms ve 600 ms periyotlarla
// çağrılıyordu — sonuç anlamsızdı.
//
// Süre bilgisi olmadan darbe sayısına "debi" denemez.
// ────────────────────────────────────────────────────────────────────────────
//
// TEK OKUYUCU: sayaca yalnızca `io_sense` task'ı erişir. Mevcut sistemde iki
// farklı çağıran aynı sayacı tüketiyordu; biri okuyunca diğerinin verisi
// sıfırlanıyordu.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Result.h"
#include "core/Time.h"

namespace hal {

/// Bir zaman penceresindeki darbe sayımı.
struct PulseWindow
{
    uint32_t       pulses;   ///< pencerede sayılan darbe
    core::Duration elapsed;  ///< pencerenin GERÇEK süresi
    bool           overflow; ///< donanım sayacı taştı (sayım güvenilmez)
};

namespace pulse {

/// PCNT birimini yapılandırır.
///
/// @param pin        darbe girişi
/// @param filterNs   donanımsal gürültü filtresi. Bu süreden kısa darbeler
///                   elenir. Çok agresif değer GERÇEK darbeleri de eler —
///                   sensörün minimum darbe genişliğine göre seçilmelidir.
core::ErrCode begin(uint8_t pin, uint16_t filterNs = 1000);

/// Sayacı okur ve sıfırlar; **sayım ile geçen süreyi birlikte** döndürür.
///
/// Okuma ve sıfırlama, aradaki darbe kaybını önleyecek şekilde yapılır.
///
/// Yalnızca `io_sense` task'ından çağrılmalıdır (tek okuyucu kuralı).
core::Result<PulseWindow> readAndReset();

/// Sayacı sıfırlamadan okur (teşhis için).
uint32_t peek();

/// Donanım sayacı sınırı.
///
/// PCNT 16 bit işaretlidir. YF-S401 sınıfı bir sensör 10 L/dk'da yaklaşık
/// 1 kHz üretir; 1 sn'lik pencerede ~1000 darbe olur — sınırın çok altında.
/// Yine de taşma tespit edilir ve `overflow` ile bildirilir.
constexpr int16_t COUNTER_LIMIT = 32000;

} // namespace pulse
} // namespace hal
