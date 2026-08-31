#include "FlowSensor.h"

#include <math.h>

#include "core/BoardPins.h"
#include "hal/PulseCounter.h"

namespace services {
namespace sensors {

core::ErrCode FlowSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;
    const core::ErrCode rc = hal::pulse::begin(board::FLOW_PULSE, flow::GLITCH_FILTER_NS);
    _ready                 = (rc == core::ErrCode::OK);
    _totalLiters           = 0.0f;
    return rc;
}

RawSample FlowSensor::sample(const SampleContext& ctx)
{
    (void)ctx;
    if (!_ready)
    {
        return rawFault();
    }

    const core::Result<hal::PulseWindow> r = hal::pulse::readAndReset();
    if (r.failed())
    {
        return rawFault();
    }

    const hal::PulseWindow& w = r.value;

    // Pencere süresi yoksa hesap yapılamaz — bir sonraki örneği bekle.
    // (Eski kodun sabit periyot varsayımı tam olarak burada kırılıyordu.)
    if (w.elapsed.ms == 0u)
    {
        return rawOk(0.0f);
    }

    // Taşma: sayım güvenilmez. Sessizce yanlış debi yayınlamak yerine arıza.
    if (w.overflow)
    {
        return rawFault();
    }

    // --- Litre ve debi hesabı — FLOAT aritmetik ---
    //
    // Tamsayı bölmesi düşük debileri sıfıra yuvarlar; kuru çalışma tespiti
    // için en kritik bölge tam da orasıdır.
    const float liters = static_cast<float>(w.pulses) / flow::PULSES_PER_LITER;

    // Debi, GERÇEK geçen süreye bölünür — sabit pencere varsayılmaz.
    const float minutes = static_cast<float>(w.elapsed.ms) / 60000.0f;
    if (!(minutes > 0.0f))
    {
        return rawOk(0.0f);
    }

    const float lpm = liters / minutes;

    if (isnan(lpm) || isinf(lpm) || lpm < 0.0f)
    {
        return rawFault();
    }

    _totalLiters += liters;

    // 0 L/dk BURADA arıza sayılmaz: pompa kapalıyken normal değerdir.
    // "Pompa açık ama akış yok" çapraz kontrolü TASK-031'e aittir.
    return rawOk(lpm);
}

} // namespace sensors
} // namespace services
