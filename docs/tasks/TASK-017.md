# TASK-017 — GPIO Safe State & RelayOutput Driver

**Phase:** 4 — Hardware Abstraction · **Priority:** **P0 (en kritik HAL task'ı)**

## Objective

Rölelerin **boot'un ilk milisaniyelerinden itibaren güvenli konumda** olmasını garanti
etmek ve röle sürüşünü tek bir kapı arkasına almak. Bu task, kuru çalışma riskine karşı
ilk savunma hattıdır.

## Scope

- Röle pinlerinin boot'ta güvenli seviyeye alınması (Boot Aşama 1)
- `RelayOutput` sürücüsü: aktif seviye yapılandırması (aktif-yüksek / aktif-düşük)
- Gerçek pin durumunun okunması
- Toplu güvenli duruma alma (`allSafe()`)
- Aktif seviyenin donanımda ölçümle doğrulanması (ISSUE-003)

## Out of Scope

- Aktüatör iş kuralları: min/max süre, cooldown, tahkim (TASK-028, TASK-029)
- Güvenlik kilitleri (TASK-030)
- Web/UI'dan kontrol

## Dependencies

- TASK-004, TASK-014

## Requirements

- `REQUIREMENTS.md` — §4 (röle kontrolü), §11-Critical (safe state yok)

## Architecture References

- §2.15 RelayOutput · §7.1 Aşama 1 · §12.2 Boot güvenliği

## Expected Design

### Karar gerektiren nokta — Boot anı güvenliği (ISSUE-003)

```text
Problem:      ESP32 GPIO'ları reset sonrası ve bootloader sırasında belirsiz durumdadır.
              Aktif-düşük röle modülünde bu, pompanın açılması demektir.
Constraints:  Kuru çalışma pompayı kalıcı olarak bozabilir;
              yazılım en erken setup() başında müdahale edebilir
Approaches:   (a) yalnızca yazılımda erken pinMode + güvenli seviye
              (b) donanımsal pull-up/pull-down ile röleyi pasif tutmak
              (c) ikisi birlikte
Trade-offs:   (a) bootloader süresi boyunca (yüzlerce ms) koruma sağlamaz
Recommended:  (c) — donanım tarafı ISSUE-003 kapsamında doğrulanmalı;
              yazılım tarafı bu task'ta zorunlu
```

**Ölçümle doğrulama zorunludur.** Röle modülünün aktif seviyesi tahmin edilmez;
multimetre veya osiloskopla boot anındaki davranış ölçülür ve `docs/HARDWARE.md`'ye yazılır.

### Sürücü kuralları

- Sürücü **hiçbir iş kuralı içermez** (§1.2 D6): süre, eşik, cooldown burada yoktur.
- Aktif seviye yapılandırılabilir olmalı; donanım değişince kod değil config değişir.
- Mantıksal aktüatör kimliği ile fiziksel pin eşlemesi bu katmanda değil, TASK-028'de.

## Implementation Notes

- Güvenli seviyeye alma **`pinMode` çağrısından önce** çıkış seviyesini yazmayı gerektirebilir;
  aksi halde pin kısa süre yanlış seviyede kalabilir. Sıra donanımda doğrulanmalı.
- Strapping pin'leri (0, 2, 12, 15) röle için kullanılmamalı — boot davranışını etkiler.
- Röle durumu okunurken **çıkış pininin gerçek seviyesi** okunmalı; yazılım tarafında
  tutulan gölge değişkene güvenilmemeli. İkisi arasında fark varsa bu bir hatadır.
- Deep sleep / reset senaryolarında GPIO tutma (hold) davranışı değerlendirilmeli.
- Toplu güvenli duruma alma, acil durum yolundan çağrılacağı için **hızlı ve
  bloklamayan** olmalı.

## Files

- `src/hal/RelayOutput.h` / `.cpp` (yeni)
- `src/core/BoardPins.h` (güncelleme)
- `docs/HARDWARE.md` (güncelleme — ölçüm sonuçları)

## Acceptance Criteria

- [ ] Röle aktif seviyesi ölçümle doğrulandı ve belgelendi (ISSUE-003 kapandı)
- [ ] Boot'ta röleler güvenli konumda; ölçümle kanıtlandı
- [ ] Aktif seviye yapılandırılabilir
- [ ] Gerçek pin durumu okunabiliyor
- [ ] `allSafe()` hızlı ve bloklamıyor
- [ ] Sürücüde hiçbir iş kuralı yok
- [ ] Röle pinleri strapping pin değil

## Test Plan

- [ ] **Boot anı ölçümü:** güç verildiği andan itibaren röle çıkışı osiloskop/multimetre ile izlendi; hiçbir anda aktif olmuyor
- [ ] Yazılımsal reset sonrası aynı ölçüm tekrarlandı
- [ ] WDT reset sonrası aynı ölçüm tekrarlandı
- [ ] Aç/kapa komutları fiziksel röleyi doğru sürüyor
- [ ] Okunan durum ile yazılan durum her zaman tutarlı
- [ ] `allSafe()` süresi ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.15, §7.1)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — `allSafe()` acil durum yolunda
- [ ] Shared state güvenli mi? — yalnızca app_core sürüyor mu
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — pin durumu tutarsızlığı tespit ediliyor mu
- [ ] ESP32 resource kullanımı uygun mu? — strapping pin kullanılmamış mı
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `digitalWrite(RELAY1, LOW)` varsayımı sorgulanmalı

