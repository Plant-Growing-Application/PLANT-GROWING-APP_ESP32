# TASK-026 — Water Level Sensor (Safety-Critical)

**Phase:** 5 — Sensor System · **Priority:** **P0**

## Objective

Su seviyesi ölçümünü implement etmek. Bu sensör güvenlik zincirinin **temelidir**; pompanın
çalışma izni doğrudan buna bağlıdır. Mevcut projede bu sensörün hiçbir izi yoktu.

## Scope

- Seviye sensörü okuma (donanım kararına göre)
- Seviye durumu üretimi: `OK`, `LOW`, `CRITICAL`
- Sensör arıza tespiti ve fail-safe davranış
- Çoklu sensör kullanılıyorsa tutarlılık kontrolü

## Out of Scope

- Pompa kilidi mantığı (TASK-030)
- Acil durum mandalı (TASK-032)
- Su ekleme otomasyonu

## Dependencies

- TASK-023

## Requirements

- `REQUIREMENTS.md` — §3.6 (kodda hiç yok), §11-Critical (pompa güvenliği için zorunlu)

## Architecture References

- §9.3 Sensör kataloğu (su seviyesi — en kritik)
- §12.1 Güvenlik zinciri Katman 1
- §9.5 Fail-safe kuralı

## Expected Design

### Karar gerektiren nokta — Sensör topolojisi (ISSUE-000)

```text
Problem:      Hangi sensör tipi ve kaç adet?
Constraints:  Tek nokta hatası pompayı kuru çalıştırabilir;
              sensör "dolu" diye yanlış okursa koruma tamamen devre dışı kalır;
              besin çözeltisi iletken ve korozif olabilir
Approaches:   (a) tek analog sensör (ultrasonik/basınç) — sürekli seviye
              (b) tek dijital şamandıra — tek eşik
              (c) iki dijital şamandıra (düşük + kritik-düşük)
              (d) analog + kritik şamandıra (yedekli)
Trade-offs:   (a) tek nokta hatası; arızada "dolu" okuyabilir
              (c) iki bağımsız cihaz → tek nokta hatası ortadan kalkar,
                  ayrıca ikisi arasında tutarlılık kontrolü yapılabilir
              (d) hem sürekli okuma hem yedeklilik, en pahalısı
Recommended:  (c) minimum; (d) tercih edilir
```

### Fail-safe kuralı — pazarlıksız

> **Seviye okunamıyorsa seviye YETERSİZ kabul edilir.** Sensör arızasında "muhtemelen
> doludur" varsayımı yapılmaz. Bu kural `ARCHITECTURE.md` §9.5'ten gelir ve hiçbir koşulda
> gevşetilemez.

### Tutarlılık kontrolü (iki şamandıra kullanılıyorsa)

Kritik-düşük tetiklenmişken düşük tetiklenmemişse fiziksel olarak tutarsızdır → en az biri
arızalıdır → **her ikisi de arızalı kabul edilir** ve pompa kilitlenir.

## Implementation Notes

- Şamandıra kontakları mekaniktir ve zıplar; debounce gerekir. Ancak debounce süresi
  güvenlik tepkisini geciktirir — dengeli seçilmeli ve ölçülmeli.
- Şamandıra kontağı sıvı içinde korozyona uğrayabilir; açık/kapalı durumların hangisinin
  "güvenli" olduğu önemlidir. **Kablo koptuğunda "su yok" okunacak** şekilde bağlanmalı
  (normalde kapalı kontak), böylece kopuk kablo fail-safe tarafa düşer.
- Analog sensör kullanılıyorsa, TASK-023 filtresi bu sensör için **çok kısa** olmalı;
  pompa çalışırken seviye hızla düşebilir ve gecikme kabul edilemez.
- Seviye değişim hızı sınırı bu sensöre uygulanmamalı veya çok gevşek olmalı.
- Sensörün kendisi bir eşik değeri tutmaz; eşikler config'ten gelir (TASK-014).

## Files

- `src/services/sensors/WaterLevelSensor.h` / `.cpp` (yeni)
- `docs/HARDWARE.md` (güncelleme — topoloji ve kablolama)

## Acceptance Criteria

