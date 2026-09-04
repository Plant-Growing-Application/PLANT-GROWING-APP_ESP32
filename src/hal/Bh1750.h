#pragma once

// BH1750 ortam ışığı (lüks) sensörü sürücüsü — TASK-066
//
// ── NEDEN IŞIK ÖLÇÜYORUZ ────────────────────────────────────────────────────
// Meyveli ürünler (çilek, domates, biber) ışığa doymaz: domates günde 14–18
// saat yeterli şiddette ışık ister. Büyütme ışığı rölesi bir ÇİZELGEYLE
// sürülür, ama çizelgenin gerçekten işe yarayıp yaramadığı ancak ölçümle
// bilinir — lamba yanmıyorsa, perde kapalıysa veya balast bozulduysa çizelge
// "ışık verdim" der, bitki aksini söyler.
//
// Bu sensör KARAR VERMEZ, ölçer. Işık kuralı çizelgedir; lüks değeri
// kullanıcıya "ışığın gerçekten yanıyor mu" sorusunun cevabını verir.
//
// ── NEDEN BLOKLAMA YOK ──────────────────────────────────────────────────────
// AHT20'den farklı olarak BH1750 SÜREKLİ MODDA çalıştırılır: çip kendi
// ritminde (~120 ms) ölçer ve sonucu bir yazmaçta tutar. Okuma iki bayttır ve
// beklemez — durum makinesi gerekmez.

#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {
namespace bh1750 {

/// I2C adresi. ADDR pini GND'ye çekiliyken 0x23, VCC'ye çekiliyken 0x5C.
/// Modüllerin varsayılanı 0x23'tür.
constexpr uint8_t I2C_ADDRESS = 0x23;

/// Bu kadar ardışık hatadan sonra çip kullanılamaz sayılır.
constexpr uint16_t ERROR_LIMIT = 10;

/// Sürekli yüksek çözünürlük modunda ilk ölçümün hazır olma süresi (ms).
///
/// Veri sayfası tipik 120 ms verir; pay bırakılmıştır. Bu süre dolmadan
/// okunan yazmaç **0** döner ve arayüzde "0 lüks" olarak görünürdü —
/// karanlık bir seradan ayırt edilemeyen, uydurma bir ölçüm (TASK-072).
constexpr uint32_t FIRST_MEASUREMENT_MS = 200;

/// Çipi açar ve sürekli yüksek çözünürlük moduna alır.
core::ErrCode begin();

/// Son ölçümü okur.
///
/// @param outLux ölçüm (lüks)
/// @return false = okuma başarısız; `outLux` DEĞİŞTİRİLMEZ
bool read(float& outLux);

/// Çip kullanılabilir durumda mı?
bool isAvailable();

/// Biriken ardışık hata sayısı — teşhis.
uint16_t errorCount();

} // namespace bh1750
} // namespace hal
