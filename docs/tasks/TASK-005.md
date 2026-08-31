# TASK-005 — Diagnostics & Logging

**Phase:** 1 — Core Infrastructure · **Priority:** P0

## Objective

Seviyeli, kodlu, sorgulanabilir bir teşhis altyapısı kurmak. Mevcut projedeki dağınık
`Serial.println` yaklaşımının yerini almak; hataların API ve OLED üzerinden görünür olmasını
sağlamak.

## Scope

- Seviyeli log fonksiyonları (`INFO`/`WARNING`/`ERROR`/`CRITICAL`)
- RAM'de sabit boyutlu halka tampon (son N olay)
- Aktif hata bayrakları (bitmask) — `raise()` / `clear()` / `activeFaults()`
- Boot raporu deposu (TASK-010 dolduracak)
- Reset nedeni okuma ve kaydetme
- Çok task'lı erişim güvenliği

## Out of Scope

- Kalıcı log dosyası yazımı (TASK-016 sonrası, yalnızca CRITICAL için)
- Log'un API üzerinden sunulması (TASK-043)
- Log'un OLED'de gösterilmesi (TASK-052)

## Dependencies

- TASK-004

## Requirements

- `REQUIREMENTS.md` — §9 Logging `[~]`, §11-Low (yapılandırılmış logging)

## Architecture References

- §2.3 Diagnostics modülü
- §16.1 Seviyeler · §16.2 Hata kodu yapısı · §16.4 Yasaklanan hata davranışları

## Expected Design

### Karar gerektiren nokta — Halka tampon eşzamanlılığı

```text
Problem:      5 task + AsyncTCP callback aynı tampona yazacak
Constraints:  ISR'den log çağrılmamalı; kritik bölge kısa olmalı;
              log yüzünden gerçek zamanlı task gecikmemeli
Approaches:   (a) mutex korumalı halka tampon
              (b) her task'a ayrı tampon, okumada birleştirme
              (c) kuyruk + adanmış log task'ı
Trade-offs:   (a) basit, kısa kritik bölge; (c) ek task maliyeti (§6.4 gereği
              gereksiz task açılmamalı); (b) sıralama bilgisini kaybettirir
Recommended:  (a) — kayıt sabit boyutlu, kopyalama ucuz
```

- Log kaydı **sabit boyutlu** olmalı: seviye, alt sistem, kod, zaman damgası, kısa metin.
  Değişken uzunluklu metin heap parçalanması yaratır.
- Serbest metin **isteğe bağlıdır**; hata kodu zorunludur (§16.2).
- Aktif hata bayrakları bitmask olarak tutulmalı ki "kaç aktif hata var" sorusu maliyetsiz
  yanıtlansın (UI 20 Hz'de bu bilgiyi çizecek).

## Implementation Notes

- ISR'den log çağrısı **yasak**. ISR yalnızca sayaç artırabilir; log task bağlamında yazılır.
- Seri port çıktısı yapılandırılabilir olmalı (üretimde kapatılabilir) ama halka tampon
  her koşulda dolmalı.
- Reset nedeni (`esp_reset_reason`) boot'ta okunmalı; watchdog kaynaklı reset **CRITICAL**
  olarak kaydedilmeli (§16.3).
- Emoji tabanlı log çıktısı kullanılmamalı; makine tarafından ayrıştırılabilir sabit format
  tercih edilmeli.
- Log çağrısının maliyeti ölçülmeli; 100 ms'lik `app_core` döngüsünde onlarca çağrı olabilir.

## Files

- `src/core/Diagnostics.h` / `.cpp` (yeni)
- `src/core/BootReport.h` (yeni)

## Acceptance Criteria

- [ ] Dört seviye çalışıyor ve halka tamponda saklanıyor
- [ ] Kayıt sabit boyutlu; dinamik ayırma yok
- [ ] `raise()`/`clear()`/`activeFaults()` bitmask üzerinden çalışıyor
- [ ] Çok task'lı erişim güvenli
- [ ] Reset nedeni boot'ta okunup kaydediliyor
- [ ] Boot raporu için depo hazır
- [ ] Seri çıktı yapılandırılabilir, halka tampon her zaman aktif

## Test Plan

- [ ] Tampon kapasitesinin üzerinde kayıt yazıldığında en eskiler düşüyor, çökme yok
- [ ] İki task aynı anda yazarken kayıt bozulması olmuyor (stres testi)
- [ ] Kasıtlı WDT reset sonrası reset nedeni doğru raporlanıyor
- [ ] Log çağrısı süresi ölçüldü ve `app_core` bütçesine sığıyor
- [ ] `activeFaults()` doğru bitmask döndürüyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§16)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — log çağrısı bloklamıyor mu
- [ ] Shared state güvenli mi? — halka tampon eşzamanlılığı
- [ ] Memory problemi var mı? — sabit boyut, heap yok
- [ ] Error handling var mı? — tampon dolu durumu
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — `Serial.println` deseni taşınmamalı

