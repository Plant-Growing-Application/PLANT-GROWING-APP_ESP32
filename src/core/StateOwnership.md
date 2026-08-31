# State Sahiplik Tablosu

> Kaynak: TASK-006 · Dayanak: `ARCHITECTURE.md` §4.1
>
> **TEK YAZAR KURALI (P1):** Her alt-state'in tam olarak **bir** sahip task'ı
> vardır. Başka bir task o alt-state'e yazarsa bu bir katman ihlalidir ve
> inceleme sırasında yakalanmalıdır.

## Sahiplik

| Alt-state | Yapı | **Tek yazar** | Yazan modül | Okuyanlar |
|---|---|---|---|---|
| `system` | `SystemStatus` | `app_core` | SystemSupervisor (TASK-012) | UI, Web |
| `network` | `NetworkStatus` | `net` | NetworkService (TASK-035) | UI, Web, app_core |
| `sensors` | `SensorsStatus` | `io_sense` | SensorService (TASK-027) | app_core, UI, Web |
| `actuators` | `ActuatorsStatus` | `app_core` | ActuatorManager (TASK-029) | UI, Web |
| `safety` | `SafetyStatus` | `app_core` | SafetyMonitor (TASK-030) | UI, Web |
| `automation` | `AutomationStatus` | `app_core` | AutomationEngine (TASK-057) | UI, Web |
| `time` | `TimeStatus` | `net` | TimeService (TASK-040) | UI, Web, app_core |

`version` alanını yalnızca `StateStore` günceller (TASK-007).

## İhlal nasıl aranır

Bir alt-state'e kimin yazdığını bulmak için ilgili `publish*()` çağrısı aranır:

```bash
grep -rn "publishSensors\|publishNetwork\|publishActuators" src/
```

Sonuçta **birden fazla task** görünüyorsa tek yazar kuralı ihlal edilmiştir.

## Merkezî OLMAYAN state (bilinçli — ARCHITECTURE §4.4)

| Durum | Nerede yaşar | Neden merkezî değil |
|---|---|---|
| Aktif ekran, menü imleci | `UiService` içi | Yalnızca `ui` task'ını ilgilendirir |
| Web oturum tablosu | `AuthService` içi | Yalnızca `WebService`'in meselesi |
| Ham sensör örnekleri, filtre geçmişi | `SensorService` içi | Yalnızca işlenmiş sonuç yayınlanır |
| Wi-Fi FSM iç sayaçları | `NetworkFsm` içi | Yayınlanan alt küme `NetworkStatus`'ta |
| **Wi-Fi şifresi** | `SecretStore` (TASK-013) | State'e, log'a, API'ye, OLED'e **asla girmez** |

## Tip sahipliği (ISSUE-010)

| Dosya | İçerik |
|---|---|
| `SystemState.h` | **Yayınlanan** state tipleri: `SensorQuality`, `SensorSample`, `ActuatorId`, `ControlSource`, `NetState`, `SystemMode`, `AutomationMode` |
| TASK-022 | Çalışma tipleri: `SensorDescriptor`, `SensorRegistry`, `ISensor` |
| TASK-028 | Konfigürasyon/komut tipleri: `ActuatorConstraints`, komut sonucu |
| TASK-035 | FSM iç durumu ve geçiş tablosu |

Sonraki task'lar bu tipleri **yeniden tanımlamaz**, include eder.
