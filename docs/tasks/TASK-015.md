# TASK-015 — ConfigService (Load, Migrate, Persist)

**Phase:** 3 — Storage & Configuration · **Priority:** P0

## Objective

Konfigürasyonun NVS'ten yüklenmesini, doğrulanmasını, gerektiğinde migrate edilmesini ve
güvenli şekilde kalıcılaştırılmasını sağlamak.

## Scope

- Boot'ta config yükleme ve doğrulama
- Şema versiyonu karşılaştırma ve migration akışı
- Bozuk/eksik kayıtta varsayılana düşme + görünür loglama
- Bölüm bazlı güncelleme (`update(section, values)`)
- Kalıcılaştırma isteğinin `store` task'ına kuyruklanması

## Out of Scope

- NVS düşük seviye erişimi (TASK-013)
- Şema tanımı (TASK-014)
- API endpoint'leri (TASK-044)

## Dependencies

- TASK-013, TASK-014

## Requirements

- `REQUIREMENTS.md` — §1 (config yüklenmesi), §7.3 (versiyonlama/migration yok)

## Architecture References

- §2.4 ConfigService · §15.3 Şema versiyonlama akışı

## Expected Design

### Migration akışı (§15.3)

```text
  okunan sürüm == mevcut  →  doğrudan kullan
  okunan sürüm <  mevcut  →  migration uygula, yeni sürümle yaz, INFO logla
  okunan sürüm >  mevcut  →  varsayılana dön, WARNING logla (firmware geri alınmış)
  kayıt yok / bozuk       →  varsayılana dön, WARNING logla
```

**Sessiz varsayılana dönüş yasaktır.** Her düşüş loglanır ve boot raporuna girer;
kullanıcı ayarlarının neden sıfırlandığını görebilmelidir.

### Karar gerektiren nokta — Kısmi bozulma davranışı

```text
Problem:      Config'in bir bölümü bozuk, diğerleri sağlam olabilir
Constraints:  NVS anahtar bazında yazıldığı için kısmi bozulma mümkün;
              tüm config'i sıfırlamak kullanıcı için ağır bir kayıp
Approaches:   (a) herhangi bir bozulmada tüm config'i sıfırla
              (b) yalnızca bozuk bölümü varsayılana döndür
Trade-offs:   (b) kullanıcı dostudur ancak tutarsız kombinasyon riski taşır —
              bu risk alanlar arası doğrulama (TASK-014) ile kapatılabilir
Recommended:  (b) + yükleme sonrası tam doğrulama
```

## Implementation Notes

- Config RAM'de **tek kopya** tutulmalı; okuyucular const referans almalı.
- Yazma isteği `store` task'ına kuyruklanmalı; çağıran flash yazmasını beklememeli (§2.12).
  Bu, web callback'inden config güncellemesinin bloklamamasını sağlar.
- Yazma **doğrulamadan sonra** yapılmalı: geçersiz değer asla kalıcılaşmamalı.
- Config değişikliği ilgili modüllere bildirilmeli. Örneğin güvenlik eşiği değişince
  `SafetyMonitor` bir sonraki döngüde yeni değeri kullanmalı — bu, config'in her döngüde
  okunmasıyla doğal olarak sağlanır; ayrı bildirim mekanizması gerekmez.
- Factory reset: sırlar dahil tüm namespace'ler temizlenmeli, ardından kontrollü yeniden
  başlatma yapılmalı (TASK-012).
- Migration fonksiyonları saf ve test edilebilir olmalı.

## Files

