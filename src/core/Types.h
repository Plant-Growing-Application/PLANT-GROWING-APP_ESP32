#pragma once

// Temel paylaşılan tipler — TASK-004
//
// Bu dosya projedeki hemen her modülün bağımlı olacağı taban tipleri içerir.
// core/ katmanı hiçbir katmana bağımlı değildir (ARCHITECTURE D5): yalnızca
// standart kütüphane include edilir.
//
// Tüm tipler POD'dur. Gerekçe: StateStore snapshot deseni (TASK-007) memcpy ile
// kopyalama gerektirir; POD olmayan bir tip state'e giremez.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

namespace core {

// ---------------------------------------------------------------------------
// Alt sistem kimlikleri
//
// Hata kodlarının üst baytını oluşturur (bkz. ErrorCodes.h) ve log kayıtlarında
// kaynağı belirtir.
// ---------------------------------------------------------------------------
enum class Subsystem : uint8_t
{
    SYS      = 0x01,  ///< boot, mod makinesi, watchdog, supervisor
    CFG      = 0x02,  ///< konfigürasyon şeması ve kalıcılığı
    STORAGE  = 0x03,  ///< NVS, LittleFS, geçmiş veri
    SENSOR   = 0x04,  ///< sensör okuma ve işleme hattı
    ACTUATOR = 0x05,  ///< röleler, aktüatör kısıtları
    SAFETY   = 0x06,  ///< güvenlik kilitleri, acil durum
    NET      = 0x07,  ///< Wi-Fi, bağlantı yönetimi
    TIME     = 0x08,  ///< SNTP, zaman geçerliliği
    WEB      = 0x09,  ///< HTTP, WebSocket, API
    UI       = 0x0A,  ///< OLED, encoder, butonlar
};

// ---------------------------------------------------------------------------
// Log seviyeleri — ARCHITECTURE §16.1
// ---------------------------------------------------------------------------
enum class LogLevel : uint8_t
{
    INFO     = 0,  ///< normal olay
    WARNING  = 1,  ///< beklenmeyen ama tolere edilebilir
    ERROR    = 2,  ///< bir işlev kullanılamıyor → DEGRADED
    CRITICAL = 3,  ///< güvenlik/bütünlük tehlikede → güvenli duruma geç
};

// ---------------------------------------------------------------------------
// FixedString — heap kullanmayan sabit boyutlu metin tamponu
//
// `String` sınıfı state ve config yapılarında KULLANILMAZ (CODING_STANDARDS Y10):
// heap parçalanması yaratır ve POD olma özelliğini bozar.
//
// N = kullanılabilir karakter sayısı; tampon içeride N+1 bayttır (sonlandırıcı).
// ---------------------------------------------------------------------------
template <size_t N>
struct FixedString
{
    char    data[N + 1];
    uint8_t len;

    /// Tamponu boşaltır.
    void clear()
    {
        data[0] = '\0';
        len     = 0;
    }

    /// Kaynağı kopyalar; N karakterden uzunsa **sessizce kesilir**.
    /// Kesilme oldu mu bilgisi dönüş değeriyle verilir — çağıran isterse
    /// bunu hata olarak ele alabilir.
    bool assign(const char* src)
    {
        if (src == nullptr)
        {
            clear();
            return true;
        }

        size_t n = strnlen(src, N + 1);
        bool   fits = (n <= N);
        if (!fits)
        {
            n = N;
        }

        memcpy(data, src, n);
        data[n] = '\0';
        len     = static_cast<uint8_t>(n);
        return fits;
    }

    const char* c_str() const { return data; }
    uint8_t     length() const { return len; }
    bool        empty() const { return len == 0; }
    bool        equals(const char* other) const
    {
        return other != nullptr && strncmp(data, other, N + 1) == 0;
    }

    static constexpr size_t capacity() { return N; }
};

// N + 1 baytlık tampon uint8_t uzunluk alanına sığmalı
static_assert(FixedString<32>::capacity() < 255, "FixedString kapasitesi uint8_t sinirinda olmali");

// ---------------------------------------------------------------------------
// Range — kapalı aralık [min, max]
//
// Config doğrulaması (TASK-014) ve sensör aralık kontrolü (TASK-023) aynı
// mantığı paylaşır; tekrarlanmaması için burada tanımlanır.
// ---------------------------------------------------------------------------
template <typename T>
struct Range
{
    T min;
    T max;

    constexpr bool contains(T v) const { return v >= min && v <= max; }
    constexpr bool valid() const { return min <= max; }

    /// Değeri aralığa sıkıştırır.
    constexpr T clamp(T v) const { return v < min ? min : (v > max ? max : v); }
};

// ---------------------------------------------------------------------------
// POD doğrulaması — derleme zamanında zorlanır
//
// Bu tiplerden biri POD olmayı kaybederse StateStore snapshot deseni bozulur.
// Hata sahada değil derleyicide çıksın.
// ---------------------------------------------------------------------------
static_assert(std::is_trivially_copyable<FixedString<32>>::value,
              "FixedString trivially copyable olmali (StateStore snapshot)");
static_assert(std::is_standard_layout<FixedString<32>>::value,
              "FixedString standard layout olmali");
static_assert(std::is_trivially_copyable<Range<int32_t>>::value,
              "Range trivially copyable olmali");

} // namespace core
