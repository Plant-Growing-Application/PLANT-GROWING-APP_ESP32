# TASK-062 — Resource & Stability Profiling

**Phase:** 14 — Integration & Hardening · **Priority:** P1

## Objective

Sistemin kaynak kullanımını ölçmek ve uzun süreli kararlılığını kanıtlamak. Stack
boyutlarını ölçüme dayalı olarak düzeltmek.

## Scope

- Stack watermark ölçümü ve stack boyutlarının düzeltilmesi
- Heap kullanımı ve parçalanma izleme
- Task döngü sürelerinin ve CPU kullanımının ölçümü
- Uzun süreli kararlılık testi (72 saat)
- Flash aşınma tahmininin doğrulanması

## Out of Scope

- Yeni özellik ekleme
- Mimari değişiklik
- Arıza enjeksiyonu (TASK-061)

## Dependencies

- TASK-060

## Requirements

- `REQUIREMENTS.md` — §10 (stack watermark yok), §9 (heap izleme yok)

## Architecture References

- §6.1 Task tablosu (stack sütunu) · §10 Task health monitoring

## Expected Design

### Stack boyutları — tahminden ölçüme

TASK-011'de stack boyutları **tahmin** olarak verilmişti. Bu task'ta:

```text
  1. Her task'ın en kötü durum watermark'ı ölçülür
  2. Ölçüme güvenlik payı eklenir
  3. Stack boyutları düzeltilir (fazlaysa küçültülür, azsa büyütülür)
```

En kötü durum senaryoları özellikle test edilmeli: en derin çağrı zinciri, en büyük
snapshot kopyası, en uzun JSON serileştirmesi.

### Heap parçalanması

ESP32'de uzun süreli çalışmada heap parçalanması ciddi bir sorundur. Ölçülmesi gerekenler:

- Toplam boş heap
- **En büyük ayrılabilir blok** (parçalanma göstergesi — toplam boş alan yeterli olsa bile
  bu düşükse ayırma başarısız olur)
- Zaman içindeki eğilim

### Riskli alanlar

| Alan | Neden riskli |
|---|---|
| Wi-Fi yeniden bağlanma | Her döngüde bellek sızabilir |
| WS istemci bağlan/kopar | İstemci kaynakları temizlenmiyor olabilir |
| JSON serileştirme | Tekrarlanan ayırma/serbest bırakma parçalanma yaratır |
| Tarama sonuçları | Bellek temizlenmiyor olabilir |

## Implementation Notes

- Ölçüm altyapısı kalıcı olmalı: kaynak istatistikleri `/api/diagnostics` üzerinden
  sürekli izlenebilmeli. Yalnızca test sırasında ölçüm yapmak yetersizdir; sahada da
  görünür olmalı.
- 72 saatlik test **gerçekçi yük altında** yapılmalı: web arayüzü açık, sensörler okunuyor,
  otomasyon çalışıyor, periyodik Wi-Fi kesintileri.
- CPU kullanımı task bazında ölçülmeli; hangi task ne kadar harcıyor bilinmeli.
- Döngü sürelerinin **en kötü durumu** kaydedilmeli, ortalaması değil. Güvenlik döngüsünde
  önemli olan en kötü durumdur.
- Flash yazma sayacı izlenmeli ve TASK-058'deki aşınma tahminiyle karşılaştırılmalı.
- Ölçüm sonuçları belgelenmeli; sonraki değişikliklerde referans olacak.

## Files

- `src/core/ResourceMonitor.h` / `.cpp` (yeni)
- `src/tasks/TaskConfig.h` (güncelleme — ölçülmüş stack boyutları)
- `docs/RESOURCE_PROFILE.md` (yeni — ölçüm raporu)

## Acceptance Criteria

- [ ] Her task'ın stack watermark'ı en kötü durumda ölçüldü
- [ ] Stack boyutları ölçüme göre düzeltildi
- [ ] Heap kullanımı ve en büyük blok izleniyor
- [ ] Kaynak istatistikleri API üzerinden erişilebilir
- [ ] Task bazında CPU kullanımı ölçüldü
- [ ] Döngü sürelerinin en kötü durumu kaydedildi
- [ ] **72 saat kesintisiz çalışma tamamlandı**
- [ ] 72 saat boyunca heap eğilimi sabit (sızıntı yok)
- [ ] Heap parçalanması kabul edilebilir sınırda
- [ ] Flash yazma sayısı tahminle uyumlu
- [ ] Ölçümler belgelendi