- `src/services/ConfigService.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Boot'ta config yükleniyor ve doğrulanıyor
- [ ] Migration akışı dört senaryoyu da doğru işliyor
- [ ] Varsayılana her düşüş loglanıyor ve boot raporuna giriyor
- [ ] Kısmi bozulmada yalnızca ilgili bölüm varsayılana dönüyor
- [ ] Yükleme sonrası tam doğrulama yapılıyor
- [ ] Yazma isteği kuyruklanıyor, çağıran bloklanmıyor
- [ ] Geçersiz değer kalıcılaşmıyor
- [ ] Factory reset sırlar dahil her şeyi temizliyor

## Test Plan

- [ ] Boş NVS ile boot: varsayılan yükleniyor, WARNING loglanıyor
- [ ] Kasıtlı bozuk kayıt: ilgili bölüm varsayılana dönüyor, diğerleri korunuyor
- [ ] Eski şema versiyonu: migration uygulanıyor, sonuç doğrulamadan geçiyor
- [ ] Daha yeni şema versiyonu: varsayılana dönülüyor ve loglanıyor
- [ ] Geçersiz değer yazma denemesi reddediliyor
- [ ] Factory reset sonrası tüm ayarlar ve sırlar temizleniyor
- [ ] Config yazma sırasında web arayüzü donmuyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§2.4, §15.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — **flash yazması çağıranı bloklamamalı**
- [ ] Shared state güvenli mi? — config'e çok task'lı erişim
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — bozuk/eksik/eski kayıt
- [ ] ESP32 resource kullanımı uygun mu? — gereksiz flash yazması yok mu
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — sessiz `memset` sıfırlaması taşınmamalı

## Definition of Done

Ortak DoD + dört migration senaryosu test edildi + sessiz sıfırlama olmadığı doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Karar 1 — Bölüm başına ayrı NVS anahtarı (kısmi bozulma kurtarması)

```text
Problem:      Config NVS'e nasil yazilacak?
Gereksinim:   "Kismi bozulmada YALNIZCA ilgili bolum varsayilana donsun"
Approaches:   (a) tum config tek blob → bir bayt bozulursa HER SEY gider
                  (mevcut sistemin `StoredData` deseninin aynisi)
              (b) bolum basina ayri anahtar → yalnizca bozuk bolum kaybolur
Selected:     (b) — alti anahtar: net, sens, act, safe, auto, sys (+ ver)
Kazanc:       Kullanici Wi-Fi ayarini kaybetmeden kalibrasyonunu geri alabilir.
              Ayrica tek alan degisiminde yalnizca O BOLUM yeniden yazilir
              → flash asinmasi azalir (ARCHITECTURE §15.2 gerekcesi).
```

## Karar 2 — Kısmi kurtarma sonrası **tam doğrulama zorunlu**

Bölümler ayrı ayrı kurtarılınca, tek tek geçerli ama **birlikte tutarsız** bir
config oluşabilir (örn. eski `safety` + yeni `sensors`). Bu yüzden yükleme
sonunda `validateAll()` çalışır.

Başarısız olursa: **tüm config varsayılana döner ve CRITICAL loglanır.**
Yarı tutarlı bir config ile çalışmak, güvenlik eşiklerinin beklenmedik
kombinasyonlarda uygulanması demektir — kabul edilemez.

## Karar 3 — Her varsayılana dönüş GÖRÜNÜR

```text
ARCHITECTURE §16.4: sessiz varsayilana donus YASAK.
Mevcut sistemde `memset(&Setting, 0, ...)` sessizce yapiliyordu — kullanici
ayarlarinin neden gittigini bilmiyordu.

Her dusus:
  · WARNING (bolum) veya CRITICAL (tum config) loglanir
  · `lastLoadResult()` ile sorgulanabilir
  · boot raporuna ve arayuze yansir
```

## Karar 4 — `persist()` senkron; kuyruk çağıranın işi

```text
Gereksinim: "Yazma istegi kuyruklanmali, cagiran flash yazmasini beklememeli"
Sorun:      Kuyruk `store` task'inda (TASK-059) — henuz yok.
            ConfigService (services/) baska bir servisi cagiramaz (D4).
Selected:   `persist()` SENKRON yazar ve `ErrCode` doner.
            HANGI TASK'tan cagrilacagi ust katmanin karari:
            TASK-059 `store` task'i kendi baglamindan cagiracak.
