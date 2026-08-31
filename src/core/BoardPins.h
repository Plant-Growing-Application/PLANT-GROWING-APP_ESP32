#pragma once

// Merkezî pin haritası — TASK-002
//
// Gerekçeler ve kablolama: docs/HARDWARE.md
// Bu dosya YALNIZCA pin numarası ve donanım kısıt kontrolü içerir.
// İş kuralı, eşik veya zamanlama buraya girmez.
//
// Bu header hiçbir şey include etmez; toplayıcı header değildir (ARCHITECTURE §17).

#include <stdint.h>

namespace board {

// ---------------------------------------------------------------------------
// ESP32 donanım kısıtları — derleme zamanında zorlanır
// ---------------------------------------------------------------------------

/// ADC1 kanalı olan pinler. Wi-Fi radyosu aktifken ADC2 KULLANILAMAZ,
/// bu yüzden tüm analog sensörler bu kümeden seçilmek zorundadır.
constexpr bool isAdc1(uint8_t p)
{
    return p == 32 || p == 33 || p == 34 || p == 35 || p == 36 || p == 39;
}

/// Giriş-only pinler: çıkış sürülemez ve DAHİLİ PULL-UP YOKTUR.
/// Buraya bağlanan anahtar/darbe sensörü harici pull-up gerektirir.
constexpr bool isInputOnly(uint8_t p)
{
    return p == 34 || p == 35 || p == 36 || p == 39;
}

/// Boot sırasında seviyesi okunan strapping pinleri. Röle gibi çıkışlar için
/// uygun değildir: boot anında istenmeyen seviye üretebilir.
constexpr bool isStrapping(uint8_t p)
{
    return p == 0 || p == 2 || p == 5 || p == 12 || p == 15;
}

/// Dahili flash'a ayrılmış pinler — kullanılamaz.
constexpr bool isFlashReserved(uint8_t p)
{
    return p >= 6 && p <= 11;
}

/// Güvenle çıkış olarak sürülebilen pin.
constexpr bool isSafeOutput(uint8_t p)
{
    return !isInputOnly(p) && !isStrapping(p) && !isFlashReserved(p);
}

/// Dahili pull-up'a sahip, güvenli dijital giriş.
constexpr bool isSafePullupInput(uint8_t p)
{
    return !isInputOnly(p) && !isFlashReserved(p);
}

// ---------------------------------------------------------------------------
// I2C — OLED paneli
// ---------------------------------------------------------------------------
constexpr uint8_t I2C_SDA = 21;
constexpr uint8_t I2C_SCL = 22;

// ---------------------------------------------------------------------------
// Analog sensörler — hepsi ADC1 olmak ZORUNDA
// ---------------------------------------------------------------------------
constexpr uint8_t ADC_WATER_TEMP = 35;  ///< NTC (donanım kararı: TASK-024)
constexpr uint8_t ADC_PH         = 34;
constexpr uint8_t ADC_EC         = 36;
constexpr uint8_t ADC_SPARE      = 39;  ///< yedek: analog seviye / 4. sensör

// ---------------------------------------------------------------------------
// Dijital girişler
// ---------------------------------------------------------------------------
constexpr uint8_t FLOW_PULSE       = 4;   ///< PCNT ile sayılır (TASK-019)
constexpr uint8_t LEVEL_FLOAT_LOW  = 13;  ///< şamandıra: düşük seviye
constexpr uint8_t LEVEL_FLOAT_CRIT = 14;  ///< şamandıra: kritik seviye
// ── ENCODER: MEVCUT DONANIMA GERI ALINDI (ISSUE-001 kapatilmadi, KABUL EDILDI)
//
// TASK-002'de analog butceyi korumak icin 18/19'a tasinmisti. Ilk sahada
// donanimin hala 33/32'ye kablolu oldugu goruldu ve kullanici kodun geri
// alinmasini tercih etti.
//
// BEDELI ACIKCA KABUL EDILIYOR: GPIO 32/33 ADC1_CH4 ve CH5'tir. Bu iki
// kanal artik encoder tarafindan isgal ediliyor; mevcut 4 analog sensor
// (34/35/36/39) ETKILENMIYOR ama BESINCI/ALTINCI analog sensor icin yer
// KALMADI. Yeni bir analog sensor gerekirse ya encoder tasinacak ya harici
// bir ADC (ADS1115 vb.) eklenecek.
constexpr uint8_t ENCODER_A        = 33;
constexpr uint8_t ENCODER_B        = 32;
constexpr uint8_t ENCODER_PUSH     = 25;
constexpr uint8_t BUTTON_BACK      = 27;

// ---------------------------------------------------------------------------
// Dijital çıkışlar
// ---------------------------------------------------------------------------
constexpr uint8_t RELAY_WATER_PUMP = 16;
constexpr uint8_t RELAY_AIR_PUMP   = 17;
constexpr uint8_t STATUS_LED       = 23;

// ---------------------------------------------------------------------------
// Röle polaritesi — DERLEME ZAMANI SABİTİ (config DEĞİL)
//
// NEDEN CONFIG DEĞİL: boot Aşama 1 röleleri config yüklenmeden ÖNCE güvenli
// seviyeye alır (ARCHITECTURE §7.1). Polarite o anda bilinmek zorundadır;
// config'te tutmak, güvenli seviyenin bilinmediği bir pencere yaratırdı.
// Ayrıca polarite fiziksel kablolamanın bir özelliğidir, çalışma zamanı
// tercihi değil.
//
// !!! DOĞRULANMAMIŞ VARSAYIM (ISSUE-003) !!!
// Bu değer donanımda ÖLÇÜLMEDİ. Yaygın optokuplörlü röle modülleri
// aktif-düşüktür, bu yüzden `true` seçildi. Yanlışsa boot anında pompa
// çalışabilir. TASK-017'nin Definition of Done'ı ölçüm gerektirir.
constexpr bool RELAY_ACTIVE_LOW = true;

/// Rölenin KAPALI olduğu fiziksel pin seviyesi.
constexpr uint8_t RELAY_SAFE_LEVEL = RELAY_ACTIVE_LOW ? 1u : 0u;

/// Rölenin AÇIK olduğu fiziksel pin seviyesi.
constexpr uint8_t RELAY_ACTIVE_LEVEL = RELAY_ACTIVE_LOW ? 0u : 1u;

// ---------------------------------------------------------------------------
// Derleme zamanı doğrulama
//
// Pin planı donanım kısıtlarına karşı burada zorlanır. Bir pin yanlış sınıfa
// taşınırsa proje DERLENMEZ — hata sahada değil, derleyicide yakalanır.
// ---------------------------------------------------------------------------

// Analog sensörler ADC1'de olmalı (Wi-Fi + ADC2 çakışması)
static_assert(isAdc1(ADC_WATER_TEMP), "ADC_WATER_TEMP ADC1 pini olmali");
static_assert(isAdc1(ADC_PH),         "ADC_PH ADC1 pini olmali");
static_assert(isAdc1(ADC_EC),         "ADC_EC ADC1 pini olmali");
static_assert(isAdc1(ADC_SPARE),      "ADC_SPARE ADC1 pini olmali");

// Röleler ve LED güvenli çıkış olmalı (strapping / giriş-only değil)
static_assert(isSafeOutput(RELAY_WATER_PUMP), "RELAY_WATER_PUMP guvenli cikis olmali");
static_assert(isSafeOutput(RELAY_AIR_PUMP),   "RELAY_AIR_PUMP guvenli cikis olmali");
static_assert(isSafeOutput(STATUS_LED),       "STATUS_LED guvenli cikis olmali");

// Dahili pull-up gerektiren girişler giriş-only pinlerde OLMAMALI.
// ISSUE-002: eski kodda akış sensörü GPIO 34'te INPUT_PULLUP ile kullanılıyordu;
// GPIO 34'te dahili pull-up yoktur ve bu ayar donanımsal olarak etkisizdi.
static_assert(isSafePullupInput(FLOW_PULSE),       "FLOW_PULSE dahili pull-up gerektirir");
static_assert(isSafePullupInput(LEVEL_FLOAT_LOW),  "LEVEL_FLOAT_LOW dahili pull-up gerektirir");
static_assert(isSafePullupInput(LEVEL_FLOAT_CRIT), "LEVEL_FLOAT_CRIT dahili pull-up gerektirir");
static_assert(isSafePullupInput(ENCODER_A),        "ENCODER_A dahili pull-up gerektirir");
static_assert(isSafePullupInput(ENCODER_B),        "ENCODER_B dahili pull-up gerektirir");
static_assert(isSafePullupInput(ENCODER_PUSH),     "ENCODER_PUSH dahili pull-up gerektirir");
static_assert(isSafePullupInput(BUTTON_BACK),      "BUTTON_BACK dahili pull-up gerektirir");

// ISSUE-001 — encoder ADC1 kanallarini isgal ediyor.
//
// Bu kisit BILINCLI OLARAK GEVSETILDI (mevcut donanim 33/32'ye kablolu).
// Yerine ANALOG SENSORLERLE CAKISMA yasagi konuldu: encoder pinleri bir
// analog sensor pini ile AYNI olamaz. Boyle bir cakisma sessiz bir ariza
// olurdu — sensor okumasi encoder darbeleriyle bozulurdu.
static_assert(ENCODER_A != ADC_WATER_TEMP && ENCODER_A != ADC_PH &&
                  ENCODER_A != ADC_EC && ENCODER_A != ADC_SPARE,
              "ENCODER_A bir analog sensor pini ile CAKISIYOR");
static_assert(ENCODER_B != ADC_WATER_TEMP && ENCODER_B != ADC_PH &&
                  ENCODER_B != ADC_EC && ENCODER_B != ADC_SPARE,
              "ENCODER_B bir analog sensor pini ile CAKISIYOR");

// Encoder ADC1 isgal ediyorsa, KALAN analog butce sifirdir. Yeni bir analog
// sensor eklenirse bu iddia BOZULUR ve derleme durur — sessiz cakisma yok.
static_assert(!(isAdc1(ENCODER_A) && isAdc1(ENCODER_B)) ||
                  (ADC_WATER_TEMP != 32 && ADC_WATER_TEMP != 33 &&
                   ADC_PH != 32 && ADC_PH != 33 &&
                   ADC_EC != 32 && ADC_EC != 33 &&
                   ADC_SPARE != 32 && ADC_SPARE != 33),
              "encoder ADC1'de: 32/33 analog olarak KULLANILAMAZ");

} // namespace board
