#pragma once

// Rotary encoder quadrature çözücüsü — TASK-071
//
// ── NEDEN core/ İÇİNDE VE SAF ───────────────────────────────────────────────
// Çözücü eskiden `hal/InputDevices.cpp` içindeki ISR'nin gövdesindeydi ve
// **test edilemiyordu**: "ileri gidip geri döndüğümde ilk tık kayboluyor"
// hatası ancak elde bir encoder çevirerek görülebiliyordu ve tekrarlanabilir
// değildi.
//
// Burası donanım görmez: girdi iki pinin okunmuş hâli, çıktı bir detent
// olayı. `pio test -e native` yön değiştirme senaryosunu sentetik olarak
// koşturabilir.
//
// ══ ÇÖZÜLEN HATA: YÖN DEĞİŞİMİNDE KAYBOLAN İLK TIK ═════════════════════════
//
// Belirti: ileri doğru sürekli çevirirken sorun yok; ileri gidip **geri**
// dönüldüğünde ilk detent hiçbir şey yapmıyor, ikinci detent çalışıyor.
//
// Sebep: `steps` biriktiricisi yön değiştiğinde SIFIRLANMIYORDU.
//
// Bir detent 4 quadrature geçişi üretir ve eşik 4'tür. Teoride her detent
// sonunda `steps == 0` kalır. Pratikte KALMAZ:
//
//   · sıçrama filtresi ara sıra geçerli bir geçişi eler
//   · `attachInterrupt` ile kurulan GPIO ISR'si IRAM'de değildir; `store`
//     task'ı flash'a yazarken (geçmiş kaydı her 60 sn) kesme geçici olarak
//     çalışmaz ve o sırada dönen encoder'ın geçişleri düşer
//
// Sonuç: biriktiricide kalıcı bir artık oluşur (örn. +3). Aynı yönde devam
// ederken bu ZARARSIZDIR — emisyon yalnızca bir geçiş kayar ve kullanıcı
// farkı hissetmez:
//
//   ileri:  +3 →(+1) 4 ✅ TIK → 0 →(+1,+2,+3) 3 →(+1) 4 ✅ TIK → 0 …
//
// Ama yön değişince artık TERSE ÇALIŞIR: −4'e ulaşmak için önce +3'ü
// harcamak gerekir, yani **iki detent**:
//
//   geri:   +3 →(−1) 2 →(−1) 1 →(−1) 0 →(−1) −1   ← 1. detent bitti, TIK YOK
//           −2, −3, −4 ✅ TIK                       ← 2. detentte geldi
//
// Tam olarak bildirilen davranış budur.
//
// Çözüm: yön değiştiğinde biriken kısmi adımlar ATILIR. Kısmi bir dönüş zaten
// kullanıcının "bir tık" saymadığı harekettir; onu ters yöndeki ilk tıkın
// hesabına yazmak yanlıştır.

#include <stdint.h>

namespace core {

/// Bir çözümleme turunun sonucu.
enum class EncoderTick : uint8_t
{
    NONE = 0,  ///< henüz bir detent tamamlanmadı
    CW   = 1,  ///< saat yönünde bir detent
    CCW  = 2,  ///< saat yönünün tersine bir detent
};

/// Tipik EC11 sınıfı encoder: detent başına 4 quadrature geçişi.
constexpr uint8_t DEFAULT_STEPS_PER_DETENT = 4;

/// Çözücünün taşıdığı tüm durum. POD.
struct EncoderDecoder
{
    uint8_t phase;  ///< son bilinen quadrature durumu: (A << 1) | B
    int8_t  steps;  ///< detente dönüşmemiş adımlar (işaretli)

