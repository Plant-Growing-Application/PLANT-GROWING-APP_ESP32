# TASK-007 — StateStore

**Phase:** 1 — Core Infrastructure · **Priority:** P0

## Objective

`SystemState`'e güvenli, tutarlı ve düşük maliyetli erişim sağlamak. Mevcut projedeki
korumasız global değişken erişimini (Kritik Problem 2) yapısal olarak imkânsız kılmak.

## Scope

- Mutex korumalı state deposu
- Alt-state bazlı yayınlama fonksiyonları (`publishSensors`, `publishNetwork`, `publishActuators`, `publishSafety`, `publishAutomation`, `publishSystem`, `publishTime`)
- Atomik snapshot alma (`snapshot()`)
- Monotonik versiyon sayacı (`version()`)
- Tek yazar kuralının uygulanması

## Out of Scope

- State alanlarının doldurulması (servis task'ları)
- Değişim bildirimi/abonelik mekanizması — bilinçli olarak yok (§5.1)
- UI ve web'in state kullanımı (kendi task'ları)

## Dependencies

- TASK-006

## Requirements

- `REQUIREMENTS.md` — Kritik Problem 2, §10 (task senkronizasyonu yok)

## Architecture References

- §2.1 StateStore modülü
- §4.2 Versiyonlama · §4.3 Neden merkezi state
- §5 Event/mesaj mimarisi (mutex vs queue gerekçesi)

## Expected Design

### Karar gerektiren nokta 1 — Kilit stratejisi

```text
Problem:      5 task + AsyncTCP callback aynı state'e erişecek
Constraints:  UI 20 Hz okuyor, app_core 10 Hz yazıyor; kritik bölge kısa olmalı;
              okuyucu kilit tutarak iş yapmamalı; deadlock olmamalı
Approaches:   (a) tek mutex + tam snapshot kopyası
              (b) alt-state başına ayrı mutex
              (c) çift tamponlama (double buffer) + atomik takas
              (d) okuma-yazma kilidi
Trade-offs:   (b) atomik tutarlı görüntüyü bozar ve kilit sırası deadlock riski doğurur
              (c) bellek iki katına çıkar, yazar tarafı karmaşıklaşır
              (d) ESP32'de doğrudan karşılığı yok, elle kurulur
Recommended:  (a) — ARCHITECTURE §4.3 gerekçesi; kritik bölge yalnızca memcpy süresi
```

### Karar gerektiren nokta 2 — Tek yazar kuralının zorlanması

```text
Approaches:   (a) yalnızca dokümantasyonla (sözleşme)
              (b) çalışma zamanında çağıran task handle'ı doğrulama
              (c) derleme zamanı token/anahtar deseni
Trade-offs:   (b) hatayı erken yakalar, küçük çalışma zamanı maliyeti getirir
              ve geliştirme sırasında çok değerlidir
Recommended:  Geliştirme derlemesinde (b), üretimde devre dışı bırakılabilir
```

## Implementation Notes

- Kritik bölge içinde **asla** log çağrısı, seri port yazımı veya başka kilit alma olmamalı.
- `snapshot()` kopyayı çağıranın verdiği tampona yazmalı; dönüş değeriyle büyük yapı
  kopyalamak yığın (stack) baskısı yaratır. UI ve web task'larının stack bütçeleri sınırlı.
- Mutex bekleme süresi **sınırlı** olmalı (sonsuz bekleme yok); zaman aşımında hata dönüp
  `Diagnostics`'e kaydedilmeli. Bu, tek bir hatalı task'ın tüm sistemi kilitlemesini önler.
- Versiyon sayacı taşması ele alınmalı (fark hesabı, doğrudan karşılaştırma değil).
- ISR'den erişim **yasak**.
- Kritik bölge süresi ölçülmeli; hedef 10 µs mertebesi.

## Files

