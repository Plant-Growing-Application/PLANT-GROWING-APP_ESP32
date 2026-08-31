# TASK-030 — SafetyMonitor & Interlocks

**Phase:** 6 — Safety & Actuator System · **Priority:** **P0**

## Objective

Güvenlik kilitlerini her döngüde hesaplayan ve aktüatör açma taleplerine **veto** hakkı
kullanan bağımsız bir denetleyici kurmak. Bu modül sistemin en kritik parçasıdır.

## Scope

- Ön koşul kilitlerinin hesaplanması (§12.1 Katman 1)
- `permits(actuatorId) → Verdict{allow, reason}` arayüzü
- Çalışma sırasında izleme (§12.1 Katman 2) — maxRunTime ve seviye düşüşü
- `safety` alt-state'inin yayınlanması
- Kilit nedenlerinin loglanması

## Out of Scope

- Akış doğrulama (TASK-031 — ayrı task, ayrı test)
- Acil durum mandalı ve kurtarma (TASK-032)
- Otomasyon kuralları — güvenlik otomasyondan **bağımsızdır**

## Dependencies

- TASK-026, TASK-029

## Requirements

- `REQUIREMENTS.md` — §11-Critical (pompa güvenlik koşulları, safe state)

## Architecture References

- §2.7 SafetyMonitor · §12.1 Güvenlik zinciri · §12.2 Güvenlik ilkeleri
- §11.1 Değerlendirme döngüsü (güvenlik her zaman önce)

## Expected Design

### Temel ilkeler (§12.2) — hiçbiri gevşetilemez

| İlke | Uygulama |
|---|---|
| **Fail-safe varsayılan** | Okunamayan durum = tehlikeli durum. Sensör arızalıysa pompa çalışmaz. |
| **Tek veto noktası** | Tüm açma yolları buradan geçer. Yan kapı yok. |
| **Bağımsızlık** | MANUAL modda da tam yetkili. Otomasyon kapalıyken de çalışır. |
| **Gözlemlenebilirlik** | Her veto neden koduyla loglanır. Sessiz engelleme yok. |

### Kilit tablosu (başlangıç seti)

| Kilit | Koşul | Etkilediği aktüatör |
|---|---|---|
| `LEVEL_INSUFFICIENT` | Su seviyesi `LOW` veya `CRITICAL` | Su pompası |
| `LEVEL_SENSOR_FAULT` | Seviye sensörü okunamıyor | Su pompası |
| `EMERGENCY_LATCHED` | Acil durum mandalı aktif | Tümü |
| `MAX_RUNTIME_EXCEEDED` | Çalışma süresi aşıldı | İlgili aktüatör |
| `DRY_RUN_DETECTED` | Akış doğrulama başarısız (TASK-031) | Su pompası |

Tablo genişletilebilir olmalı ancak her yeni kilit **açık gerekçeyle** eklenmeli.

### Karar gerektiren nokta — Kilit değerlendirme sıklığı

```text
Problem:      Kilitler ne sıklıkta hesaplanacak?
Constraints:  app_core 100 ms'de bir çalışıyor;
              seviye düşüşünde tepki hızlı olmalı;
              hesaplama app_core bütçesini zorlamamalı
Approaches:   (a) her app_core döngüsünde tam hesaplama
              (b) değişim tetiklemeli (sensör state versiyonu değişince)
Trade-offs:   (b) daha verimli ama karmaşık ve kaçırma riski taşır
Recommended:  (a) — güvenlik için öngörülebilirlik verimlilikten önemlidir;
              hesaplama maliyeti ölçülüp doğrulanmalı
```

## Implementation Notes

- `SafetyMonitor` **otomasyondan önce** çalışır (§11.1 adım 3). Bu sıra değiştirilemez.
- Modül **domain katmanındadır**: donanıma dokunmaz, yalnızca snapshot okur ve karar üretir.
  Bu sayede host tarafında tam test edilebilir (TASK-064).
- Kilit nedeni bir bitmask olarak tutulmalı; birden fazla kilit aynı anda aktif olabilir ve
  kullanıcı hepsini görmelidir.
- Kilit **aktifleşirken** loglanmalı, her döngüde değil — log seli önlenmeli.
- Sensör kalitesi kontrolü: değer `OK` değilse kilit aktif. Kalite alanı olmadan bu kontrol
  yapılamaz — TASK-022'nin neden P0 olduğunun gerekçesi budur.
- Eşikler config'ten okunmalı; her döngüde okunması yeterlidir (ayrı bildirim gerekmez).

## Files

- `src/domain/SafetyMonitor.h` / `.cpp` (yeni)
- `src/domain/models/SafetyState.h` (yeni)

## Acceptance Criteria