    void reset(uint8_t initialPhase)
    {
        phase = static_cast<uint8_t>(initialPhase & 0x03u);
        steps = 0;
    }
};

/// Quadrature geçiş tablosu.
///
/// İndeks = (öncekiDurum << 2) | yeniDurum · Değer: +1 / −1 / 0 (geçersiz).
///
/// NEDEN TABLO: uzun bir `if/else if` zinciri yerine sabit süreli arama —
/// ISR'de olması gereken budur. Geçersiz (iki bit birden değişen) geçişler
/// 0 döner ve sayılmaz; gürültü reddinin ilk katmanı budur.
constexpr int8_t QUAD_TABLE[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0,
};

/// İki quadrature durumu arasındaki yön.
constexpr int8_t quadDelta(uint8_t prev, uint8_t next)
{
    return QUAD_TABLE[((prev & 0x03u) << 2) | (next & 0x03u)];
}

/// Yalnızca bilinen durumu günceller, **saymaz**.
///
/// Zaman kapısı (sıçrama filtresi) bir kesmeyi reddettiğinde çağrılır.
/// Reddedilen kenarda durumu güncellememek, çözücünün gerçek pin durumundan
/// AYRIŞMASINA yol açardı: bir sonraki gerçek geçiş bayat bir "önceki durum"a
/// göre hesaplanır, tabloda geçersiz görünür ve **sessizce kaybolur**. Bu,
/// yukarıda anlatılan artığın ikinci kaynağıdır.
/// (`constexpr` DEĞİL: `constexpr void` C++14 gerektirir, firmware C++11
/// derlenir — `Intensity` makro çakışmasıyla aynı sınıf bir tuzak.)
inline void encoderSyncPhase(EncoderDecoder& d, uint8_t newPhase)
{
    d.phase = static_cast<uint8_t>(newPhase & 0x03u);
}

/// Bir quadrature geçişini işler ve detent tamamlandıysa olayı döndürür.
///
/// @param d               çözücü durumu — **değiştirilir**
/// @param newPhase        `(A << 1) | B`
/// @param stepsPerDetent  detent başına geçiş sayısı; 0 verilirse 1 kabul edilir
inline EncoderTick encoderAdvance(EncoderDecoder& d, uint8_t newPhase,
                                  uint8_t stepsPerDetent)
{
    const int8_t delta = quadDelta(d.phase, newPhase);
    d.phase = static_cast<uint8_t>(newPhase & 0x03u);

    if (delta == 0)
    {
        return EncoderTick::NONE;  // geçersiz geçiş (gürültü)
    }

    // ── YÖN DEĞİŞTİ: BİRİKMİŞ KISMİ ADIMLARI AT ────────────────────────────
    // Bu iki satır, "geri dönerken ilk tık kayboluyor" hatasının çözümüdür.
    // Ters yöndeki ilk detent, önceki yönün artığını ödemek zorunda kalmaz.
    if ((delta > 0 && d.steps < 0) || (delta < 0 && d.steps > 0))
    {
        d.steps = 0;
    }

    // Taşma koruması: `int8_t` sınırında sarma, işaret değişimi demektir ve
    // encoder aniden ters yöne dönmüş gibi görünürdü.
    int16_t acc = static_cast<int16_t>(d.steps) + delta;
    if (acc > 127)  { acc = 127; }
    if (acc < -128) { acc = -128; }
    d.steps = static_cast<int8_t>(acc);

    const int8_t threshold =
        static_cast<int8_t>(stepsPerDetent == 0u ? 1u : stepsPerDetent);

    if (d.steps >= threshold)
    {
        d.steps = static_cast<int8_t>(d.steps - threshold);
        return EncoderTick::CW;
    }
    if (d.steps <= -threshold)
    {
        d.steps = static_cast<int8_t>(d.steps + threshold);
        return EncoderTick::CCW;
    }
    return EncoderTick::NONE;
}

// --- Derleme zamanı doğrulama ----------------------------------------------

// Tablo simetrik olmalı: ileri bir geçişin tersi geri olmalıdır. Bir satır
// yanlış girilirse encoder tek yönde çalışır ve bu sahada ancak elle fark
// edilir.
static_assert(quadDelta(0, 1) == -quadDelta(1, 0), "quadrature tablosu 00<->01 asimetrik");
static_assert(quadDelta(1, 3) == -quadDelta(3, 1), "quadrature tablosu 01<->11 asimetrik");
static_assert(quadDelta(3, 2) == -quadDelta(2, 3), "quadrature tablosu 11<->10 asimetrik");
static_assert(quadDelta(2, 0) == -quadDelta(0, 2), "quadrature tablosu 10<->00 asimetrik");

// Aynı durumda kalmak hareket DEĞİLDİR.
static_assert(quadDelta(0, 0) == 0 && quadDelta(1, 1) == 0 &&
                  quadDelta(2, 2) == 0 && quadDelta(3, 3) == 0,
              "ayni duruma gecis hareket sayilmamali");

// İki bit birden değişen geçiş GEÇERSİZ olmalı (kaçırılmış ara durum).
static_assert(quadDelta(0, 3) == 0 && quadDelta(3, 0) == 0 &&
                  quadDelta(1, 2) == 0 && quadDelta(2, 1) == 0,
              "cift bit gecisi gecersiz sayilmali");

} // namespace core
