# Topraksız Tarım Sistemi

## Implementation Plan

> **Doküman zinciri**
> `REQUIREMENTS.md` (**NE**) → `ARCHITECTURE.md` (**NASIL**) → **`IMPLEMENTATION_PLAN.md` (SIRA)** → `docs/tasks/TASK-XXX.md` (**İŞ**)
>
> Bu doküman kod içermez. Yeni mimariyi 65 bağımsız, test edilebilir task'a böler,
> bağımlılıklarını çıkarır ve uygulama sırasını belirler.
>
> **Toplam:** 16 faz · 65 task
> **Referans:** `REQUIREMENTS.md` (2026-08-30) · `ARCHITECTURE.md`

---

## 1. Temel Kural — Eski Kodun Statüsü

Mevcut `src/` ve `include/` klasörleri **yalnızca dört amaçla** okunur:

1. Sistemin hangi davranışlara sahip olduğunu görmek
2. Donanım bağlantılarını ve pin kullanımını anlamak
3. Geçmişte yaşanan problemleri tespit etmek
4. Kullanıcı akışlarını (menü gezinme, Wi-Fi kurulum adımları) anlamak

**Kesinlikle yapılmayacaklar:**

| Yasak | Gerekçe |
|---|---|
| Eski sınıfı yeni projede yeniden oluşturmak | `MyWiFi`, `GrowPlantClass`, `SensorClass` kaldırıldı — yerine `ARCHITECTURE.md` §2 modülleri gelir |
| Eski fonksiyonu birebir taşımak | Örn. `connect(5000)` bloklayan bir desendir; taşınmaz |
| "Çalışıyor" diye eski algoritmayı korumak | Akış hesabı `(pulses*100)/450` sabit periyot varsayar — yeniden tasarlanır |
| Eski global değişkenleri kullanmak | `currentIP`, `currentMAC`, `Sensor.WaterFlow` → `StateStore`'a taşınır |
| Eski task yapısını kopyalamak | 4 task → 5 task, farklı sorumluluk ve öncelik dağılımı |
| Eski bug'ı taşımak | GPIO 34'te `INPUT_PULLUP`, `1.5` detent oranı, ters WDT init sırası |

**Her task'ın Review Checklist'inde "eski kod gereksiz şekilde kopyalanmış mı?" maddesi
bulunur ve bu madde işaretlenmeden task DONE olmaz.**

---

## 2. Task Çalışma Protokolü

Her task **üç adımda** yürütülür. Adımlar atlanamaz.

```text
  ┌──────────────────────────────────────────────────────────────┐
  │  STEP 1 — DESIGN                                             │
  │                                                              │
  │   · Task dosyasını oku, Requirements + Architecture ref.       │
  │     bölümlerindeki kaynak bölümleri gözden geçir              │
  │   · Tasarım kararı gerektiren noktaları listele                │
  │   · Alternatifleri değerlendir:                               │
  │                                                              │
  │        Problem → Constraints → Possible approaches            │
  │             → Trade-offs → Selected approach                  │
  │                                                              │
  │   · Seçimi ve gerekçesini task dosyasına yaz                  │
  │   ÇIKTI: yazılı tasarım kararı — henüz kod yok               │
  └──────────────────────────────┬───────────────────────────────┘
                                 ▼
  ┌──────────────────────────────────────────────────────────────┐
  │  STEP 2 — IMPLEMENT                                          │
  │                                                              │
  │   · YALNIZCA bu task'ın Scope bölümündeki işi yap             │
  │   · Out of Scope listesindeki hiçbir şeye dokunma             │
  │   · Files bölümünde listelenmemiş dosyayı değiştirme          │
  │   ÇIKTI: derlenen, scope'la sınırlı kod                       │
  └──────────────────────────────┬───────────────────────────────┘
                                 ▼
  ┌──────────────────────────────────────────────────────────────┐
  │  STEP 3 — REVIEW                                             │
  │                                                              │
  │   · Review Checklist'in tamamını işaretle                     │
  │   · Acceptance Criteria'nın tamamını doğrula                  │
  │   · Test Plan'i çalıştır, sonucu dürüstçe raporla             │
  │   ÇIKTI: DONE veya eksik listesi                              │
  └──────────────────────────────────────────────────────────────┘
```