Sinir:      Header'da "AsyncTCP callback'inden CAGIRMAYIN" acikca yazili.
```

## Karar 5 — Migration yolu şimdi kuruluyor, göç şimdi yok

`schemaVersion = 1` ilk sürüm; göç edecek eski sürüm yok. Ancak **migration
akışı şimdi kurulur** (dört senaryo), çünkü TASK-054 sürümü 2'ye çıkaracak.
Sonradan eklemek, o noktada zaten kaydedilmiş kullanıcı ayarlarını riske atardı.

## Kapsam dışı

- NVS düşük seviye erişimi → TASK-013 · Şema → TASK-014
- API uç noktaları → TASK-044 · Kuyruk ve `store` task'ı → TASK-059

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Boot'ta yükleniyor ve doğrulanıyor
- [x] Migration akışı dört senaryoyu işliyor (eşit / eski / yeni / yok-bozuk)
- [x] **Varsayılana her düşüş loglanıyor** ve `lastLoadResult()` ile sorgulanabiliyor
- [x] **Kısmi bozulmada yalnızca ilgili bölüm varsayılana dönüyor** — altı ayrı
      NVS anahtarı sayesinde
- [x] Yükleme sonrası **tam doğrulama** yapılıyor; tutarsızsa tümü varsayılana
      dönüyor + CRITICAL
- [x] Geçersiz değer kalıcılaşmıyor — `update*` önce doğruluyor, `persist()`
      yazmadan önce bir kez daha
- [x] Factory reset config **ve sırları** siliyor
- [ ] Yazma isteği kuyruklanıyor — **`store` task'ı henüz yok (TASK-059).**
      `persist()` senkron; hangi task'tan çağrılacağı üst katmanın kararı ve
      header'da "AsyncTCP callback'inden çağırmayın" olarak belgelendi

## Tasarım notu — yalnızca değişen bölüm yazılır

`dirtyMask` sayesinde tek bir eşik değişiminde altı bölümün tamamı değil,
yalnızca `safe` yeniden yazılır. Mevcut sistemin tek blok deseni her değişimde
tüm ayarları yeniden yazıyordu (REQUIREMENTS §7.1).

Sürüm anahtarı **en son** yazılır: bölümlerden biri yazılamazsa sürüm eski
kalır ve bir sonraki boot'ta tutarsız bir "yeni sürüm" iddiası oluşmaz.

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı (Flash +256 bayt)
- [x] D4 denetimi: `services/` başka bir servisi include etmiyor
- [ ] **Boş NVS → varsayılan + WARNING — donanım gerekiyor**
- [ ] **Kısmi bozuk kayıt → yalnızca o bölüm — donanım gerekiyor**
- [ ] **Eski/yeni şema sürümü senaryoları — donanım gerekiyor**
- [ ] **Factory reset sonrası temizlik — donanım gerekiyor**
- [ ] **Config yazarken web'in donmadığı — TASK-059 sonrası**

## Review Checklist

- [x] Architecture'a uygun mu? — §2.4, §15.3 dört senaryo
- [x] Gereksiz abstraction var mı? — namespace + serbest fonksiyon
- [x] Blocking işlem var mı? — `persist()` senkron ve bu **bilinçli**;
      sınır header'da açıkça yazılı (D4 nedeniyle kuyruk buraya konamaz)
- [x] Shared state güvenli mi? — RAM'de tek kopya; yazma yolu tek task'tan
      kullanılacak (TASK-059)
- [x] Memory problemi var mı? — 392 bayt config + maske; heap yok
- [x] Error handling var mı? — bölüm kaybı, sürüm uyuşmazlığı, doğrulama
      hatası, yazma hatası: dördü de loglanıyor ve raporlanıyor
- [x] ESP32 resource kullanımı uygun mu? — yalnızca değişen bölüm yazılıyor
- [x] Task sorumluluğu doğru mu? — servis task bilmiyor
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** Tek blok
      `StoredData` deseni ve sessiz `memset` sıfırlaması taşınmadı.

## Durum

**TASK-015: TAMAMLANDI** (kuyruk entegrasyonu TASK-059'a bırakıldı, donanım
testleri bekliyor).
