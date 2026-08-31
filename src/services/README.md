# services/ — Donanım Sahibi Servisler (L2)

**Katman:** L2 — Services (ARCHITECTURE §1.1)

## Sorumluluk

Bir donanım kaynağını sahiplenip periyodik yöneten servisler: sensör örnekleme
ve işleme, Wi-Fi durum makinesi, kalıcı depolama, zaman senkronizasyonu,
konfigürasyon yönetimi.

## Bağımlılık kuralı

| Neye bağımlı olabilir | Neye bağımlı OLAMAZ |
|---|---|
| `core/`, `hal/` | `domain/`, `interfaces/` |

**Servisler birbirini doğrudan çağırmaz** (ARCHITECTURE D4). `SensorService`,
`NetworkService`'i tanımaz. Servisler arası koordinasyon `domain/` katmanının işidir;
bilgi paylaşımı `StateStore` üzerinden yapılır.

## Bu klasöre girmeyecekler

- Ekrana çizim
- Aktüatör tetikleme
- Otomasyon veya güvenlik kararı

`SensorService` yalnızca ölçer ve yayınlar. Mevcut sistemdeki en belirgin katman
ihlali (`Sensor.cpp` içinde okuma + OLED çizimi) burada tekrarlanmaz.

## İlgili task'lar

TASK-015, TASK-022 … TASK-027, TASK-035 … TASK-040, TASK-058, TASK-059