### 2.1 Tasarım özgürlüğü

Task dosyaları **ne yapılacağını** ve **hangi kısıtlara uyulacağını** söyler;
**nasıl kodlanacağını** dikte etmez. Geliştirici daha iyi bir algoritma, daha uygun bir
FreeRTOS mekanizması veya daha temiz bir state modeli seçebilir — **tek şart, kararı
gerekçesiyle yazmasıdır.**

Eski kodun yaklaşımı hiçbir zaman "varsayılan çözüm" değildir.

### 2.2 Scope Creep Protokolü

Bir task sırasında kapsam dışı bir problem fark edilirse **o problem çözülmez**.
`docs/ISSUES.md` dosyasına şu formatta kaydedilir:

```text
ISSUE-XXX
Found during: TASK-XXX
Severity:     P0 | P1 | P2 | P3
Description:  ...
Impact:       ...
Recommended:  yeni TASK-XXX önerisi veya mevcut TASK-XXX'e ekleme
```

Sonra mevcut task'a kalınan yerden devam edilir. Issue, faz sonunda triyaj edilir.

### 2.3 Definition of Done (tüm task'lar için geçerli taban)

Bir task **yalnızca aşağıdakilerin tamamı sağlandığında** DONE sayılır:

- [ ] **Code** — Scope içindeki iş tamamlandı, Out of Scope'a dokunulmadı
- [ ] **Build** — `pio run` uyarısız derleniyor (yeni uyarı üretilmedi)
- [ ] **Test** — Task'ın Test Plan'i çalıştırıldı, sonuçlar raporlandı
- [ ] **Review** — Review Checklist'in tamamı işaretlendi
- [ ] **Acceptance** — Acceptance Criteria'nın tamamı doğrulandı
- [ ] **Design record** — STEP 1'deki tasarım kararı yazılı olarak task dosyasında
- [ ] **No dead code** — Bildirilip implement edilmemiş fonksiyon, kullanılmayan alan yok (ARCHITECTURE P7)

> "Kod yazıldı" tek başına **DONE değildir.**

---

## 3. Öncelik Tanımları

| Seviye | Anlam | Kapsam |
|---|---|---|
| **P0 — Critical** | Olmadan sistem **güvensiz** veya var olamaz | Safe state, pompa koruması, sensör doğrulama, shared state senkronizasyonu, watchdog, config bütünlüğü, çekirdek altyapı |
| **P1 — High** | Sistemin **temel işlevi** için gerekli | Ağ kararlılığı, kimlik doğrulama, state senkronizasyonu, otomasyon motoru |
| **P2 — Medium** | İşlevsel tamlık | Display ekranları, tarama, frontend görünümleri, zaman servisi |
| **P3 — Low** | İyileştirme | Geçmiş veri, raporlama, kozmetik |

**ARCHITECTURE §21 gereği güvenlik zinciri (PHASE 6) otomasyondan (PHASE 12) önce
tamamlanır.** Bu, planın en önemli sıralama kuralıdır.

---

## 4. Fazlar ve Task Listesi

### PHASE 0 — Project Foundation

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-001 | Project Skeleton & Layer Structure | P0 | — |
| TASK-002 | Build Configuration & Partition Layout | P0 | TASK-001 |
| TASK-003 | Coding Standards & Layering Rules | P1 | TASK-001 |

### PHASE 1 — Core Infrastructure

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-004 | Core Types, Result & Error Model | P0 | TASK-001 |
| TASK-005 | Diagnostics & Logging | P0 | TASK-004 |
| TASK-006 | SystemState Model | P0 | TASK-004 |
| TASK-007 | StateStore | P0 | TASK-006 |
| TASK-008 | Command Model & CommandQueue | P0 | TASK-004 |

