#include "services/HistoryStore.h"

#include <string.h>

#include "core/Diagnostics.h"
#include "hal/FileStore.h"

namespace services {
namespace history {
namespace {

using core::ErrCode;
using core::SensorId;

uint32_t g_writeIdx    = 0;   ///< bir sonraki yazılacak slot
uint32_t g_nextSeq     = 1;   ///< 0 geçersiz sayıldığı için 1'den başlar
uint32_t g_stored      = 0;   ///< dosyadaki geçerli kayıt sayısı
uint32_t g_corrupt     = 0;
uint32_t g_writeErrors = 0;
bool     g_ready       = false;

uint32_t offsetOf(uint32_t index) { return index * RECORD_BYTES; }

/// CRC-8 (poly 0x07). `crc8` alanı hariç tüm kayıt üzerinden.
uint8_t crc8Of(const Record& r)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&r);
    uint8_t        c = 0xFFu;
    for (size_t i = 0; i < RECORD_BYTES - 1u; ++i)
    {
        c ^= p[i];
        for (uint8_t b = 0; b < 8u; ++b)
        {
            c = (c & 0x80u) ? static_cast<uint8_t>((c << 1) ^ 0x07u)
                            : static_cast<uint8_t>(c << 1);
        }
    }
    return c;
}

/// Kayıt geçerli mi? `seq == 0` boş slot; CRC hatası yarım yazılmış kayıt
/// (yazma sırasında güç kesintisi).
bool valid(const Record& r) { return r.seq != 0u && r.crc8 == crc8Of(r); }

bool readRecord(uint32_t index, Record& out)
{
    if (hal::fs::readAt(FILE_PATH, offsetOf(index), &out, RECORD_BYTES) != ErrCode::OK)
    {
        memset(&out, 0, sizeof(out));
        return false;
    }
    return true;
}

/// Bitişik bir kayıt aralığını TEK dosya açmasıyla okur.
///
/// `readAt` her çağrıda dosyayı açıp kapattığı için kayıt başına çağrı
/// yapmak kabul edilemez maliyettedir.
uint16_t readSpan(uint32_t index, uint16_t count, Record* out)
{
    if (count == 0u) { return 0u; }
    const size_t bytes = static_cast<size_t>(count) * RECORD_BYTES;
    if (hal::fs::readAt(FILE_PATH, offsetOf(index), out, bytes) != ErrCode::OK)
    {
        return 0u;
    }
    return count;
}

/// Adım 1: geçerli kayıt sayısı.
///
/// Geçersiz kayıtlar bir SONEK oluşturur (dosya sırayla doldurulur), bu
/// yüzden ilk geçersiz dizin ikili aramayla bulunabilir.
uint32_t findValidCount()
{
    Record r{};
    if (!readRecord(0, r) || !valid(r)) { return 0u; }

    if (readRecord(RECORD_COUNT - 1u, r) && valid(r)) { return RECORD_COUNT; }

    uint32_t lo = 0;                // valid
    uint32_t hi = RECORD_COUNT - 1; // invalid
    while (hi - lo > 1u)
    {
        const uint32_t mid = lo + (hi - lo) / 2u;
        if (readRecord(mid, r) && valid(r)) { lo = mid; }
        else                                { hi = mid; }
    }
    return lo + 1u;
}

/// Adım 3: dolu halkada `seq`in düştüğü nokta — döndürülmüş sıralı dizi
/// probleminin standart çözümü.
uint32_t findRotation()
{
    Record a{}, b{};
    if (!readRecord(0, a) || !readRecord(RECORD_COUNT - 1u, b)) { return 0u; }

    // Hiç dönmemişse (seq[0] < seq[N-1]) yazma başa döner.
    if (a.seq < b.seq) { return 0u; }

    uint32_t lo = 0;
    uint32_t hi = RECORD_COUNT - 1u;
    while (lo < hi)
    {
        const uint32_t mid = lo + (hi - lo) / 2u;
        Record m{};
        if (!readRecord(mid, m)) { return 0u; }
        if (m.seq > a.seq) { lo = mid + 1u; }   // hâlâ eski (yüksek) tarafta
        else               { hi = mid; }
    }
    return lo;
}

} // namespace

