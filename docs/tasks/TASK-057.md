# TASK-057 — AutomationEngine, Mode & Manual Override

**Phase:** 12 — Automation · **Priority:** P1

## Objective

Kural değerlendiricilerini tek bir motorda birleştirmek, çalışma modunu (MANUAL/AUTO)
yönetmek ve **süreli manuel override** mekanizmasını kurmak.

## Scope

- `AutomationEngine.evaluate()` — kuralları çalıştırıp istenen aktüatör durumlarını üretme
- Mod yönetimi: `MANUAL` / `AUTO`
- Süreli manuel override ve kalan sürenin takibi
- `automation` alt-state'inin yayınlanması
- `app_core` döngüsüne entegrasyon (TASK-033'te ayrılan yer)

## Out of Scope

- Kural değerlendirme detayı (TASK-055, TASK-056)
- Aktüatör kısıtları ve güvenlik (TASK-029, TASK-030)

## Dependencies

- TASK-055, TASK-056, TASK-033

## Requirements

- `REQUIREMENTS.md` — §4 (otomatik kontrol yok), §5.3 (manuel/otomatik mod seçimi yok)

## Architecture References

- §2.8 AutomationEngine · §10.3 Tahkim ve süreli override
- §11.1 Değerlendirme döngüsü · §11.4 Otomasyonun bilmediği şeyler

## Expected Design

### Süreli manuel override (§10.3)

```text
Problem:      AUTO modda operatör manuel komut verdi. Otomasyon ne yapmalı?
Constraints:  Kalıcı override, unutulan bir komutun otomasyonu süresiz devre dışı
              bırakmasına yol açar — hidroponikte bitki kaybı demektir;
              hiç override olmaması operatörü çaresiz bırakır
Approaches:   (a) manuel komut modu MANUAL'e çevirir (kalıcı)
              (b) manuel komut reddedilir (AUTO'da yalnızca otomasyon)
              (c) süreli override: N dakika sonra otomasyon kontrolü geri alır
Recommended:  (c) — ARCHITECTURE §10.3 kararı; süre yapılandırılabilir,
              kalan süre arayüzde gösterilir
```

### Otomasyonun bilmediği şeyler (§11.4)

Motor **yalnızca** "şu aktüatörün açık/kapalı olmasını istiyorum" der. Bilmez:

- Güvenlik kilitleri
- Aktüatör kısıtları (min/max/cooldown)
- Röle polaritesi, pin numarası

Bu ayrım sayesinde kural motoru karmaşıklaşsa bile güvenlik mantığı sabit, denetlenebilir
ve ayrı test edilebilir kalır.

### Değerlendirme sırası

```text
  1. mod MANUAL ise → kurallar değerlendirilmez, mevcut durum korunur
  2. mod AUTO ise:
       a. aktif override varsa → o aktüatör için otomasyon atlanır
       b. threshold kuralları değerlendirilir
       c. schedule kuralları değerlendirilir (zaman geçerliyse)
       d. çakışmalar önceliğe göre çözülür
  3. istenen durumlar ActuatorManager'a iletilir
```

## Implementation Notes

- Motor `app_core` içinde, **güvenlik değerlendirmesinden sonra** çağrılır (§11.1 adım 4).
  Bu sıra TASK-033'te sabitlenmiştir.
- Override süresi dolduğunda kullanıcıya bildirilmeli (state üzerinden); sessizce
  otomasyona dönmek kafa karıştırır.
- Mod değişimi bir komuttur ve `CommandQueue` üzerinden gelir; UI veya web doğrudan
  motoru çağırmaz.
- Mod değişimi kalıcı olmalı (config'te); yeniden başlatmada korunmalı. Ancak varsayılan
  `MANUAL` olmalı (güvenli varsayılan, TASK-014).
- Çakışan kurallar için öncelik tanımsızsa bu bir yapılandırma hatasıdır; loglanmalı.
- Motor saf olmalı: snapshot + config + zaman → istenen durumlar. Yan etkisi olmamalı.
  Bu, host tarafında test edilebilirliği sağlar.
- Kural sonuçlarının izlenebilirliği önemli: hangi kural aktüatörü neden açtı bilgisi
  state'te veya logda bulunmalı — aksi halde otomasyon "kara kutu" olur.

## Files

- `src/domain/AutomationEngine.h` / `.cpp` (yeni)
- `src/domain/AppCore.cpp` (güncelleme — motor çağrısı)

## Acceptance Criteria

- [ ] Motor kuralları değerlendirip istenen durumları üretiyor
- [ ] MANUAL modda kurallar değerlendirilmiyor
- [ ] Süreli override çalışıyor; süre yapılandırılabilir
- [ ] Kalan override süresi state'te taşınıyor ve gösteriliyor
- [ ] Override süresi dolduğunda kullanıcıya bildiriliyor
- [ ] Mod değişimi komut üzerinden geliyor ve kalıcı
- [ ] Varsayılan mod `MANUAL`
- [ ] Motor güvenlik kilitlerini ve aktüatör kısıtlarını bilmiyor
- [ ] Hangi kuralın neden tetiklendiği izlenebilir
- [ ] Motor saf; host'ta test edilebilir
- [ ] Güvenlikten sonra çağrılıyor

## Test Plan

- [ ] AUTO modda kural aktüatörü sürüyor
- [ ] MANUAL modda kurallar çalışmıyor
- [ ] AUTO modda manuel komut override başlatıyor
- [ ] Override süresi dolunca otomasyon kontrolü geri alıyor
- [ ] **Güvenlik vetosu otomasyon kararını geçersiz kılıyor** (kritik test)
- [ ] Mod değişimi yeniden başlatmada korunuyor
- [ ] Çakışan kurallar önceliğe göre çözülüyor
- [ ] Host tarafında motor senaryoları test edildi
- [ ] Uçtan uca: gerçek sensör → kural → pompa çalışması doğrulandı
- [ ] Kural tetikleme izlenebilirliği doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.8, §10.3, §11.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — değerlendirme süresi `app_core` bütçesinde mi
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — tanımsız öncelik, geçersiz kural
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — **güvenlikten sonra mı çalışıyor**
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — referans yok, sıfırdan tasarım

## Definition of Done

Ortak DoD + **M7 kilometre taşı: kurallar aktüatörleri sürüyor ve güvenlik vetosu her
durumda kazanıyor** + uçtan uca sulama senaryosu donanımda doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — `MANUAL` modda kurallar HİÇ değerlendirilmez

Yalnızca "istek üretilmez" değil, **değerlendirme hiç çalışmaz**. Sonuç
aynı görünür ama fark önemli: `MANUAL`'de kural zamanlayıcıları da
ilerlemez, dolayısıyla `AUTO`'ya geçince kurallar temiz bir durumdan
başlar — yarı ilerlemiş bir histerezisle değil.

Bu aynı zamanda **M4 kapısının kapalı kalma mekanizmasıdır**: varsayılan
mod `MANUAL`, dolayısıyla otomasyon kodu derlenmiş olsa bile çalışmaz.

## Karar 2 — Süreli manuel override: (c)

```text
(a) manuel komut modu kalici olarak MANUAL'e cevirir
    → operator bir kez mudahale eder, otomasyon SESSIZCE olur, kimse
      fark etmez. Hidroponikte bu bitki kaybidir.
(b) AUTO'da manuel komut reddedilir
    → operatoru caresiz birakir; acil bir mudahale gerektiginde
      once mod degistirmesi gerekir.

(c) SECILDI: override SURELIDIR. `manualOverrideMs` (varsayilan 15 dk)
    dolunca otomasyon o aktuatorun kontrolunu GERI ALIR.
```

**Override AKTÜATÖR BAŞINADIR**, global değil: operatör hava pompasına
müdahale ettiğinde su pompasının otomasyonu durmamalı.

Kalan süre `AutomationStatus.overrideRemaining` ile **yayınlanır** ve
süre dolduğunda log üretilir — sessizce otomasyona dönmek kafa karıştırır.

## Karar 3 — Motor kısıtları ve güvenliği BİLMEZ (§11.4)

Motor yalnızca `actuators::request(id, on, ControlSource::AUTOMATION, now)`
çağırır. Sonucu (`DEFERRED_COOLDOWN`, `REJECTED_SAFETY` …) **kaydeder ama
davranışını değiştirmez** — kural bir sonraki döngüde aynı isteği yine
üretir ve kısıt/kilit kalkınca kendiliğinden uygulanır.

Motorun "cooldown var, o zaman istemeyeyim" demesi, kısıt mantığının iki
yere dağılması olurdu.

## Karar 4 — Çakışma çözümü: öncelik, sonra kural sırası

```text
Ayni aktuatoru hedefleyen birden fazla kural:
    1. yuksek `priority` KAZANIR
    2. esit oncelik → dogrulama zaten REDDEDIYOR (TASK-054)
       ama calisma zamaninda yine de belirsizlik birakmiyoruz:
       DIZIDEKI ILK kural kazanir ve bu DAVRANIS BELGELIDIR.
```

## Karar 5 — Hiçbir kural bir aktüatör hakkında konuşmuyorsa DOKUNULMAZ

`applies == 0` olan kurallar sessizdir. Bir aktüatörü hedefleyen etkin
kural yoksa motor o aktüatör için **hiçbir istek üretmez** — mevcut durum
korunur. Aksi hâlde kural tanımlamak, tanımlanmamış aktüatörleri kapatmak
anlamına gelirdi.

## Karar 6 — Mod değişimi bir KOMUTTUR ve kalıcıdır

`CommandType::SET_AUTOMATION_MODE` (TASK-008'de zaten tanımlı) üzerinden
gelir; UI veya web motoru doğrudan çağırmaz. Değişiklik config'e yazılır
ve yeniden başlatmada korunur — ama **varsayılan `MANUAL`**'dir.

---

# STEP 3 — REVIEW RECORD

- [x] `evaluate()` kuralları çalıştırıp istekleri üretiyor
- [x] Mod yönetimi `MANUAL`/`AUTO`; kalıcı (config'te), varsayılan `MANUAL`
- [x] Süreli manuel override, **aktüatör başına**
- [x] Kalan süre yayınlanıyor; süre dolduğunda loglanıyor
- [x] `automation` alt-state'i yayınlanıyor
- [x] `app_core` döngüsüne bağlandı — TASK-033'te ayrılan yer dolduruldu
- [x] Döngü sırası doğrulandı: `safety::evaluate` (197) → `automation::evaluate`
      (207) → `actuators::apply` (213)
- [x] Motor donanıma dokunmuyor — tarama: `hal::|digitalWrite|relay` → **0**
- [x] Heap tahsisi yok
- [x] `SET_AUTOMATION_MODE` komut yolundan işleniyor
- [ ] **Donanım testleri bekliyor**

## Bulunan hata: tahkim KALICI KİLİTLENME üretiyordu

TASK-029'da şunu yazmıştım:

> "MANUAL override'ın SÜRE SINIRI TASK-057'nin işidir. O yapılana kadar
> AUTOMATION kaynaklı talep gelmiyor, dolayısıyla kilitlenme oluşmuyor."

TASK-057 yapıldı ve **kilitlenme canlıya geçti**:

```text
1. Operator pompayi acar        → rt.source = MANUAL
2. 15 dk sonra override doler   → g_override temizlenir
3. Otomasyon istek gonderir     → sourceOutranks(AUTOMATION, MANUAL) = false
                                → REJECTED_MODE
4. ... ve bu SONSUZA KADAR boyle kalir.
```

Override süresinin dolması `rt.source`'u geri vermiyordu. Sonuç: operatör
bir kez müdahale ettikten sonra otomasyon o aktüatörü **bir daha hiç**
kontrol edemezdi — üstelik hiçbir hata görünmeden, yalnızca INFO
seviyesinde "düşük öncelikli kaynak reddedildi" satırlarıyla.

**Düzeltme:** `actuators::releaseSource(id)` eklendi ve override sona
ererken çağrılıyor. `SAFETY` kaynağı bilinçli olarak serbest bırakılmıyor —
güvenliğin belirlediği bir durumu otomasyonun devralması, vetoyu delmek
olurdu.

Ayrıca override süpürmesi **moddan bağımsız** hâle getirildi: yalnızca
kural hedefi olan aktüatörler kontrol edilseydi, kuralı olmayan bir
aktüatörün override'ı hiç sona ermez ve `releaseSource()` hiç çağrılmazdı.

## Tamamlanan eksik: `/api/config/automation`

TASK-044 kapsamında `PUT /api/config/automation` vardı ama uygulanmamıştı.
Bu turda eklendi (mod + `manualOverrideMs`).

## M4 KAPISI — durum

| Gereklilik | Durum |
|---|---|
| Otomasyon kodu yazıldı | ✅ |
| Varsayılan mod `MANUAL` | ✅ |
| Varsayılan kural kümesi boş | ✅ |
| `MANUAL`'de kurallar hiç değerlendirilmiyor | ✅ |
| **M4 donanımda doğrulandı** | ❌ **HAYIR** |

**Kapı kapalı.** Otomasyon derlenmiş durumda ama çalışmıyor ve M4
doğrulanana kadar `AUTO`'ya geçilmemelidir. Bu bir kod işi değil, donanım
doğrulaması işidir.

**TASK-057: TAMAMLANDI** (M4 kapısı ve donanım testleri bekliyor).