### PHASE 2 — Boot & Task Framework

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-009 | WatchdogGuard | P0 | TASK-004, TASK-005 |
| TASK-010 | Staged Boot Framework & Boot Report | P0 | TASK-005, TASK-009 |
| TASK-011 | Task Framework & Heartbeat | P0 | TASK-009 |
| TASK-012 | SystemSupervisor & Mode Machine | P0 | TASK-007, TASK-011 |

### PHASE 3 — Storage & Configuration

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-013 | NvsStore & Secret Store | P0 | TASK-004, TASK-005 |
| TASK-014 | Config Schema, Defaults & Validation | P0 | TASK-004 |
| TASK-015 | ConfigService (Load, Migrate, Persist) | P0 | TASK-013, TASK-014 |
| TASK-016 | FileStore (LittleFS) | P1 | TASK-004, TASK-005 |

### PHASE 4 — Hardware Abstraction (HAL)

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-017 | GPIO Safe State & RelayOutput Driver | **P0** | TASK-004, TASK-014 |
| TASK-018 | AdcInput Driver | P1 | TASK-004 |
| TASK-019 | PulseCounter Driver (PCNT) | P1 | TASK-004 |
| TASK-020 | OledPanel Driver | P2 | TASK-004 |
| TASK-021 | Input Devices (Encoder & Buttons) | P2 | TASK-004, TASK-008 |

### PHASE 5 — Sensor System

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-022 | Sensor Model, Quality & Registry | P0 | TASK-004, TASK-006 |
| TASK-023 | Sensor Processing Pipeline | P0 | TASK-022, TASK-014 |
| TASK-024 | Analog Sensors (Water Temp, pH, EC) | P1 | TASK-018, TASK-023 |
| TASK-025 | Flow Sensor | P1 | TASK-019, TASK-023 |
| TASK-026 | Water Level Sensor (Safety-Critical) | **P0** | TASK-023 |
| TASK-027 | SensorService & io_sense Task | P0 | TASK-024, TASK-025, TASK-026, TASK-011 |

### PHASE 6 — Safety & Actuator System

> **Bu faz otomasyondan önce tamamlanmalıdır.** Güvenlik zinciri kurulmadan hiçbir
> otomatik aktüatör kontrolü yazılmaz.

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-028 | Actuator Model & Constraints | **P0** | TASK-004, TASK-014 |
| TASK-029 | ActuatorManager & Arbitration | **P0** | TASK-017, TASK-028 |
| TASK-030 | SafetyMonitor & Interlocks | **P0** | TASK-026, TASK-029 |
| TASK-031 | Flow Verification & Dry-Run Protection | **P0** | TASK-025, TASK-030 |
| TASK-032 | Emergency Stop Latch & Recovery | **P0** | TASK-030, TASK-012 |
| TASK-033 | app_core Task Loop | **P0** | TASK-029..032, TASK-008 |

### PHASE 7 — Network / Wi-Fi

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-034 | WifiRadio Driver & Event Bridge | P1 | TASK-004, TASK-005 |
| TASK-035 | Network State Model & FSM | P1 | TASK-034, TASK-007 |
| TASK-036 | STA Connection Manager & IP Configuration | P1 | TASK-035, TASK-015 |
| TASK-037 | Retry & Backoff Strategy | P1 | TASK-036 |
| TASK-038 | SoftAP & AP Fallback | P1 | TASK-036, TASK-037 |
| TASK-039 | Wi-Fi Scan Service | P2 | TASK-035 |

### PHASE 8 — Time

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-040 | TimeService (SNTP, Timezone, Validity) | P2 | TASK-035, TASK-015 |

### PHASE 9 — Web Backend

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-041 | WebService Skeleton & Static Asset Serving | P1 | TASK-016, TASK-035 |
| TASK-042 | AuthService (Hash, Token, Rate Limit) | P1 | TASK-013, TASK-041 |
| TASK-043 | REST API — State, Diagnostics, System | P2 | TASK-041, TASK-042, TASK-007 |
| TASK-044 | REST API — Config & Network | P1 | TASK-042, TASK-015, TASK-039 |
| TASK-045 | WebSocket Protocol & Command Path | P1 | TASK-042, TASK-008 |
| TASK-046 | Telemetry Publisher & Backpressure | P2 | TASK-045, TASK-007 |

