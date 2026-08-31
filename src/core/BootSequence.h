#pragma once

// Aşamalı açılış yürütücüsü — TASK-010
//
// Mevcut sistemdeki iki ölümcül davranışı ortadan kaldırır:
//   · OLED init hatasında `while(true)` → tüm sistem kilitleniyordu
//   · LittleFS mount hatasında `setup()`'tan erken `return` → hiçbir task
//     oluşmuyordu, sistem yarı ölü kalıyordu
//
// TEMEL KURAL (ARCHITECTURE P4): hiçbir aşama başarısızlığı boot'u DURDURMAZ.
// Sistem kısıtlı modda ayakta kalır ve neyin çalışmadığını raporlar.
//
// KATMAN KISITI: `core/` katmanı `hal/` veya `services/`'e bağımlı olamaz (D5).
// Bu yüzden yürütücü aşama İÇERİĞİNİ bilmez; aşama fonksiyonları dışarıdan
// (TASK-013 boot wiring) verilir. Yürütücü yalnızca sırayı, zorunluluğu,
// süreyi ve mod türetmeyi yönetir.
//
// Yan etki olarak: yürütücü sahte aşama fonksiyonlarıyla donanımsız test
// edilebilir (TASK-064).

#include <stdint.h>

#include "BootReport.h"
#include "ErrorCodes.h"
#include "SystemState.h"

namespace core {

/// Açılış aşamaları — ARCHITECTURE §7.1 sırası.
///
/// Sıra rastgele değildir: GPIO güvenli seviye, log altyapısından bile ÖNCE
/// gelir. Röleler boot'un ilk milisaniyelerinde güvenli konuma alınmazsa
/// aktif-düşük bir röle modülünde pompa kuru çalışabilir (ISSUE-003).
enum class BootStage : uint8_t
{
    RESET_AND_WDT   = 0,  ///< ZORUNLU — reset nedeni okuma + TWDT yapılandırma
    GPIO_SAFE_STATE = 1,  ///< ZORUNLU — TÜM RÖLELER KAPALI
    CORE_SERVICES   = 2,  ///< ZORUNLU — Diagnostics, StateStore, kuyruklar
    CONFIG_LOAD     = 3,  ///< NVS → başarısızsa varsayılan config
    FILESYSTEM      = 4,  ///< LittleFS → başarısızsa web statiği yok
    DISPLAY_HW      = 5,  ///< OLED → başarısızsa sistem TAM çalışır, ekran yok
                          ///  (isim `DISPLAY` degil: Arduino.h `#define DISPLAY 0x1`)
    SENSOR_HW       = 6,  ///< ADC + PCNT → başarısızsa sensör kalitesi düşer
    NETWORK_RADIO   = 7,  ///< Wi-Fi → başarısızsa OFFLINE; güvenlik etkilenmez
    TASK_CREATION   = 8,  ///< ZORUNLU — beş task oluşturma
};

/// Aşama fonksiyonu imzası.
///
/// Aşama **yan etkisiz raporlama** yapar: sonucu döndürür, mod kararı VERMEZ.
/// Mod türetme yürütücünün işidir (ARCHITECTURE §7.2).
using BootStageFn = ErrCode (*)();

/// Tablo satırı: hangi aşama, zorunlu mu, hangi fonksiyon.
struct BootStageDef
{
    BootStage   stage;
    bool        required;  ///< true → başarısızlığı SAFE moda götürür
    BootStageFn fn;
};

/// Bir aşamanın bu süreden uzun sürmesi bir sorun göstergesidir: ya donanım
/// bekleniyor ya da aşama içinde `delay()` var (yasak — CODING_STANDARDS Y3).
/// Aşıldığında WARNING loglanır.
constexpr Duration SLOW_STAGE_THRESHOLD = millisecs(500);

namespace boot {

/// Aşama tablosunu sırayla çalıştırır, sonuçları rapora yazar ve sistem
/// modunu türetir.
///
/// Hiçbir aşama başarısızlığı yürütmeyi durdurmaz: tablo sonuna kadar
/// çalıştırılır.
///
/// @param table     aşama tanımları, çalıştırılacak sırada
/// @param count     tablo uzunluğu
/// @param outReport doldurulacak boot raporu
/// @return türetilen sistem modu (RUNNING / DEGRADED / SAFE)
SystemMode run(const BootStageDef* table, uint8_t count, BootReport& outReport);

/// Boot sonucundan sistem modunu türetir.
///
///   zorunlu aşama başarısız         → SAFE
///   zorunlu olmayan aşama başarısız → DEGRADED
///   hepsi başarılı                  → RUNNING
///
/// `EMERGENCY` bir boot sonucu DEĞİLDİR: çalışma zamanında mandallanır
/// (TASK-032). Mod türetmenin iki yerde yapılmaması için bu sınır nettir.
SystemMode deriveMode(const BootReport& report);

/// Aşamanın insan okunabilir adı — rapor çıktısı ve teşhis için.
const char* stageName(uint8_t stageId);

} // namespace boot
} // namespace core