int16_t scaleFor(SensorId id, float v)
{
    float s = v;
    switch (id)
    {
        case SensorId::WATER_TEMP: s = v * 10.0f;  break;
        case SensorId::PH:
        case SensorId::EC:
        case SensorId::WATER_FLOW: s = v * 100.0f; break;
        default:                   s = v;          break;   // seviye, nem
    }
    // `int16` sınırına kırp: taşma sessiz bir işaret değişimi üretirdi.
    if (s > 32767.0f)  { s = 32767.0f; }
    if (s < -32768.0f) { s = -32768.0f; }
    return static_cast<int16_t>(s);
}

float unscale(SensorId id, int16_t raw)
{
    switch (id)
    {
        case SensorId::WATER_TEMP: return static_cast<float>(raw) / 10.0f;
        case SensorId::PH:
        case SensorId::EC:
        case SensorId::WATER_FLOW: return static_cast<float>(raw) / 100.0f;
        default:                   return static_cast<float>(raw);
    }
}

core::ErrCode begin()
{
    g_writeIdx = 0;
    g_nextSeq  = 1;
    g_stored   = 0;
    g_corrupt  = 0;

    if (!hal::fs::isMounted())
    {
        // Dosya sistemi yoksa geçmiş kaydı yapılmaz — ama sistem DURMAZ (P4).
        core::diag::log(core::LogLevel::WARNING, ErrCode::STORAGE_FS_MOUNT_FAILED, 0,
                        "gecmis deposu devre disi - dosya sistemi yok");
        return ErrCode::STORAGE_FS_MOUNT_FAILED;
    }

    // ── HALKA KONUMU: iki ikili arama, ~30 okuma ───────────────────────────
    const uint32_t v = findValidCount();
    g_stored         = v;

    if (v == 0u)
    {
        g_writeIdx = 0;
        g_nextSeq  = 1;
    }
    else if (v < RECORD_COUNT)
    {
        // Sarmamış: geçerli kayıtlar [0, v) aralığında.
        g_writeIdx = v;
        Record last{};
        if (readRecord(v - 1u, last) && valid(last)) { g_nextSeq = last.seq + 1u; }
    }
    else
    {
        // Dolu halka: `seq`in düştüğü nokta bir sonraki yazma konumudur.
        g_writeIdx = findRotation();
        const uint32_t newest = (g_writeIdx + RECORD_COUNT - 1u) % RECORD_COUNT;
        Record last{};
        if (readRecord(newest, last) && valid(last)) { g_nextSeq = last.seq + 1u; }
    }

    g_ready = true;
    core::diag::log(core::LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(g_stored),
                    "gecmis deposu hazir");
    return ErrCode::OK;
}

core::ErrCode append(const core::SystemState& snap, bool timeValid)
{
    if (!g_ready) { return ErrCode::STORAGE_FS_MOUNT_FAILED; }

    Record r{};
    r.seq   = g_nextSeq;
    r.epoch = timeValid ? static_cast<uint32_t>(snap.time.epoch.s) : 0u;

    // Saat geçersizken de KAYDEDİLİR ama İŞARETLENİR. Kaydı atlamak kesinti
    // dönemini geçmişte görünmez kılar; sahte zaman damgası basmak ise eski
    // sistemin `"00:00:00"` desenidir ve kabul edilemez.
    r.flags = timeValid ? FLAG_TIME_VALID : 0u;

    const uint8_t sn = (snap.sensors.count <= core::MAX_SENSORS) ? snap.sensors.count
                                                                 : core::MAX_SENSORS;
    for (uint8_t i = 0; i < sn && i < SENSOR_SLOTS; ++i)
    {
        const core::SensorSample& s = snap.sensors.samples[i];
        r.values[i] = scaleFor(s.id, s.value);
        if (s.quality == core::SensorQuality::OK)
        {
            r.qualityMask = static_cast<uint8_t>(r.qualityMask | (1u << i));
        }
    }

    for (uint8_t i = 0; i < core::MAX_ACTUATORS && i < 8u; ++i)
    {
        if (snap.actuators.items[i].isOn != 0u)
        {
            r.actuatorMask = static_cast<uint8_t>(r.actuatorMask | (1u << i));
        }
    }

    r.crc8 = crc8Of(r);

    const ErrCode e = hal::fs::writeAt(FILE_PATH, offsetOf(g_writeIdx), &r, RECORD_BYTES);
    if (e != ErrCode::OK)
    {
        // Sessizce yutulmaz: sayılır ve raporlanır.
        ++g_writeErrors;
        core::diag::raise(e, static_cast<int32_t>(g_writeIdx));
        return e;
    }

    ++g_nextSeq;
    g_writeIdx = (g_writeIdx + 1u) % RECORD_COUNT;   // halka başa döner
    if (g_stored < RECORD_COUNT) { ++g_stored; }

    return ErrCode::OK;
}