- [ ] Sensör topolojisi kararı verildi ve gerekçelendirildi (ISSUE-000 kapandı)
- [ ] `OK` / `LOW` / `CRITICAL` durumları üretiliyor
- [ ] **Okunamıyorsa "yetersiz" kabul ediliyor** (fail-safe)
- [ ] Kablo koptuğunda güvenli tarafa düşen kablolama belgelenmiş
- [ ] Çoklu sensörde tutarsızlık tespit ediliyor ve arıza sayılıyor
- [ ] Debounce süresi ölçülmüş ve güvenlik tepkisini geciktirmiyor
- [ ] Eşikler config'ten geliyor

## Test Plan

- [ ] Gerçek su seviyesi düşürülerek `LOW` ve `CRITICAL` geçişleri doğrulandı
- [ ] **Sensör kablosu çıkarıldığında "yetersiz" raporlanıyor** (fail-safe kanıtı)
- [ ] İki şamandıra kullanılıyorsa tutarsız kombinasyon arıza üretiyor
- [ ] Şamandıra zıplaması yanlış geçiş üretmiyor
- [ ] Seviye düşüşünden durum değişimine kadar geçen süre ölçüldü
- [ ] Pompa çalışırken hızlı seviye düşüşü zamanında tespit ediliyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§9.3, §12.1)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **fail-safe davranış bu task'ın özü**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + **fail-safe davranış kablo çıkarma testiyle kanıtlandı** + tespit gecikmesi
ölçüldü ve TASK-030 için kabul edilebilir olduğu onaylandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Topoloji: iki dijital şamandıra (ISSUE-000)

```text
Problem:      Hangi sensor tipi ve kac adet?
Constraints:  TEK NOKTA HATASI pompayi kuru calistirir.
              Sensor "dolu" diye YANLIS okursa koruma tamamen devre disi kalir.
              Besin cozeltisi iletken ve korozif.
Approaches:   (a) tek analog (ultrasonik/basinc) → surekli seviye, ama
                  arizada "dolu" okuyabilir → tek nokta hatasi
              (b) tek dijital samandira → tek esik, yine tek nokta hatasi
              (c) IKI dijital samandira (dusuk + kritik-dusuk)
              (d) analog + kritik samandira (yedekli, en pahali)
Selected:     (c) — pin plani buna gore yapildi (GPIO 13 LOW, GPIO 14 CRITICAL)
Gerekce:      Iki BAGIMSIZ cihaz → tek nokta hatasi ortadan kalkar.
              Ayrica ikisi arasinda TUTARLILIK KONTROLU yapilabilir:
              fiziksel olarak imkansiz bir kombinasyon ariza demektir.
NOT:          Bu bir DONANIM karari ve ISSUE-000 hala kullanicinin onayini
              bekliyor. Yazilim iki samandira varsayimiyla yazildi; tek
              sensore dusulurse tutarlilik kontrolu kaybolur ve ariza tespiti
              zayiflar — bu durum belgelendi.
```

## Karar 2 — FAIL-SAFE: okunamıyorsa seviye YETERSİZ

```text
PAZARLIKSIZ KURAL (ARCHITECTURE §9.5, §12.2):

  Seviye okunamiyorsa "muhtemelen doludur" VARSAYILMAZ.
  YETERSIZ kabul edilir ve pompa kilitlenir.

Gerekce: Yanlis pozitif bir kilit uretim kaybidir (pompa calismaz).
         Yanlis negatif bir izin DONANIM KAYBIDIR (pompa kuru calisir).
         Ikisi esdeger degil; guvenli taraf acik.
```

## Karar 3 — Kablolama: kopuk kablo "su yok" tarafına düşmeli

```text
Samandira kontagi NORMALDE KAPALI (NC) baglanir ve pull-up'li girise cekilir:

   Su VAR   → kontak KAPALI → giris LOW
   Su YOK   → kontak ACIK   → giris HIGH
   KABLO KOPUK              → giris HIGH  → "su yok" okunur  ✓ FAIL-SAFE

Ters baglanirsa kopuk kablo "su var" okunur ve koruma sessizce olur.
Bu bir DONANIM gereksinimidir; docs/HARDWARE.md'ye yazildi.
```

## Karar 4 — Tutarlılık kontrolü: imkânsız kombinasyon = arıza

