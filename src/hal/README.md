# hal/ — Donanım Soyutlama (L1)

**Katman:** L1 — Drivers / HAL (ARCHITECTURE §1.1)

## Sorumluluk

Fiziksel donanımın ince, durumsuz sarmalayıcıları: GPIO/röle çıkışı, ADC girişi,
darbe sayıcı, OLED paneli, encoder, butonlar, NVS, dosya sistemi, Wi-Fi radyosu.

## Bağımlılık kuralı

| Neye bağımlı olabilir | Neye bağımlı OLAMAZ |
|---|---|
| `core/`, ESP-IDF / Arduino donanım API'leri | `services/`, `domain/`, `interfaces/` |

Alt katman üst katmanı **çağıramaz** (ARCHITECTURE D2). Bir sürücü bilgi yukarı
taşıyacaksa dönüş değeriyle taşır; üst katmanı çağırmaz.

## Bu klasöre girmeyecekler

- İş kuralı: eşik, süre, cooldown, kalibrasyon formülü (ARCHITECTURE D6)
- Karar verme ("sensör arızalı", "pompa çalışmalı")
- Birim dönüşümü ve anlamlandırma

Sürücü ham veriyi üretir; anlamlandırmak `services/` katmanının işidir.

## Donanım sahipliği (ARCHITECTURE P2)

Her fiziksel kaynağa **tek bir modül** dokunur ve o modül **tek bir task**'tan
çağrılır. Bu kural sürücü seviyesinde de doğrulanabilir olmalıdır.

## İlgili task'lar

TASK-013, TASK-016 … TASK-021, TASK-034
