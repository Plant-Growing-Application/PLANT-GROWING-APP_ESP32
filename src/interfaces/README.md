# interfaces/ — Sunum Katmanı (L4)

**Katman:** L4 — Presentation / Interfaces (ARCHITECTURE §1.1)

## Sorumluluk

Kullanıcı arayüzleri: `web/` (HTTP + WebSocket + API + kimlik doğrulama) ve
`ui/` (OLED ekranları + encoder/buton navigasyonu).

## Bağımlılık kuralı

| Neye bağımlı olabilir | Neye bağımlı OLAMAZ |
|---|---|
| `core/`, gerekli `hal/` sürücüleri (OLED, girdi) | `domain/`, `services/` |

**`web/` ile `ui/` birbirini tanımaz** (ARCHITECTURE D3). İkisi de aynı snapshot'ı
okur, aynı komut kuyruğuna yazar.

## İki yönlü kural (ARCHITECTURE §2.10, §2.11)

```text
OKUMA:  StateStore.snapshot()      — salt okunur
YAZMA:  CommandQueue.post()        — TEK çıkış yolu
```

Arayüzler **durum üretmez**, yalnızca cihazdan gelen durumu gösterir
(ARCHITECTURE P5). Bir arayüz elemanının gösterdiği her aktüatör durumu, cihazda
gerçekten var olan durumdur.

## Bu klasöre girmeyecekler

- Sensör okuma
- Wi-Fi bağlantısı veya mod değiştirme
- Röle sürme
- Kalıcı depolamaya yazma
- Başka bir task'ı `vTaskSuspend`/`vTaskResume` ile yönetme

Mevcut sistemdeki `GrowPlant.cpp` bunların **hepsini** yapıyordu. Yeni tasarımda
arayüz eylemi kendisi yapmaz; komut üretir.

## İlgili task'lar

TASK-041 … TASK-046, TASK-050 … TASK-053