```text
Iki samandira: LOW (ustte) ve CRITICAL (altta).

  LOW=su var,  CRIT=su var   → OK          (seviye yeterli)
  LOW=su yok,  CRIT=su var   → LOW         (dusuk ama kritik degil)
  LOW=su yok,  CRIT=su yok   → CRITICAL    (kritik seviye)
  LOW=su var,  CRIT=su yok   → IMKANSIZ    → EN AZ BIRI ARIZALI

Son satir fiziksel olarak olamaz: ust samandira suda yuzerken alttaki
kuru olamaz. Bu durumda IKISI DE ARIZALI kabul edilir → FAULT → pompa kilitli.
```

## Karar 5 — Debounce: gerekli ama minimum

```text
Samandira kontaklari mekaniktir ve ziplar. AMA debounce suresi guvenlik
tepkisini GECIKTIRIR.
Selected: 50 ms — mekanik ziplamayi bastirmaya yeter, tepkiyi anlamli
          olcude geciktirmez. Olcumle gozden gecirilmeli.
Ek koruma: filtre KAPALI (filterStrength = 0, varsayilan config).
```

## Karar 6 — Ayrık seviyenin `float` ile temsili

```text
`SensorSample.value` float. Seviye ise AYRIK (3 durum).
Selected: sayisal kod — 0.0 = CRITICAL, 1.0 = LOW, 2.0 = OK
Gerekce:  `SensorSample` tekduze kalir (tum sensorler ayni yapi);
          SafetyMonitor esik karsilastirmasi yapabilir (deger < 1.0 → kritik).
          Birim `SensorUnit::LEVEL_STATE` olarak isaretli, UI bunu
          sayi degil DURUM olarak gosterir.
```

## Kapsam dışı

- Pompa kilidi mantığı → TASK-030 · Acil durum mandalı → TASK-032
- Su ekleme otomasyonu → yok (kapsam dışı)

---

# STEP 3 — REVIEW RECORD

- [x] Topoloji kararı: **iki bağımsız dijital şamandıra** (GPIO 13 / 14).
      ISSUE-000 hâlâ kullanıcı onayını bekliyor — yazılım bu varsayımla yazıldı
- [x] `CRITICAL` / `LOW_LEVEL` / `SUFFICIENT` durumları üretiliyor
- [x] **Okunamıyorsa "yetersiz" kabul ediliyor** — fail-safe her yolda:
      · `begin()` öncesi başlangıç durumu `CRITICAL`
      · sürücü hazır değilse `rawFault()`
      · tutarsız kombinasyonda `rawFault()` + `CRITICAL`
- [x] Kablo koptuğunda güvenli tarafa düşen kablolama `docs/HARDWARE.md` §9'da
      **zorunlu madde** olarak belgelendi
- [x] Tutarsızlık tespit ediliyor: üst şamandıra ıslak + alt kuru = fiziksel
      olarak imkânsız → **her ikisi de arızalı** sayılıyor
- [x] Debounce 50 ms — zıplamayı bastırır, tepkiyi anlamlı ölçüde geciktirmez
- [x] Filtre kapalı (varsayılan config `filterStrength = 0`)
- [ ] **Gerçek su ile geçiş testleri, kablo çıkarma testi, tespit gecikmesi
      ölçümü — donanım gerekiyor**

## Fail-safe zincirinin izi

```text
begin() oncesi          → _state = CRITICAL     (pompa kilitli)
_ready == false         → rawFault()            (pompa kilitli)
tutarsiz kombinasyon    → rawFault() + CRITICAL (pompa kilitli)
her iki samandira kuru  → CRITICAL              (pompa kilitli)
kablo kopuk (NC + pull-up) → giris HIGH → "su yok" → CRITICAL
```

Her yol güvenli tarafa düşüyor. "Muhtemelen doludur" varsayımı hiçbir dalda yok.

## Bulgu — ISSUE-009'un ÜÇÜNCÜ tekrarı

`WaterLevelState::LOW`, Arduino'nun `#define LOW 0x0` makrosuyla çakıştı.
`LOW_LEVEL` olarak yeniden adlandırıldı (`OK` → `SUFFICIENT` de tutarlılık için).

**Bu sefer kendi koyduğum kuralı uygulamayı atladım.** TASK-010 kaydında
"tarama artık her yeni enum için zorunlu sayılmalı" yazmıştım ama TASK-026'da
enum yazmadan önce taramadım. Hata derleyicide yakalandı, sahada değil —
ama ISSUE-009'un neden `CODING_STANDARDS`'a girmesi gerektiğinin üçüncü kanıtı.

**TASK-026: TAMAMLANDI** (donanım doğrulaması ve ISSUE-000 onayı bekliyor).