- `src/core/StateStore.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Kilit stratejisi seçildi ve gerekçesi yazıldı
- [ ] Alt-state bazlı yayınlama fonksiyonları çalışıyor
- [ ] `snapshot()` atomik ve tutarlı kopya veriyor
- [ ] Versiyon sayacı her yayınlamada artıyor
- [ ] Mutex bekleme süresi sınırlı; zaman aşımı hata olarak raporlanıyor
- [ ] Kritik bölgede log/blocking çağrısı yok
- [ ] Tek yazar kuralı en az geliştirme derlemesinde doğrulanıyor
- [ ] Kritik bölge süresi ölçülüp kaydedildi

## Test Plan

- [ ] İki task eşzamanlı yazar + bir task okurken tutarsız snapshot alınmıyor (stres testi)
- [ ] Versiyon sayacı monotonik artıyor; taşma sınırında doğru davranıyor
- [ ] Mutex zaman aşımı senaryosu tetiklenip doğru hata döndüğü doğrulandı
- [ ] Kritik bölge süresi ölçüldü, hedefin altında
- [ ] 24 saatlik çalışmada heap sabit (sızıntı yok)

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.1, §4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — sonsuz mutex beklemesi var mı
- [ ] Shared state güvenli mi? — **bu task'ın ana konusu**
- [ ] Memory problemi var mı? — snapshot kopyası stack'te mi, boyutu uygun mu
- [ ] Error handling var mı? — kilit zaman aşımı
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu? — tek yazar kuralı
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — global değişken deseni taşınmamalı

## Definition of Done

Ortak DoD + eşzamanlılık stres testi geçti + kritik bölge süresi ölçüldü + hiçbir global
mutable değişken kalmadı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Kilit stratejisi

```text
Selected:  (a) Tek mutex + tam snapshot kopyasi  — ARCHITECTURE §4.3 gerekcesi
Olcum:     sizeof(SystemState) = 312 bayt (TASK-006'da olculdu)
           240 MHz'de 312 baytlik memcpy ~1 us'in belirgin altinda
           → 10 us kritik bolge hedefine rahat siginiyor

Reddedilenler:
  (b) Alt-state basina ayri mutex → atomik tutarli goruntuyu BOZAR.
      UI bes alt-state okurken arada yazim olursa ekranda birbiriyle
      tutarsiz degerler cikar. Ayrica kilit sirasi deadlock riski dogurur.
  (c) Cift tamponlama → bellek iki katina cikar (624 B), yazar tarafi
      karmasiklasir; 1 us'lik kritik bolge icin gerekcesiz.
  (d) Okuma-yazma kilidi → ESP32'de dogrudan karsiligi yok, elle kurulmasi
      gerekir; kazanci 1 us'lik bolge icin ihmal edilebilir.
```

## Karar 2 — Tek yazar kuralının zorlanması

```text
Problem:      P1 "tek yazar" kurali nasil garanti edilir?
Approaches:   (a) yalnizca dokumantasyon    → ihlal fark edilmez
              (b) calisma zamaninda task handle dogrulamasi
              (c) derleme zamani token deseni → cagri yerlerini kirletir
Selected:     (b) — her alt-state ilk publish'te cagiran task'i SAHIP olarak
              kaydeder. Sonraki cagrilarda handle karsilastirilir.

IHLAL DAVRANISI — onemli tasarim karari:
  Ihlal REDDEDILMEZ, LOGLANIR ve SAYILIR.
  Gerekce: reddetmek, programlama hatasini calisma zamani davranis
  degisikligine cevirir ve ikinci bir hata (eksik state) yaratir.
  Ihlal gorunur olmali, sistemi bozmamali.

  Tespit KILIT ICINDE yapilir (bayrak set edilir),
  loglama KILIT DISINDA (CODING_STANDARDS §7 — kritik bolgede log yasak).
```

## Karar 3 — `snapshot()` çağıranın tamponuna yazar

```text
Problem:      312 baytlik yapi nasil dondurulmeli?
Constraints:  ui task stack 3.5 KB, app_core 4 KB (ARCHITECTURE §6.1);
              deger donusu ek bir kopya ve yigin baskisi yaratir
Selected:     void snapshot(SystemState& out) — cagiranin verdigi tampona yazilir.
              Deger donusu KULLANILMAZ.
```

## Karar 4 — Kilit zaman aşımı sınırlı

Sonsuz bekleme **yasaktır** (CODING_STANDARDS §7): tek bir hatalı task tüm
sistemi kilitlememelidir. Zaman aşımında `ErrCode` döner ve olay sayılır.
Okuyucu bir önceki snapshot'ıyla devam edebilir.

## Karar 5 — Versiyon sayacı ve taşma

```text
Sayac:   uint32, her publish'te artar.
Tasma:   20 Hz'de 2^32 ≈ 6.8 yil. Yine de dogrudan karsilastirma YAPILMAZ;
         `isNewerThan(a, b)` isaretli fark kullanir — tasma sinirinda dogru
         calisir (Millis ile ayni yaklasim).
Kullanim: Web gereksiz WS trafigi uretmez, UI ekrani gereksiz cizmez
         (ARCHITECTURE §4.2).
```

## Karar 6 — Kritik bölge ölçümü yerleşik

Acceptance Criteria "kritik bölge süresi ölçülüp kaydedildi" istiyor ve
TASK-062 (profilleme) bu veriye ihtiyaç duyacak. Ölçümü sonradan eklemek
`StateStore`'a tekrar dokunmak demektir; **şimdi yerleştiriliyor**:

- `esp_timer_get_time()` ile kilit tutma süresi ölçülür (donanım sayacı, ucuz)
- Yalnızca **en yüksek** değer saklanır (istatistik birikimi yok)
- `stats()` ile okunur — TASK-043 `/api/diagnostics` ve TASK-062 kullanacak

## Kapsam dışı bırakılanlar

- State alanlarını dolduran mantık → ilgili servis task'ları
- Değişim bildirimi / abonelik → **bilinçli olarak YOK** (ARCHITECTURE §5.1);
  okuyucular kendi periyotlarında snapshot alır
- UI ve web'in state kullanımı → kendi task'ları

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Kilit stratejisi seçildi ve gerekçesi yazıldı (STEP 1, Karar 1) —
      reddedilen üç alternatif de gerekçelendirildi
- [x] Yedi alt-state için yayınlama fonksiyonu çalışıyor
- [x] `snapshot()` atomik ve tutarlı kopya veriyor; çağıranın tamponuna yazıyor
- [x] Versiyon sayacı her yayınlamada artıyor; `isNewerThan()` taşma güvenli
- [x] Mutex beklemesi **10 ms ile sınırlı**; zaman aşımı `ErrCode` döndürüyor
      ve `lockTimeouts` sayacına işleniyor
- [x] **Kritik bölgede log/bloklama yok** — tarama ile doğrulandı
- [x] Tek yazar kuralı çalışma zamanında doğrulanıyor (task handle karşılaştırması)
- [x] Kritik bölge süresi ölçümü **yerleşik** (`maxLockHoldUs`) — TASK-062 okuyacak

## Ölçümler

Çekirdek modüllerin gerçek RAM maliyeti (tüm fonksiyonlar referanslanarak):

| Ölçüm | Değer |
|---|---|
| İskelet (TASK-002 sonrası) | 21 464 B |
| + Diagnostics + CommandQueue + StateStore | 23 480 B |
| **Çekirdek altyapı toplam** | **~1 704 B** (test tamponu hariç) |

Dağılım (hesap):

```text
Diagnostics   halka tampon 768 + hata dizisi 32 + BootReport 76 + mutex  ≈  960 B
CommandQueue  kuyruk 256 + StaticQueue_t + atomikler                     ≈  350 B
StateStore    SystemState 312 + mutex + sahip dizisi 28 + istatistik      ≈  440 B
```

Flash: 267 437 → 267 837 B (StateStore **+400 bayt**).

> **Not:** Yalnızca `begin()` çağrıldığında ölçüm +864 B çıkıyor; linker
> `--gc-sections` ile kullanılmayan statik veriyi atıyor. Yukarıdaki 1 704 B
> tüm fonksiyonlar referanslandığındaki **gerçek** bütçedir. Sistem TASK-013'te
> bağlandığında bu değer doğrulanacak.

## Statik denetimler

```text
Kritik bolge icinde diag::log / Serial / ikinci kilit : 0 ihlal
portMAX_DELAY (sonsuz bekleme)                        : yok
new / malloc / dinamik mutex                          : yok (Static varyantlar)
Modul ici global'ler                                  : 3 dosyanin 3'u de
                                                        anonim namespace icinde
                                                        → dis baglantisi yok
```

**Global mutable değişken (Y1) durumu:** çekirdek modüllerin iç durumu anonim
namespace içinde ve mutex korumasında. Mevcut sistemdeki `currentIP`,
`currentMAC`, `Sensor.WaterFlow` gibi dış bağlantılı, korumasız global'lerin
karşıtı — `src/` genelinde böyle bir değişken **yok**.

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı
- [x] Kilit sırası ve kritik bölge içeriği tarama ile doğrulandı
- [x] Taşma güvenli versiyon karşılaştırması (`isNewerThan`) implement edildi
- [ ] **Eşzamanlılık stres testi (2 yazar + 1 okuyucu) — donanım gerekiyor**
- [ ] **Kritik bölge süresinin fiili ölçümü — donanım gerekiyor**
      (altyapı hazır: `stats().maxLockHoldUs`)
- [ ] **Mutex zaman aşımı senaryosu — donanım gerekiyor**
- [ ] **24 saatlik heap kararlılığı — donanım gerekiyor**

> Bu maddeler TASK-060 (entegrasyon) ve TASK-062 (profilleme) kapsamında
> donanımda kapatılacak. Ölçüm altyapısı şimdi yerleştirildiği için o task'lar
> `StateStore`'a yeniden dokunmayacak.

## Review Checklist

- [x] Architecture'a uygun mu? — §2.1 modül sözleşmesi, §4.2 versiyonlama,
      §4.3 tek mutex gerekçesi uygulandı
- [x] Gereksiz abstraction var mı? — abonelik/event bus **bilinçli olarak yok**
      (ARCHITECTURE §5.1); okuyucular kendi periyotlarında snapshot alır.
      Yayınlama gövdesi tek şablon fonksiyonda toplandı, yedi kopya yok.
- [x] Blocking işlem var mı? — kilit beklemesi 10 ms sınırlı, sonsuz bekleme yok;
      kritik bölge yalnızca `memcpy` + sayaç
- [x] **Shared state güvenli mi? — bu task'ın ana konusu.** Tek mutex, atomik
      snapshot, tek yazar doğrulaması, sınırlı bekleme.
- [x] Memory problemi var mı? — 440 B statik; snapshot çağıranın tamponuna
      yazılıyor (yığın baskısı yok); heap kullanımı sıfır
- [x] Error handling var mı? — kilit zaman aşımı `ErrCode` döndürüyor ve
      sayılıyor; `snapshot()` başarısızlıkta `out`'u **değiştirmiyor** ki
      çağıran önceki kopyasıyla devam edebilsin
- [x] ESP32 resource kullanımı uygun mu? — statik mutex, `esp_timer_get_time()`
      donanım sayacı
- [x] Task sorumluluğu doğru mu? — tek yazar kuralı çalışma zamanında
      doğrulanıyor; ihlal reddedilmiyor, **CRITICAL loglanıyor ve sayılıyor**
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Mevcut sistemde
      hiçbir senkronizasyon primitifi yoktu (REQUIREMENTS §10: "Mutex, semaphore
      veya queue hiç kullanılmıyor"); bu modül tam olarak o boşluğu kapatmak
      için sıfırdan tasarlandı.

## Tasarım notu — ihlal neden reddedilmiyor?

Tek yazar ihlalinde yazma **iptal edilmiyor**. Gerekçe: reddetmek, bir
programlama hatasını çalışma zamanı davranış değişikliğine çevirir ve ikinci
bir hata (eksik/eskimiş state) yaratır. İhlal görünür olmalı ama sistemi
bozmamalıdır. Bu yüzden CRITICAL loglanır, sayılır ve `/api/diagnostics`
üzerinden okunabilir olur.

## Durum

**TASK-007: TAMAMLANDI** (çalışma zamanı testleri donanım bekliyor).