uint16_t readRecent(Record* out, uint16_t count)
{
    if (!g_ready || out == nullptr || count == 0u) { return 0u; }
    if (count > MAX_PAGE) { count = MAX_PAGE; }
    if (count > g_stored) { count = static_cast<uint16_t>(g_stored); }
    if (count == 0u) { return 0u; }

    // TOPLU OKUMA: `readAt` her çağrıda dosyayı AÇIP KAPATIYOR. Kayıt başına
    // bir çağrı, 240 kayıtlık bir sayfa için 240 dosya açması demekti —
    // AsyncTCP bağlamı bir yana, `store` task'ında bile kabul edilemez.
    // Halka en fazla İKİ bitişik parçaya bölünür.
    const uint32_t start = (g_writeIdx + RECORD_COUNT - count) % RECORD_COUNT;
    const uint32_t first = (start + count <= RECORD_COUNT)
                               ? count
                               : (RECORD_COUNT - start);

    if (readSpan(start, static_cast<uint16_t>(first), out) != first) { return 0u; }
    if (first < count)
    {
        const uint16_t rest = static_cast<uint16_t>(count - first);
        if (readSpan(0, rest, out + first) != rest) { return static_cast<uint16_t>(first); }
    }

    // Geçerlilik RAM üzerinde denetlenir; bozuk kayıtlar ELENİR ve sorgu
    // çökmez (yarım yazılmış bir kayıt tüm geçmişi okunamaz yapmamalı).
    uint16_t n = 0;
    for (uint16_t i = 0; i < count; ++i)
    {
        if (valid(out[i]))
        {
            if (n != i) { out[n] = out[i]; }
            ++n;
        }
        else if (out[i].seq != 0u) { ++g_corrupt; }
    }
    return n;
}

uint16_t readRange(uint32_t fromEpoch, uint32_t toEpoch, uint16_t skip, Record* out,
                   uint16_t maxCount)
{
    if (!g_ready || out == nullptr || maxCount == 0u) { return 0u; }
    if (maxCount > MAX_PAGE) { maxCount = MAX_PAGE; }

    uint16_t n       = 0;
    uint16_t skipped = 0;

    // En eskiden yeniye tara. `g_stored` sınırlı olduğu için bu döngü
    // en fazla 20 480 okuma yapabilir — bu yüzden `readRange` YALNIZCA
    // `store` task'ından çağrılmalıdır (ISSUE kaydı: TASK-059).
    for (uint32_t k = g_stored; k > 0u && n < maxCount; --k)
    {
        const uint32_t idx = (g_writeIdx + RECORD_COUNT - k) % RECORD_COUNT;
        Record         r{};
        if (!readRecord(idx, r) || !valid(r)) { continue; }

        if (r.epoch < fromEpoch || r.epoch > toEpoch) { continue; }
        if (skipped < skip) { ++skipped; continue; }

        out[n++] = r;
    }
    return n;
}

uint32_t totalWritten()  { return (g_nextSeq > 0u) ? (g_nextSeq - 1u) : 0u; }
uint32_t storedCount()   { return g_stored; }
uint32_t corruptCount()  { return g_corrupt; }
uint32_t writeErrors()   { return g_writeErrors; }

} // namespace history
} // namespace services
