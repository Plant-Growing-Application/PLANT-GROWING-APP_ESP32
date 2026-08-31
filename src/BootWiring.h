#pragma once

// Boot bağlama — TASK-060
//
// Bu dosya, TASK-002…059 arasında yazılmış modülleri boot sırasına ve task
// tablosuna bağlar. **Yeni işlev içermez**; yalnızca bağlantı kurar.
//
// ── ISSUE-013 / ISSUE-018 ───────────────────────────────────────────────────
// Boot wiring planda hiçbir task'a atanmamıştı ve sistem bu yüzden HİÇ boot
// etmedi — 33 task'ta yazılan hiçbir kod çalışmadı. "Tüm task'ları birlikte
// çalıştırmak" (TASK-060 kapsam maddesi 1) onsuz tanım gereği imkânsız
// olduğu için wiring bu task'ın kapsamına alındı.

#include "core/BootSequence.h"
#include "core/SystemState.h"

namespace app {

/// Boot aşamalarını sırayla çalıştırır ve sistem modunu döndürür.
///
/// Aşama sırası ARCHITECTURE §7.1 ile birebir; **Aşama 1 (röleler güvenli)
/// Aşama 0'dan hemen sonra gelir** ve bu pazarlıksızdır: pompanın korunması
/// log altyapısının hazır olmasından önceliklidir.
core::SystemMode runBoot(core::BootReport& outReport);

} // namespace app