### PHASE 10 — Frontend

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-047 | Asset Pipeline & App Shell | P2 | TASK-041 |
| TASK-048 | State Sync Client (No Optimistic Update) | **P1** | TASK-045, TASK-046, TASK-047 |
| TASK-049 | Dashboard, Control & Settings Views | P2 | TASK-048, TASK-043, TASK-044 |

### PHASE 11 — Display / UI

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-050 | ViewModelBuilder | P2 | TASK-007 |
| TASK-051 | Screen Framework & Navigation | P2 | TASK-050, TASK-021 |
| TASK-052 | Screens (Home, Sensors, Control, Network, System, Alerts) | P2 | TASK-051, TASK-020 |
| TASK-053 | UiService & ui Task Integration | P2 | TASK-052, TASK-011, TASK-008 |

### PHASE 12 — Automation Engine

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-054 | Rule Model & Config Schema | P1 | TASK-014, TASK-028 |
| TASK-055 | Threshold Rule Evaluation (Hysteresis) | P1 | TASK-054, TASK-027 |
| TASK-056 | Schedule Rule Evaluation (Time Validity) | P2 | TASK-054, TASK-040 |
| TASK-057 | AutomationEngine, Mode & Manual Override | P1 | TASK-055, TASK-056, TASK-033 |

### PHASE 13 — Data Logging / History

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-058 | Ring-File History Store | P3 | TASK-016 |
| TASK-059 | StorageService Task & History API | P3 | TASK-058, TASK-043 |

### PHASE 14 — Integration & Hardening

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-060 | System Integration Bring-Up | **P0** | TASK-033, TASK-038, TASK-053, TASK-057 |
| TASK-061 | Failure Injection & Degraded Mode Verification | **P0** | TASK-060 |
| TASK-062 | Resource & Stability Profiling | P1 | TASK-060 |
| TASK-063 | Security Hardening Pass | P1 | TASK-060, TASK-042 |

### PHASE 15 — Testing & Final Review

| Task | Ad | Öncelik | Bağımlılık |
|---|---|---|---|
| TASK-064 | Host Test Harness & Domain Tests | P1 | TASK-030, TASK-057 |
| TASK-065 | Hardware-in-the-Loop Tests & Final Conformance Review | P1 | TASK-061, TASK-064 |

---

## 5. Bağımlılık Grafiği

### 5.1 Faz seviyesinde

```text
                      PHASE 0  Foundation
                            │
                      PHASE 1  Core Infrastructure
                            │
                      PHASE 2  Boot & Task Framework
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
      PHASE 3 Storage/Config       PHASE 4 HAL
              │                           │
              └─────────────┬─────────────┘
                            ▼
                    PHASE 5  Sensors
                            │
                            ▼
              PHASE 6  SAFETY & ACTUATORS   ◀── kritik kilit noktası
                            │                   (otomasyondan ÖNCE)
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
      PHASE 7 Network  PHASE 11 Display  PHASE 12 Automation
              │             │              (P7'ye bağlı DEĞİL)
              ▼             │
      PHASE 8 Time          │
              │             │
              ▼             │
      PHASE 9 Web Backend   │
              │             │
              ▼             │
      PHASE 10 Frontend     │
              │             │
              └──────┬──────┴──── PHASE 13 History
                     ▼
             PHASE 14  Integration & Hardening
                     │
                     ▼
             PHASE 15  Testing & Final Review
```

### 5.2 Kritik yol (critical path)

Projenin en uzun zorunlu zinciri. Bu zincirdeki her gecikme projeyi doğrudan geciktirir:

```text
TASK-001  Skeleton
   ↓
TASK-004  Core Types
   ↓
TASK-006  SystemState  →  TASK-007  StateStore
   ↓
TASK-011  Task Framework
   ↓
TASK-014  Config Schema  →  TASK-015  ConfigService
   ↓
TASK-017  RelayOutput (safe state)
   ↓
TASK-023  Sensor Pipeline  →  TASK-026  Water Level Sensor
   ↓
TASK-029  ActuatorManager
   ↓
TASK-030  SafetyMonitor
   ↓
TASK-031  Flow Verification  →  TASK-032  Emergency Stop
   ↓
TASK-033  app_core Task Loop
   ↓
TASK-057  AutomationEngine
   ↓
TASK-060  Integration  →  TASK-061  Failure Injection
   ↓
TASK-065  Final Review
```

### 5.3 Paralel geliştirilebilir kollar

Aşağıdaki gruplar birbirine bağımlı **değildir**; farklı geliştiriciler veya farklı
oturumlar tarafından eşzamanlı yürütülebilir.

| Kol | Task'lar | Önkoşul |
|---|---|---|
| **A — Ağ** | TASK-034 … TASK-040 | PHASE 2 tamamlandıktan sonra; PHASE 5/6'ya bağımlı değil |
| **B — Display** | TASK-050 … TASK-053 | TASK-007 + TASK-020/021; sensör ve ağ tamamlanmadan başlayabilir (ViewModel sahte snapshot ile test edilir) |
| **C — Frontend** | TASK-047 | Sözleşme netleştiği anda (TASK-045 tasarımı); backend tamamlanmadan başlayabilir |
| **D — HAL sürücüleri** | TASK-018, TASK-019, TASK-020, TASK-021 | Birbirinden bağımsız, aynı anda yazılabilir |
| **E — Geçmiş veri** | TASK-058, TASK-059 | TASK-016 sonrası; ana akışı bloklamaz |
| **F — Test altyapısı** | TASK-064 | Domain modülleri hazır oldukça büyütülür |

**Paralellik uyarısı:** Kol A (ağ) ile kritik yol aynı anda ilerletilebilir, ancak
**TASK-060 (entegrasyon) her iki kol da bitmeden başlamaz.**

---

## 6. Kilometre Taşları (Milestones)

| M | Ad | Kapsam | Doğrulama kriteri |
|---|---|---|---|
| **M1** | Ayakta duran iskelet | PHASE 0–2 | Kart boot ediyor, WDT aktif, 5 task oluşuyor, boot raporu seri porttan okunuyor, hiçbir hata sistemi durdurmuyor |
| **M2** | Güvenli donanım tabanı | PHASE 3–4 | Config NVS'ten yükleniyor, boot'ta tüm röleler kapalı, sürücüler tek tek doğrulandı |
| **M3** | Ölçen sistem | PHASE 5 | Tüm sensörler kalite bilgisiyle `StateStore`'a yayınlanıyor, arıza durumları tespit ediliyor |
| **M4** | **Güvenli sistem** | PHASE 6 | Pompa yalnızca güvenlik izniyle çalışıyor; kuru çalışma, seviye ve max süre korumaları donanımda kanıtlandı |
| **M5** | Bağlanan sistem | PHASE 7–8 | Wi-Fi bloklamadan bağlanıyor, kopmada backoff + AP fallback çalışıyor, saat senkron |
| **M6** | Yönetilebilir sistem | PHASE 9–11 | Web ve OLED cihazın gerçek durumunu gösteriyor; komutlar ack'li; iyimser güncelleme yok |
| **M7** | Otomatik sistem | PHASE 12–13 | Kurallar aktüatörleri sürüyor, güvenlik vetosu her durumda kazanıyor |
| **M8** | Sahaya hazır | PHASE 14–15 | Arıza enjeksiyonu testleri geçti, 72 saat kesintisiz çalışma, uyumluluk incelemesi tamam |

> **M4 kapı (gate) noktasıdır:** M4 doğrulanmadan PHASE 12 (otomasyon) başlatılmaz.

---

## 7. Faz Bazlı Risk Notları

