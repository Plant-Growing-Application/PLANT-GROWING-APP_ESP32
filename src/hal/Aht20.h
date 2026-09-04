#pragma once

// AHT20 sıcaklık + bağıl nem sensörü sürücüsü — TASK-066
//
// ── NEDEN BU ÇİP ────────────────────────────────────────────────────────────
// ADC1 bütçesi tükenmişti (ISSUE-001: encoder 32/33'ü işgal ediyor), yani
// ANALOG bir ortam sensörü fiziksel olarak takılamazdı. AHT20 I2C'dir ve
// OLED'in zaten kullandığı hatta yeni pin istemeden oturur. Tek çipten iki
// ölçüm çıkar: ortam sıcaklığı ve bağıl nem.
//
// Bu, `SensorId::HUMIDITY`'nin ilk gerçek sürücüsüdür. Kimlik TASK-006'dan
// beri tanımlıydı, arayüzde kartı vardı ve geçmiş verisinde yeri ayrılmıştı
// ama HİÇBİR KOD onu okumuyordu — arayüzde hayalet bir sensör duruyordu.
//
// ── BLOKLAMA YASAK (ARCHITECTURE P3) ────────────────────────────────────────
// AHT20 ölçümü ~80 ms sürer. `delay(80)` yazmak `io_sense` task'ının 250 ms'lik
// periyodunun üçte birini yerdi ve güvenlik sensörlerinin (su seviyesi, akış)
// örneklemesini geciktirirdi — kuru çalışma tespitini yavaşlatan bir ortam
// sensörü kabul edilemez.
//
// Bu yüzden sürücü İKİ FAZLIDIR: bir çağrı ölçümü tetikler, sonraki çağrı
// sonucu okur. Hiçbir noktada beklenmez. Bedeli, okunan değerin bir örnekleme
// periyodu kadar eski olmasıdır; ortam sıcaklığı ve nem o ölçekte değişmez.
//
// ── CRC ─────────────────────────────────────────────────────────────────────
// AHT20 (AHT10'dan farklı olarak) her okumaya CRC8 ekler. Doğrulanır: bozuk
// bir I2C aktarımı 40 °C yerine 120 °C okutabilir ve bu değer ısıtıcı kuralına
// girer. Kontrol edilmeyen bir CRC, var olmayan bir korumadır.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace hal {
namespace aht20 {

/// I2C cihaz adresi — sabittir, AHT20'de adres seçimi yoktur.
constexpr uint8_t I2C_ADDRESS = 0x38;

/// Ölçümün tamamlanması için beklenen süre (veri sayfası: tipik 80 ms).
/// Pay bırakılmıştır; erken okuma `busy` bitiyle zaten yakalanır.
constexpr core::Duration CONVERSION_TIME = core::millisecs(100);

/// Tetiklenen bir ölçüm bu süre içinde tamamlanmazsa çip yanıtsız sayılır ve
/// durum makinesi yeniden tetiklemeye döner. Bu olmadan tek bir kayıp yanıt
/// sürücüyü kalıcı olarak `MEASURING` durumunda kilitlerdi.
constexpr core::Duration MEASURE_TIMEOUT = core::seconds(2);

/// Bu kadar ardışık hatadan sonra çip kullanılamaz sayılır.
constexpr uint16_t ERROR_LIMIT = 10;

/// Çipi hazırlar ve ilk ölçümü tetikler.
///
/// İlk ölçümün BAŞLATMADA tetiklenmesi bilinçlidir: boot ile ilk örnekleme
/// turu arasında geçen süre dönüşüm süresinden çok uzundur, dolayısıyla ilk
/// `sample()` çağrısı hazır bir değer bulur ve sensör "arızalı" görünmez.
core::ErrCode begin();

/// Durum makinesini ilerletir. **Bloklamaz.**
///
/// Aynı `now` değeriyle birden çok kez çağrılabilir: iki sensör sarmalayıcısı
/// (sıcaklık ve nem) tek çipi paylaşır ve ikisi de aynı turda bunu çağırır.
/// İkinci çağrı işlemsizdir — aksi hâlde her tur iki kez tetikleme yapılır ve
/// ölçüm hiç tamamlanmazdı.
void service(core::Millis now);

/// En az bir geçerli ölçüm alındı mı?
bool hasReading();

/// Son geçerli ortam sıcaklığı (°C). `hasReading()` yanlışken anlamsızdır.
float temperatureC();

/// Son geçerli bağıl nem (%).
float humidityPct();

/// Çip kullanılabilir durumda mı? (Ardışık hata sınırı aşılmadı.)
bool isAvailable();

/// Biriken ardışık hata sayısı — teşhis.
uint16_t errorCount();

} // namespace aht20
} // namespace hal