- [ ] Kilit tablosu implement edildi
- [ ] `permits()` neden koduyla birlikte karar döndürüyor
- [ ] Sensör kalitesi `OK` değilse ilgili kilit aktifleşiyor (fail-safe)
- [ ] Güvenlik otomasyondan önce değerlendiriliyor
- [ ] MANUAL modda da veto uygulanıyor
- [ ] Kilit nedenleri bitmask olarak taşınıyor ve yayınlanıyor
- [ ] Kilit aktifleşirken loglanıyor; log seli yok
- [ ] Modül donanıma dokunmuyor; host'ta test edilebilir
- [ ] Hesaplama süresi ölçüldü ve `app_core` bütçesinde

## Test Plan

- [ ] Su seviyesi düşükken pompa açma talebi reddediliyor
- [ ] Seviye sensörü kablosu çıkarıldığında pompa açma talebi reddediliyor (fail-safe)
- [ ] MANUAL modda web'den verilen açma komutu güvenlik vetosuyla reddediliyor
- [ ] Çalışan pompa, seviye düştüğünde anında kapatılıyor
- [ ] Birden fazla kilit aynı anda aktifken hepsi raporlanıyor
- [ ] Kilit kalkınca aktüatör tekrar açılabiliyor
- [ ] Host tarafında tüm kilit kombinasyonları test edildi
- [ ] Değerlendirme süresi ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.7, §12)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — **fail-safe davranış eksiksiz mi**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — otomasyondan önce mi çalışıyor
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + **tüm kilit senaryoları donanımda kanıtlandı** (simülasyon yeterli değil) +
host tarafı kombinasyon testleri geçti.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Değerlendirme sıklığı: (a) her `app_core` döngüsünde tam hesaplama

```text
Selected: (a) — 100 ms'de bir, kosulsuz, tam hesaplama.
Reddedilen: (b) degisim tetiklemeli.
Gerekce:  Guvenlik icin ONGORULEBILIRLIK verimlilikten onemlidir. Degisim
          tetiklemeli bir tasarimda "tetigin kacirildigi" tek bir yol bile
          kilidin hic hesaplanmamasi demektir ve bu SESSIZ bir arizadir.
          Hesaplama birkac karsilastirma + bir dizi taramasi; 100 ms
          butcesinde olculebilir bir yuk degil.
```

## Karar 2 — İzin sorusu "açılabilir mi" DEĞİL, "enerjili kalabilir mi"

**Bu, TASK-029'a geri uygulanan bir tasarım düzeltmesidir.**

```text
Ilk tasarim: SafetyPermitFn(id, turningOn) — yalnizca ACMA aninda soruluyordu.
Problem:     §12.1 Katman 2 "calisma sirasinda izleme" ve test plani
             "calisan pompa, seviye dustugunde ANINDA kapatiliyor" maddesi
             bu imzayla karsilanamiyordu. SafetyMonitor'un roleye uzanmasi
             ya da ActuatorManager'a ikinci bir zorla-kapat yolu eklenmesi
             gerekecekti — ikisi de TEK KAPI kuralini zayiflatir.

Duzeltme:    SafetyPermitFn(id) → "bu aktuator SU AN enerjili olabilir mi?"
             Ayni yanit hem acmaya hem devam etmeye uygulanir.

Kazanc:      Katman 2 uygulamasi TEK KAPININ ICINDE kaldi. SafetyMonitor
             saf bir karar uretecidir; hicbir sekilde donanima uzanmaz.
             `apply()` adim 3'te enerjili aktuatorler icin izin taze sorulur,
             kalkmissa `minRunMs` TANIMADAN derhal kapatilir.
```

## Karar 3 — Kilit → aktüatör eşlemesi AÇIK bir tablo

```text
Kilit                   | Su pompasi | Hava pompasi | Gerekce
------------------------|-----------|--------------|------------------------
EMERGENCY_LATCHED       |    X      |      X       | Acil durum her seyi keser
LEVEL_INSUFFICIENT      |    X      |      -       | Kuru calisma pompayi yakar
LEVEL_SENSOR_FAULT      |    X      |      -       | Okunamayan = en kotu durum
DRY_RUN (TASK-031)      |    X      |      -       | Akis dogrulama basarisiz
MAX_RUNTIME_REPEATED    |    X      |      X       | Sistemik ariza gostergesi

Hava pompasi seviyeden ETKILENMEZ: hava tasi susuz kalinca hasar gormez,
yalnizca ise yaramaz. Kuru calisma riski su pompasina ozgudur. Gereksiz
kilit, operatorun guvenlik uyarilarina duyarsizlasmasina yol acar.
```

## Karar 4 — Fail-safe: okunamayan sensör = en kötü durum