## Definition of Done

Ortak DoD + çok task'lı stres testi geçti + log çağrı maliyeti ölçüldü.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Log kaydında serbest metin saklanmayacak

```text
Problem:      Halka tamponda ne saklanacak?
Constraints:  ARCHITECTURE §16.2: "Serbest metin yalnizca insan icindir";
              32 kayitlik tamponda 33 baytlik metin = ~1 KB RAM;
              app_core 10 Hz calisiyor, kayit maliyeti dusuk olmali;
              UI ve API kaydi okuyup gosterecek
Approaches:   (a) ErrCode + sabit boyutlu metin tamponu
              (b) ErrCode + sayisal baglam (detail)
              (c) yalnizca ErrCode
Trade-offs:   (a) 33 bayt/kayit; metin cihazda uretilir, yerellestirilemez
              (c) baglam kaybi — "hangi sensor arizali" bilinmez
Selected:     (b) — `ErrCode` + `int32_t detail`.
              Kayit 12 bayt. `detail` baglam tasir: sensor kimligi, task
              kimligi, olculen deger, alt-neden kodu.
Gerekçe:      Metin **sunum katmaninin isi**. Kod→metin esleme UI (TASK-052) ve
              frontend (TASK-049) tarafinda yapilir; orada yerellestirilebilir
              ve cihaz RAM'i harcanmaz. Serbest metin yalnizca seri porta
              yazilir, SAKLANMAZ.
```

Mevcut projedeki emoji tabanlı `Serial.println` deseninin tam karşıtı:
kayıt makine tarafından ayrıştırılabilir, gösterim insana bırakılır.

## Karar 2 — Aktif hata takibi: bitmask mi, liste mi?

```text
Problem:      "Aktif hatalar" nasil tutulacak?
Constraints:  ErrCode uint16 ve seyrek — 65536 bitlik maske imkansiz;
              UI durum cubugu 20 Hz'de "kac aktif hata" soruyor → O(1) olmali;
              ALERTS ekrani (TASK-052) hatalarin LISTESINI gosterecek
Approaches:   (a) yalnizca alt sistem maskesi (10 bit)     → liste kaybolur
              (b) sabit boyutlu aktif hata dizisi           → sayim O(1) degil
              (c) (b) + artimli tutulan sayac ve alt sistem maskesi
Selected:     (c) — 16 slotluk aktif hata dizisi + `count` + `subsystemMask`.
              Ucu de raise()/clear() icinde artimli guncellenir.
Tasma:        16'dan fazla farkli hata aktiflesirse yeni hata DUSURULUR ve
              `overflow` bayragi set edilir. Sessiz kayip yok — bayrak
              API'den okunabilir.
```

## Karar 3 — Eşzamanlılık: kritik bölgede seri port yok