| Faz | Risk | Azaltma |
|---|---|---|
| PHASE 4 | Röle modülü aktif-düşükse boot'ta pompa çalışabilir | TASK-017 ilk iş olarak seviye doğrulaması yapar; osiloskop/multimetre ile kanıtlanır |
| PHASE 4/5 | ADC1 pin çakışması (encoder GPIO 32/33'te) | TASK-002 pin planını kilitler; encoder ADC olmayan pinlere taşınır |
| PHASE 5 | GPIO 34–39'da dahili pull-up yok | TASK-025 harici pull-up gereksinimini doğrular |
| PHASE 6 | Su seviyesi sensörü tipi henüz kesin değil | TASK-026 iki şamandıra varsayımıyla tasarlanır, tek analog sensöre düşülürse arıza tespiti genişletilir |
| PHASE 7 | AsyncTCP + Wi-Fi yeniden bağlanmada bellek sızıntısı | TASK-062 uzun süreli heap izlemesi yapar |
| PHASE 9 | AsyncTCP callback'inde bloklama | TASK-041 ve TASK-045 review checklist'inde açık madde |
| PHASE 14 | Entegrasyonda ortaya çıkan zamanlama sorunları | TASK-062 stack watermark ve döngü süresi ölçümü |

---

## 8. Faz Çıkış Kriterleri

Bir faz, **tüm task'ları DONE** ve aşağıdaki ek koşul sağlandığında kapanır:

| Faz | Ek çıkış kriteri |
|---|---|
| PHASE 0–2 | Boot hiçbir koşulda durmuyor; WDT reset nedeni kaydediliyor |
| PHASE 3 | Bozuk/eksik config varsayılana düşüyor ve **loglanıyor** (sessiz sıfırlama yok) |
| PHASE 4 | Her sürücü tek başına donanımda doğrulandı; hiçbiri iş kuralı içermiyor |
| PHASE 5 | Her sensör için 5 arıza senaryosu (kopuk, kısa, donmuş, aralık dışı, sıçrama) test edildi |
| PHASE 6 | **Güvenlik testleri donanımda kanıtlandı** — simülasyon yeterli değil |
| PHASE 7 | 100 kez bağlantı kesme/kurma döngüsünde heap sabit, blocking yok |
| PHASE 9–10 | Sayfa yenilemede arayüz cihazın gerçek durumunu gösteriyor |
| PHASE 12 | Otomasyon aktifken güvenlik vetosu test edildi |
| PHASE 14–15 | 72 saat kesintisiz çalışma; WDT reset yok; heap sızıntısı yok |

---

## 9. Task Dosyaları

Tüm task tanımları `docs/tasks/` altındadır:

```text
docs/
├── ISSUES.md              ← scope creep kayıtları
└── tasks/
    ├── TASK-001.md  …  TASK-065.md
```

Her task dosyası standart 13 bölüm içerir: Objective, Scope, Out of Scope, Dependencies,
Requirements, Architecture References, Expected Design, Implementation Notes, Files,
Acceptance Criteria, Test Plan, Review Checklist, Definition of Done.

---

## 10. Uygulama Sırası — Özet

```text
  1. TASK-001 → 003     iskelet, build, standartlar
  2. TASK-004 → 008     core: tipler, log, state, kuyruk
  3. TASK-009 → 012     watchdog, boot, task framework, supervisor
  4. TASK-013 → 016     NVS, config, LittleFS          ┐ paralel
  5. TASK-017 → 021     HAL sürücüleri                 ┘
  6. TASK-022 → 027     sensör sistemi
  7. TASK-028 → 033     GÜVENLİK + AKTÜATÖR   ◀── M4 KAPISI
  8. TASK-034 → 040     ağ + zaman                     ┐ paralel
  9. TASK-050 → 053     display                        ┘
 10. TASK-041 → 046     web backend
 11. TASK-047 → 049     frontend
 12. TASK-054 → 057     otomasyon motoru
 13. TASK-058 → 059     geçmiş veri
 14. TASK-060 → 063     entegrasyon + sağlamlaştırma
 15. TASK-064 → 065     test + final review
```

---

*Bu plan uygulama sırasını ve iş bölümünü tanımlar. Her task'ın detayı kendi dosyasındadır.
Kodlama, ilgili task dosyasının STEP 1 (DESIGN) adımı tamamlanmadan başlamaz.*