## Test Plan

- [ ] Her task için en kötü durum senaryosu çalıştırılıp watermark ölçüldü
- [ ] 100 Wi-Fi bağlan/kes döngüsünde heap sabit
- [ ] 100 WS istemci bağlan/kopar döngüsünde heap sabit
- [ ] 1000 API isteği sonrası heap ve en büyük blok ölçüldü
- [ ] 100 tarama sonrası bellek sızıntısı yok
- [ ] 72 saatlik yüklü çalışma: heap, WDT reset, task sağlığı izlendi
- [ ] Düzeltilmiş stack boyutlarıyla taşma olmadığı doğrulandı
- [ ] CPU kullanımı toplamda makul sınırda

## Review Checklist

- [ ] Architecture'a uygun mu? (§6.1)
- [ ] Gereksiz abstraction var mı? — izleme kodu aşırı yük getiriyor mu
- [ ] Blocking işlem var mı? — ölçüm kodu bloklamıyor mu
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — **bu task'ın tamamı**
- [ ] Error handling var mı? — düşük heap durumunda davranış
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı?

## Definition of Done

Ortak DoD + **72 saatlik test tamamlandı, heap sabit, WDT reset yok** + stack boyutları
ölçüme dayalı olarak düzeltildi + rapor yazıldı.

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

**Tarih:** 2026-08-31

## Bulgu: veri toplanıyordu ama HİÇBİR YERDE SUNULMUYORDU

`TaskRunner::endCycle()` stack watermark ve döngü süresi örnekliyor,
`TaskRegistry` saklıyordu. Ama:

```text
uxTaskGetStackHighWaterMark → TaskRunner.cpp:68   ✅ toplaniyor
core::taskreg::health()     → hicbir yerden cagrilmiyor  ❌
```

**Ölçülemeyen bir şey doğrulanamaz.** Bu task'ın ilk gerçek işi veriyi
görünür kılmaktı.

`GET /api/diagnostics` artık her task için: `registered`, `beats`,
`maxLoopUs`, `overruns`, `minStack`, `lastBeat`.

`minStack` **BAYT** cinsindendir — ESP-IDF `uxTaskGetStackHighWaterMark`'ı
vanilla FreeRTOS'tan farklı olarak bayt döndürür (TASK-011'de doğrulanmıştı).

## Statik tahsis tablosu (ölçüm DEĞİL)

| Task | Çekirdek | Öncelik | Stack (bayt) | Periyot |
|---|---|---|---|---|
| `app_core` | 1 | 4 | 4096 | 100 ms |
| `io_sense` | 1 | 3 | 3072 | 250 ms |
| `net` | 0 | 2 | 5120 | 100 ms |
| `ui` | 1 | 2 | 3584 | 50 ms |
| `store` | 0 | 1 | 4096 | olay güdümlü |

Toplam statik RAM (bağlı firmware): **72 208 bayt** (%22). Kalan ~255 KB
heap Wi-Fi (~50 KB), AsyncTCP, WS istemcileri ve ArduinoJson için.

## Flash aşınma tahmini

```text
Gecmis halkasi : 1 kayit/60 sn = 1440 yazma/gun, 24 bayt
                 LittleFS asinma dengelemesi ile 480 KB'a yayilir
                 → slot basina ~gunde 0,07 yazma
Config         : yalnizca DEGISIKLIKTE, 2 sn birlestirme ile
Acil mandal    : yalnizca acil duruma GECISTE
Boot sayaci    : boot basina 1

Kalici yuk: pratikte YALNIZCA gecmis halkasi.
```

**DOĞRULANMADI** — LittleFS'in gerçek aşınma dağılımı ölçülmeli.

## YAPILMAYAN ölçümler

- [ ] Gerçek stack watermark → stack boyutlarının düzeltilmesi
- [ ] Heap kullanımı ve parçalanma izleme (uzun süreli)
- [ ] Task döngü süreleri ve CPU kullanımı
- [ ] 72 saat kararlılık testi
- [ ] Flash aşınma tahmininin doğrulanması

**TASK-062: ENSTRÜMANTASYON TAMAMLANDI, ÖLÇÜMLER YAPILMADI.**
Artık ölçüm *mümkün*; önceki durumda değildi.