```text
Problem:      5 task + AsyncTCP callback ayni tampona yazacak
Constraints:  CODING_STANDARDS §7: kritik bolge icinde log/seri port/bloklama YOK;
              Serial.print I2C/UART hizinda calisir — mutex tutarken yapilamaz
Selected:     Mutex YALNIZCA halka tamponu ve hata dizisini korur.
              Sira: kilidi al → kaydi yaz → kilidi birak → SONRA seri porta yaz.
              Kritik bolge yalnizca birkac struct atamasi kadar.
ISR:          ISR'den log CAGRILMAZ (CODING_STANDARDS §6). Mutex almak ISR'de
              cokmeye yol acar; bu yuzden ISR baglami tespit edilirse cagri
              sessizce dusurulur ve sayilir.
```

## Karar 4 — `core/` platform bağımlılığı sınırı

```text
Problem:      Reset nedeni okumak ESP-IDF cagrisi gerektirir (esp_reset_reason).
              core/ katmani "yalnizca stdlib" olmali (D5).
Selected:     Header (`Diagnostics.h`) platform bagimsiz kalir — ESP-IDF tipi
              disari sizmaz, reset nedeni kendi enum'umuza cevrilir.
              Yalnizca `.cpp` ESP-IDF ve FreeRTOS cagirir.
Gerekçe:      D5'in amaci dongusel bagimlilik ve katman ihlalini onlemek.
              Platform API'si bir KATMAN degil; arayuzu kirletmedigi surece
              implementasyonda kullanilabilir.
```

## Karar 5 — Boot raporu bu task'ta yalnızca **depo**

Aşama kimlikleri ve boot akışı TASK-010 scope'unda. Burada yalnızca sabit
boyutlu kayıt yapısı ve saklama alanı tanımlanır; aşama enum'u TASK-010'a bırakılır.

## Bellek bütçesi (tasarım hedefi)

| Yapı | Hesap | Boyut |
|---|---|---|
| Halka tampon | 64 kayıt × 12 bayt | 768 B |
| Aktif hatalar | 16 slot × 2 bayt + sayaç | ~40 B |
| Boot raporu | 12 aşama × 8 bayt + başlık | ~112 B |
| **Toplam** | | **< 1 KB** |

## Kapsam dışı bırakılanlar

- Kalıcı log dosyası (LittleFS) → TASK-016 sonrası, yalnızca CRITICAL
- `/api/diagnostics` endpoint'i → TASK-043
- OLED ALERTS ekranı → TASK-052
- Boot aşamalarının tanımı ve yürütücüsü → TASK-010

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Dört seviye çalışıyor ve halka tamponda saklanıyor (64 kayıt)
- [x] Kayıt sabit boyutlu (**12 bayt**); dinamik ayırma yok
- [x] `raise()` / `clear()` / `activeFaults()` çalışıyor; alt sistem bitmask'i
      artımlı güncelleniyor, `activeFaultCount()` O(1)
- [x] Çok task'lı erişim güvenli — mutex, sınırlı bekleme (20 ms), ISR koruması
- [x] Reset nedeni okunuyor; WDT ve panic kaynaklı reset **CRITICAL** loglanıyor
- [x] Boot raporu deposu hazır (`BootReport`, 12 aşama slotu) — TASK-010 dolduracak
- [x] Seri çıktı yapılandırılabilir (`setSerialLevel`), halka tampon her zaman aktif

## Ölçümler

| Yapı | Ölçülen | Toplam RAM |
|---|---|---|
| `LogRecord` | 12 bayt | 64 × 12 = **768 B** |
| `FaultSummary` | 38 bayt | — |
| `BootReport` | 76 bayt | 76 B |
| Aktif hata dizisi | 16 × 2 bayt | 32 B |
| **Diagnostics toplam** | | **~900 B** (tasarım hedefi < 1 KB ✔) |

