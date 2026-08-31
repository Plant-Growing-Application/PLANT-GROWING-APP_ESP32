# core/ — Cross-cutting Altyapı

**Katman:** Cross-cutting (ARCHITECTURE §1.1)

## Sorumluluk

Sistemin tamamının paylaştığı temel yapı taşları: veri tipleri, hata modeli,
merkezî durum deposu, komut kuyruğu, teşhis/log, konfigürasyon şeması, watchdog.

## Bağımlılık kuralı

| Neye bağımlı olabilir | Neye bağımlı OLAMAZ |
|---|---|
| Standart kütüphane, FreeRTOS temel API'leri | `hal/`, `services/`, `domain/`, `interfaces/` |

`core/` **hiçbir katmana bağımlı değildir** (ARCHITECTURE D5). Bu, döngüsel
bağımlılığı yapısal olarak imkânsız kılar. Buradaki her tip POD olmalıdır —
`StateStore` snapshot deseni bunu gerektirir.

## Bu klasöre girmeyecekler

- Donanım erişimi (GPIO, ADC, I2C, radyo)
- İş kuralı, eşik, zamanlama mantığı
- Ağ veya dosya sistemi çağrısı

## İlgili task'lar

TASK-004 … TASK-010, TASK-014
