# tasks/ — FreeRTOS Task Giriş Noktaları

**Katman:** Çapraz — task iskeletleri ve zamanlama

## Sorumluluk

Beş task'ın giriş fonksiyonları, periyodik döngü iskeleti, çekirdeğe sabitleme,
heartbeat yayını ve watchdog beslemesi.

## Task tablosu (ARCHITECTURE §6.1)

| Task | Periyot | Öncelik | Çekirdek | Sahip olduğu kaynak |
|---|---|---|---|---|
| `app_core` | 100 ms | 4 | 1 | Röle GPIO'ları |
| `io_sense` | 250 ms | 3 | 1 | ADC1, PCNT |
| `net` | 100 ms (olay güdümlü) | 2 | 0 | Wi-Fi radyosu |
| `ui` | 50 ms | 2 | 1 | OLED (I2C), encoder, butonlar |
| `store` | olay güdümlü | 1 | 0 | Flash (NVS + LittleFS) |

## Kurallar

1. Her task **kendi başlangıcında** watchdog'a kaydolur.
2. Döngü sırası her zaman: **iş → heartbeat → watchdog besleme**.
   Besleme asla döngünün ortasında yapılmaz — task'ın gerçekten ilerlediğini
   kanıtlamaz.
3. Task döngüsünde bloklama yoktur (ARCHITECTURE P3): `while(hazır değil)`,
   `delay()`, uzun flash/ağ işlemi yasak.
4. Task'lar birbirini `vTaskSuspend`/`vTaskResume` ile yönetmez.
5. Yeni task açmadan önce ARCHITECTURE §6.4 okunmalı: "her özellik için bir task"
   yaklaşımı kullanılmaz.

## İlgili task'lar

TASK-011, TASK-027, TASK-033, TASK-035, TASK-053, TASK-059