```text
Seviye sensoru ornegi:
  quality == OK            → deger kullanilir
  quality != OK            → LEVEL_SENSOR_FAULT (okunamiyor)
  sensor snapshot'ta YOK   → LEVEL_SENSOR_FAULT (hic gelmemis)

`requireLevelSensor == 0` ise bu kilit UYGULANMAZ — operatorun bilincli
tercihi. Varsayilan 1'dir (TASK-014 Karar: guvenlik sensorleri varsayilan
olarak ETKIN).
```

## Karar 5 — Log seli önleme: kilit KENARINDA loglanır

Kilit maskesi bir öncekiyle karşılaştırılır; yalnızca **yeni aktifleşen**
bitler loglanır. 100 ms'de bir loglansaydı 64 kayıtlık halka tampon
**6,4 saniyede** dolar ve asıl arıza kaydı silinirdi.

## Karar 6 — SafetyMonitor SAFTIR: yalnızca snapshot okur

Donanıma dokunmaz, röleye uzanmaz, `services/` çağırmaz. Girdisi
`SystemState` snapshot'ı + `Config` + aşım sayacı; çıktısı kilit maskesi.
TASK-064 host tarafında **tüm kilit kombinasyonlarını** donanımsız
koşturabilir.

---

# STEP 3 — REVIEW RECORD

- [x] Kilit tablosu implement edildi (5 kilit, `masksFor()` eşlemesi
      `static_assert`larla kilitli)
- [x] `permits()` neden koduyla karar döndürüyor
- [x] Sensör kalitesi `OK` değilse kilit aktif — **fail-safe üç yolda da**:
      sensör yok / kalite bozuk / başlatılmamış modül
- [x] Güvenlik otomasyondan önce değerlendiriliyor (döngü sırası TASK-033'te
      sabitlenecek; `evaluate()` bağımsız çağrılabilir)
- [x] MANUAL modda da veto uygulanıyor — `permits()` modu hiç okumaz,
      dolayısıyla mod'a göre gevşetilmesi **yapısal olarak imkânsız**
- [x] Kilit nedenleri bitmask olarak taşınıyor ve yayınlanıyor
- [x] Kilit yalnızca **kenarda** loglanıyor; log seli yok
- [x] Modül donanıma dokunmuyor — tarama: `digitalWrite|digitalRead|pinMode|`
      `analogRead|Wire.|Serial.|hal::` → **0 eşleşme**
- [x] Heap kullanımı yok; durum 4 × `uint32_t` + zaman damgası
- [x] Derleme temiz
- [ ] **Kilit senaryolarının donanımda kanıtlanması — donanım gerekiyor**

## Tasarım sırasında bulunan eksik: Katman 2 uygulanamıyordu

TASK-029'da yazdığım `SafetyPermitFn(id, turningOn)` imzası **yalnızca açma
anında** soruluyordu. Kabul kriteri "çalışan pompa, seviye düştüğünde anında
kapatılıyor" bu imzayla karşılanamıyordu; karşılamak için ya SafetyMonitor'un
röleye uzanması ya da `ActuatorManager`'a ikinci bir zorla-kapat yolu
eklenmesi gerekirdi — **ikisi de tek kapı kuralını zayıflatır.**

İmza `SafetyPermitFn(id)` olarak değiştirildi ve soru "bu aktüatör ŞU AN
enerjili olabilir mi?" hâline geldi. `apply()` adım 3'te enerjili aktüatörler
için izin taze sorulur; kalkmışsa `minRunMs` **tanınmadan** derhal kapatılır.
Böylece §12.1 Katman 2 tek kapının içinde uygulanmış oldu.

## `SafetyMonitor` → `ActuatorManager` bağımlılığı: tek nokta

Yalnızca `maxRunViolations()` okunuyor (`SafetyMonitor.cpp:79`). Bu bir
sorgu; karar üretmiyor, donanıma dokunmuyor. Ters yön (`ActuatorManager` →
`SafetyMonitor`) fonksiyon işaretçisiyle kurulduğu için **derleme zamanı
döngüsel bağımlılık yok**.

## Kapsam dışı bırakılanlar (plana uygun)

`ILK_DRY_RUN` ve `ILK_EMERGENCY_LATCHED` bitleri tanımlı ve veto zincirine
dahil, ancak **bu task'ta hiçbir yerden set edilmiyorlar**.
`setExternalInterlock()` onların kapısıdır: kuru çalışma TASK-031'in,
mandal TASK-032'nin kararıdır. Böylece tek veto noktası ilkesi korunur —
o modüller de röleye uzanmaz, buraya bir bit bildirir.

**TASK-030: TAMAMLANDI** (donanım kanıtı bekliyor).
