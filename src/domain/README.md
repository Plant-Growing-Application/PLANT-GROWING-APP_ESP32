# domain/ — Karar Mantığı (L3)

**Katman:** L3 — Domain / Application (ARCHITECTURE §1.1)

## Sorumluluk

Sistemin tek karar merkezi: güvenlik kilitleri, aktüatör tahkimi ve kısıtları,
otomasyon kuralları, sistem modu yönetimi.

## Bağımlılık kuralı

| Neye bağımlı olabilir | Neye bağımlı OLAMAZ |
|---|---|
| `core/` | `hal/`, `services/`, `interfaces/` |

## Donanımsız olma kuralı

Bu katman **donanım çağrısı içermez**. Girdisi `StateStore` snapshot'ı ve
`Config`; çıktısı karardır. Tek istisna, `ActuatorManager`'ın röle çıkışına
yazmasıdır — o da `hal/RelayOutput` üzerinden ve yalnızca `app_core` task'ından.

Bu kısıt sayesinde güvenlik kuralları, otomasyon kararları ve state geçişleri
**masaüstünde, ESP32 olmadan test edilebilir** (ARCHITECTURE §17, TASK-064).
Test stratejisinin tamamı bu özelliğe dayanır — ihlal edilmesi test edilebilirliği
yok eder.

## Değerlendirme sırası (değiştirilemez)

```text
SafetyMonitor → AutomationEngine → ActuatorManager
```

Güvenlik her zaman önce gelir (ARCHITECTURE §11.1 adım 3). Otomasyon, güvenlik
değerlendirmesi yapılmamış bir state üzerinde karar veremez.

## İlgili task'lar

TASK-012, TASK-028 … TASK-033, TASK-054 … TASK-057