Flash artışı: 266 973 → 267 301 bayt (**+328 bayt**).

## Statik denetimler

```text
Kritik bolge icinde Serial/log/bloklama : 0 ihlal
Sonsuz mutex beklemesi (portMAX_DELAY)  : 0
Heap kullanimi (new/malloc/String)      : 0
```

Kilit sırası tasarımdaki gibi: **kilidi al → kaydı yaz → kilidi bırak → sonra
seri porta yaz.** UART yazma hiçbir zaman kritik bölge içinde değil.

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı. `Diagnostics.cpp` derlendiği için **TASK-004
      header'larının `static_assert`'leri de artık kalıcı olarak çalışıyor**
      (TASK-004'te bırakılan boşluk kapandı).
- [x] Halka tampon indeks matematiği elle doğrulandı (sarmalı ve sarmasız durum):
      `idx = (head + CAP - 1 - i) % CAP` → i=0 en yeni kaydı verir
- [ ] **Tampon taşması çalışma zamanı testi — donanım gerekiyor**
- [ ] **İki task eşzamanlı yazma stres testi — donanım gerekiyor**
- [ ] **Kasıtlı WDT reset sonrası neden raporlaması — donanım gerekiyor**
- [ ] **Log çağrı süresi ölçümü — donanım gerekiyor**

> Çalışma zamanı testleri fiziksel kart erişimi gerektiriyor ve yapılamadı.
> Bu maddeler TASK-060 (entegrasyon) ve TASK-062 (profilleme) kapsamında
> donanım üzerinde kapatılacak. Doğrulanmamış bir şeyi doğrulanmış saymıyorum.

## Review Checklist

- [x] Architecture'a uygun mu? — §2.3 modül sözleşmesi, §16.1 seviyeler,
      §16.2 `{subsystem, code}` yapısı uygulandı
- [x] Gereksiz abstraction var mı? — singleton sınıf yerine namespace + serbest
      fonksiyon; sanal fonksiyon yok, şablon yok
- [x] Blocking işlem var mı? — mutex beklemesi 20 ms ile sınırlı, sonsuz bekleme
      yok; seri port kritik bölge dışında
- [x] Shared state güvenli mi? — tüm paylaşılan durum tek mutex arkasında;
      ISR bağlamı tespit edilip düşürülüyor ve sayılıyor
- [x] Memory problemi var mı? — ~900 B statik, heap kullanımı sıfır
- [x] Error handling var mı? — `begin()` mutex hatasını döndürüyor; tampon ve
      hata listesi taşması sessizce yutulmuyor (`overflow` bayrağı, `droppedFromIsr`)
- [x] ESP32 resource kullanımı uygun mu? — FreeRTOS mutex, ESP-IDF yalnızca
      `.cpp` içinde; header platform bağımsız (D5 korundu)
- [x] Task sorumluluğu doğru mu? — ayrı log task'ı **açılmadı** (ARCHITECTURE §6.4)
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Mevcut sistemin
      emoji tabanlı, seviyesiz `Serial.println` deseni taşınmadı; kayıt makine
      tarafından ayrıştırılabilir sabit formatta ve serbest metin saklanmıyor.

## Bulgular

**ISSUE-009** kaydedildi: `ResetReason::EXTERNAL` derleme hatası verdi çünkü
`Arduino.h` içinde `#define EXTERNAL 0` var. Preprocessor makroları `enum class`
kapsamlamasına saygı duymuyor. Framework taraması yapıldı — projede kullanılan
diğer isimler temiz. Enumerator `EXTERNAL_PIN` olarak yeniden adlandırıldı.
Adlandırma kuralının `CODING_STANDARDS.md`'ye eklenmesi önerildi (o dosya
TASK-003'ün Files listesinde olduğu için bu task'ta değiştirilmedi).

## Durum

**TASK-005: TAMAMLANDI** (çalışma zamanı testleri donanım bekliyor).
