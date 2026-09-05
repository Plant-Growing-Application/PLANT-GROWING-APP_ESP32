#pragma once

// OLED paneli sürücüsü — TASK-020
//
// INIT HATASI BİR DURUMDUR, SİSTEMİ DURDURMAZ.
//
// Mevcut sistemde OLED init hatası `while (true)` ile karşılanıyordu ve
// TÜM SİSTEMİ kilitliyordu (REQUIREMENTS Kritik Problem 4). Ekranı olmayan
// bir sera cihazı hâlâ sulama yapabilir; kilitlenen bir cihaz yapamaz.
//
// Burada `isAvailable()` sorgulanır: OLED yoksa sistem TAM çalışır, yalnızca
// ekran çizilmez (ARCHITECTURE §16.3).
//
// DONANIMA TEK KAPI (P2): bu sürücüye **yalnızca `ui` task'ı** erişir.
// Mevcut sistemde `Sensor.cpp`, `MyWifi.cpp` ve `GrowPlant.cpp` aynı `oled`
// nesnesine korumasız yazıyordu.

#include <stdint.h>

#include "core/ErrorCodes.h"

namespace hal {

/// Ekran çözünürlüğü.
constexpr uint8_t OLED_WIDTH  = 128;
constexpr uint8_t OLED_HEIGHT = 64;

namespace oled {

/// I2C'yi ve paneli başlatır.
///
/// Başarısızlık **hata değil durumdur**: `ErrCode` döner ama sistem devam eder.
/// Boot yürütücüsü bunu zorunlu olmayan aşama olarak işler (DEGRADED).
core::ErrCode begin();

/// Panel kullanılabilir mi? `false` ise `ui` task'ı çizmeden çalışır.
bool isAvailable();

// --- Çizim yüzeyi -----------------------------------------------------------
//
// Sürücü YALNIZCA çizim yüzeyi sunar. Hangi verinin nereye çizileceği
// `interfaces/ui/` katmanının işidir (D6).

void clear();
void setTextSize(uint8_t size);
void drawText(int16_t x, int16_t y, const char* text);
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/// Dikdörtgen: `filled == false` beyaz ÇERÇEVE, `filled == true` **SİYAH**
/// dolgu — yani alanı TEMİZLER.
///
/// İsim yanıltıcıdır ve tam olarak bu yüzden üç ayrı yerde ters çizime yol
/// açtı (TASK-075): başlık bantları, ACİL rozeti ve Wi-Fi çubukları "dolu"
/// isteyip görünmez oluyordu. Vurgulu (beyaz) dolgu `fillHighlight()`'tır.
///
/// `filled == true` bugün hiçbir yerden çağrılmıyor; bir alanı temizlemek
/// meşru bir ihtiyaç olduğu için duruyor.
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled);

/// BEYAZ dolgulu dikdörtgen — seçim çubuğunun ve rozetlerin zemini.
///
/// Ters renkli satır yazmanın ilk yarısıdır; ikinci yarısı
/// `drawTextInverse()`. İkisi olmadan mono bir panelde "seçili" durumu
/// yalnızca satır başına konan bir işaretle anlatılabiliyordu ve bir metre
/// mesafeden seçilmiyordu (TASK-075).
void fillHighlight(int16_t x, int16_t y, int16_t w, int16_t h);

/// SİYAH metin — YALNIZCA `fillHighlight()` ile doldurulmuş zemin üzerine.
///
/// Boş zemine yazılırsa GÖRÜNMEZ; çağıran önce zemini doldurmalıdır.
void drawTextInverse(int16_t x, int16_t y, const char* text);

void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h);

/// Metnin piksel genişliğini ölçer (ortalama/sığdırma için).
uint16_t textWidth(const char* text, uint8_t size);

// --- Aktarım ----------------------------------------------------------------

/// Tüm çerçeveyi panele gönderir. Ekran DEĞİŞİMİNDE kullanılır.
core::ErrCode display();

/// Yalnızca belirtilen dikdörtgeni gönderir.
///
/// I2C üzerinden tam ekran aktarımı belirgin süre alır ve `ui` task'ının
/// 50 ms periyodunu zorlar. Aynı ekranda kalırken kirli alan güncellemesi
/// kullanılmalıdır.
core::ErrCode displayRegion(int16_t x, int16_t y, int16_t w, int16_t h);

// --- Teşhis ------------------------------------------------------------------

/// Ardışık I2C hatası sayısı. Eşik aşılınca panel kullanılamaz işaretlenir;
/// sonsuz yeniden deneme YAPILMAZ.
uint16_t i2cErrorCount();

} // namespace oled
} // namespace hal