## Definition of Done

Ortak DoD + **boot anı röle davranışı üç reset tipinde de ölçümle kanıtlandı** +
sonuçlar `docs/HARDWARE.md`'de yazılı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — `activeLow` config'te OLAMAZ (TASK-014'te bulunan tasarım hatası)

```text
TASK-014'te `ActuatorConfig.activeLow` alani tanimlanmisti.
TASK-017 tasariminda bunun CALISMADIGI ortaya cikti:

  Boot sirasi (ARCHITECTURE §7.1):
     Asama 1  GPIO GUVENLI SEVIYE   ← roleler burada kapatilmali
     Asama 3  CONFIG_LOAD           ← activeLow ANCAK BURADA bilinir

  Asama 1'de hangi seviyenin "kapali" oldugu BILINMIYOR olurdu.
  Iki asama arasinda role durumu TANIMSIZ kalirdi — tam olarak
  onlemeye calistigimiz sey.

Selected: `activeLow` DERLEME ZAMANI sabiti olur (BoardPins.h).
Gerekce:  Role modulunun polaritesi FIZIKSEL KABLOLAMANIN ozelligidir,
          calisma zamani tercihi degil. Config'te tutmak, guvenli seviyenin
          bilinmedigi bir pencere yaratir.
Duzeltme: `ActuatorConfig.activeLow` alani KALDIRILDI (P7 — kullanilmayan alan).
```

Bu, TASK-014'ün geriye dönük düzeltilmesini gerektirdi; aynı batch içinde
olduğu için hemen yapıldı ve TASK-014 kaydına eklendi.

## Karar 2 — Glitch'siz güvenli seviye: önce seviye, sonra çıkış modu

```text
Problem:      pinMode(OUTPUT) once cagrilirsa, pin bir an ONCEKI (tanimsiz)
              seviyeyi SURER — aktif-dusuk modulde bu role ON demektir.
Selected:     Sira TERS:
                 1) digitalWrite(pin, guvenliSeviye)   → cikis yazmacini kur
                 2) pinMode(pin, OUTPUT)               → suruculeri etkinlestir
              Boylece surucu etkinlestigi ANDA dogru seviye zaten yazmactadir.
```

## Karar 3 — ISSUE-003: bu boşluk yazılımla KAPATILAMAZ

```text
GERCEK: ESP32 GPIO'lari reset'ten sonra ve bootloader boyunca (yuzlerce ms)
        giris/yuksek empedans durumundadir. Yazilim en erken setup() basinda
        mudahale edebilir.

        Aktif-dusuk bir role modulunde, giris pini bosta/LOW iken
        ROLE CEKILI olabilir → POMPA CALISIR.

Sonuc:  Bu pencere YAZILIMLA kapatilamaz. Cozum DONANIMDADIR:
          · aktif-dusuk modul icin role girisine HARICI PULL-UP
          · veya aktif-yuksek modul icin harici PULL-DOWN

Selected: Yazilim tarafi elinden geleni yapar (Asama 1, glitch'siz sira) ve
          donanim gereksinimi `docs/HARDWARE.md`'ye ZORUNLU madde olarak yazilir.
DURUST NOT: Bu task'in "olcumle dogrula" kriteri DONANIM ERISIMI GEREKTIRIR
          ve yapilamadi. Varsayilan `activeLow = true` secildi (bu modullerde
          yaygin olan) ama DOGRULANMAMIS bir varsayimdir ve kodda oyle isaretli.
```

## Karar 4 — Gerçek pin durumu okunur, gölge değişken değil

`digitalRead()` ile **fiziksel** çıkış seviyesi okunur. Yazılımda tutulan
gölge değişkene güvenilmez: ikisi arasındaki fark bir donanım veya yazılım
hatasının göstergesidir ve `ActuatorManager` (TASK-029) bunu
`ACTUATOR_STATE_MISMATCH` olarak raporlayacaktır.

## Karar 5 — `allSafe()` kısıt tanımaz

Acil durum yolundan çağrılır (TASK-012 `enterSafeOutputs`). Hızlı, bloklamayan
ve **`minRunTime` gibi kısıtları uygulamayan** bir yoldur. Kısıtlar
`ActuatorManager`'ın işidir; acil durumda geçersizdir.

## Kapsam dışı

- Aktüatör kısıtları, tahkim → TASK-028, TASK-029
- Güvenlik kilitleri → TASK-030

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [ ] **Röle aktif seviyesi ölçümle doğrulandı — YAPILAMADI (donanım erişimi yok).**
      `RELAY_ACTIVE_LOW = true` seçildi (yaygın optokuplörlü modüllerde geçerli)
      ve kodda **doğrulanmamış varsayım** olarak açıkça işaretlendi.
      ISSUE-003 **AÇIK KALIYOR**.
- [ ] **Boot'ta röleler güvenli konumda — ölçümle kanıtlanamadı** (aynı neden)
- [x] Aktif seviye yapılandırılabilir — **derleme zamanı sabiti olarak**
      (çalışma zamanı config olarak DEĞİL; gerekçe aşağıda)
- [x] Gerçek pin durumu okunuyor (`digitalRead`, gölge değişken yok)
- [x] `allSafe()` hızlı, bloklamayan, kısıt tanımayan
- [x] Sürücüde hiçbir iş kuralı yok (D6)
- [x] Röle pinleri strapping pin değil — `BoardPins.h` `static_assert`'i zorluyor

## TASK-014'te bulduğum tasarım hatası ve düzeltmesi

TASK-014'te `ActuatorConfig.activeLow` alanı tanımlanmıştı. Bu task'ın
tasarımında **çalışmayacağı** ortaya çıktı:

```text
Boot sirasi (ARCHITECTURE §7.1):
   Asama 1  GPIO GUVENLI SEVIYE   ← roleler burada kapatilmali
   Asama 3  CONFIG_LOAD           ← activeLow ANCAK BURADA bilinir
```

Aşama 1'de hangi seviyenin "kapalı" olduğu **bilinmiyor** olurdu; iki aşama
arasında röle durumu tanımsız kalırdı — tam olarak önlemeye çalıştığımız şey.

**Düzeltme:** `activeLow` `BoardPins.h`'a derleme zamanı sabiti olarak taşındı,
`ActuatorConfig`'ten kaldırıldı (P7). Röle polaritesi fiziksel kablolamanın
özelliğidir; çalışma zamanı tercihi değildir. TASK-014 ve TASK-015 aynı batch
içinde olduğu için düzeltme geriye dönük uygulandı.

## Dürüst değerlendirme: ISSUE-003 kapatılamadı

Bu task'ın Definition of Done'ı **ölçüm gerektiriyor** ve donanım erişimi yok.
Yapabildiğim ve yapamadığım net:

| Yapıldı | Yapılamadı |
|---|---|
| Glitch'siz sıra (önce seviye, sonra `pinMode`) | Röle modülünün aktif seviyesinin ölçülmesi |
| Polaritenin boot Aşama 1'de bilinir olması | Boot anı röle davranışının osiloskopla izlenmesi |
| Donanım gereksiniminin belgelenmesi | Üç reset tipinde tekrarlanan ölçüm |

Ayrıca şunu tespit ettim: **bu pencere yazılımla kapatılamaz.** ESP32 GPIO'ları
bootloader boyunca yüksek empedanstadır ve yazılım en erken `setup()` başında
müdahale edebilir. Çözüm donanımdadır — röle giriş hattına harici pull-up
(aktif-düşük modül için) veya pull-down. `docs/HARDWARE.md` §8'e **zorunlu
madde** olarak eklendi.

Bu, "yazılımda elimizden geleni yaptık" ile "sorun çözüldü" arasındaki farkın
kayda geçmesi için önemli.

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı
- [x] Pin planı `static_assert`'lerinden geçiyor (strapping değil, güvenli çıkış)
- [ ] **Boot anı ölçümü — donanım gerekiyor**
- [ ] **Yazılımsal / WDT reset sonrası ölçüm — donanım gerekiyor**
- [ ] **Aç/kapa fiziksel doğrulaması — donanım gerekiyor**
- [ ] **`allSafe()` süresi — donanım gerekiyor**

## Review Checklist

- [x] Architecture'a uygun mu? — §2.15, §7.1 Aşama 1, §12.2 boot güvenliği
- [x] Gereksiz abstraction var mı? — düz fonksiyonlar + sabit eşleme tablosu
- [x] Blocking işlem var mı? — `allSafe()` yalnızca `digitalWrite` döngüsü
- [x] Shared state güvenli mi? — yalnızca `ActuatorManager`/`app_core` sürer;
      `allSafe()` acil durum yolundan da çağrılabilir (idempotent)
- [x] Memory problemi var mı? — sabit tablo, heap yok
- [x] Error handling var mı? — eşlenmemiş kimlik ve hazır olmayan sürücü
      reddediliyor; `begin()` öncesi röle sürülemiyor
- [x] ESP32 resource kullanımı uygun mu? — strapping pin kullanılmıyor
- [x] Task sorumluluğu doğru mu? — donanıma tek kapı (P2)
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Mevcut sistemdeki
      `digitalWrite(RELAY1, LOW)` varsayımı **sorgulandı**: LOW'un "kapalı"
      olduğu varsayımı yalnızca aktif-yüksek modüller için doğrudur ve mevcut
      kodda hiç doğrulanmamıştı.

## Durum

**TASK-017: KOŞULLU TAMAMLANDI** — yazılım tarafı bitti; **ISSUE-003 açık**,
ölçüm ve donanım değişikliği bekliyor. Bu, sahaya çıkmadan önce kapatılması
gereken bir maddedir.
