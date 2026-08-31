#pragma once

// Acil durum mandalı ve kurtarma — TASK-032
//
// ── MANDALLAMA İLKESİ (ARCHITECTURE §12.2) ──────────────────────────────────
// Kritik ihlal KENDİLİĞİNDEN TEMİZLENMEZ. Koşul düzelse bile operatör onayı
// gerekir.
//
// Gerekçe: aralıklı bir arıza (gevşek kablo, ara ara boşalan hazne)
// kendiliğinden temizlenen bir sistemde **sessizce tekrarlanır**. Pompa her
// seferinde birkaç saniye kuru çalışır ve haftalar içinde ölür. Mandal,
// insanın olaydan haberdar olmasını ZORUNLU kılar.
//
// ── KALICILIK: NVS (TASK-032 Karar 1) ───────────────────────────────────────
// Mandal güç kesintisinden sağ çıkar. "Yalnızca RAM" seçeneği kullanıcının
// sorunu reset atarak "çözmesine" izin verirdi. "Boot'ta koşullar sağlıklıysa
// temizle" seçeneği ise kuru çalışma için yalnızca-RAM'e çöker: güç
// kesildikten sonra pompa zaten kapalıdır, akış doğal olarak sıfırdır ve
// koşul değerlendirilemez.
//
// Bilinen bedeli: operatör sahada değilse sistem kilitli kalır. Bilinçli
// takas — kilitli bir sistem, sessizce ölen bir pompadan iyidir.
//
// ── YAZMA SIRASI ────────────────────────────────────────────────────────────
// RAM mandalı → röleleri kapat → CRITICAL logla → NVS'e yaz.
// NVS bloklayan bir flash işlemidir; en sona bırakılır. Yazma başarısız olsa
// bile ilk üç adım geçerlidir: güvenlik RAM mandalında ve kapalı rölelerdedir,
// kalıcılık bir İYİLEŞTİRMEDİR.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace domain {
namespace emergency {

/// Mandal kaydı — kalıcı, serbest metin İÇERMEZ.
///
/// `Diagnostics` deseniyle tutarlı: kod + zaman saklanır, metne çevirme
/// arayüz katmanının işidir. Gömülü bir sistemde kilobaytlarca metin
/// saklamak savunulamaz.
struct LatchRecord
{
    uint32_t      magic;       ///< geçerlilik damgası
    core::ErrCode reason;      ///< tetikleyicinin neden kodu
    uint8_t       latched;     ///< 1 = mandal aktif
    uint8_t       source;      ///< core::CommandSource değeri
    uint32_t      uptimeMs;    ///< mandal anındaki uptime
    uint32_t      bootCount;   ///< kaçıncı boot'ta oluştu
};

/// Kalıcı mandalı NVS'ten okur. **Boot Aşama 3'ten sonra çağrılmalıdır**
/// (NVS hazır olmalı). Röleler zaten Aşama 1'de güvenli konumdadır.
core::ErrCode begin();

/// Acil durumu mandallar. Yeniden girişte etkisizdir (ilk neden korunur).
///
/// Röleleri **doğrudan** kapatır: `ActuatorManager::forceAllOff()`. Bir
/// sonraki döngüyü beklemez.
void trigger(core::ErrCode reason, uint8_t source, core::Millis now);

bool          latched();
core::ErrCode reason();
uint32_t      latchedUptimeMs();
uint32_t      latchedBootCount();

/// Mandalı temizler — **açık operatör eylemi** gerektirir.
///
/// `blockingMask` acil durum biti DIŞINDA aktif olan kilitleri taşır. Sıfır
/// değilse temizleme REDDEDİLİR: koşullar düzelmeden onay verilirse pompa
/// açılır ve aynı arıza tekrarlar.
///
/// @return `OK` = temizlendi; `SAFETY_BLOCKED` = koşullar düzelmemiş;
///         `SAFETY_EMERGENCY_LATCHED` = zaten mandal yok
core::ErrCode clear(uint32_t blockingMask);

} // namespace emergency
} // namespace domain
