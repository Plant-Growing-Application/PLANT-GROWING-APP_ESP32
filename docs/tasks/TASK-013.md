# TASK-013 — NvsStore & Secret Store

**Phase:** 3 — Storage & Configuration · **Priority:** P0

## Objective

Kalıcı anahtar-değer deposunu kurmak ve sırları (Wi-Fi şifresi, arayüz parola hash'i)
normal konfigürasyondan ayrı, korumalı bir alanda saklamak.

## Scope

- NVS sarmalayıcısı: namespace yönetimi, tipli oku/yaz, silme
- Ayrı `secrets` namespace'i ve sır erişim arayüzü
- Yazma hata kontrolü ve raporlama
- Namespace bazlı temizleme (factory reset için altyapı)

## Out of Scope

- Config şeması ve doğrulama (TASK-014)
- ConfigService yükleme/migration mantığı (TASK-015)
- Parola hash algoritması ve token üretimi (TASK-042)
- Dosya sistemi (TASK-016)

## Dependencies

- TASK-004, TASK-005

## Requirements

- `REQUIREMENTS.md` — §7 (ham EEPROM kullanımı), §9 (şifre düz metin)

## Architecture References

- §2.15 NvsStore
- §15.1 Veri sınıflandırması · §15.2 Ham EEPROM'un terk edilme gerekçesi

## Expected Design

### Karar gerektiren nokta — Neden NVS, neden `EEPROM.h` değil

Mevcut proje tüm ayarları tek bir `StoredData` bloğu olarak yazıyor. Üç problem:

```text
1. Tek alan değişse bile tüm blok yeniden yazılıyor  → gereksiz flash aşınması
2. Yazma sırasında güç kesilirse TÜM config kayboluyor → tek nokta hatası
3. Yapı değişince eski kayıt tamamen geçersiz oluyor  → migration imkânsız
```

NVS bu üçünü de çözer: anahtar bazında atomik yazma, aşınma dengeleme, alan bazında
evrim. **Karar verilmiş sayılır**; geliştirici yalnızca namespace düzenini tasarlar.

### Sır yönetimi kuralları

- Sırlar **ayrı namespace**'te tutulmalı; normal config okumaları sırlara erişmemeli.
- Sır okuma arayüzü **maskeleme desteklemeli**: "var mı?" sorusu şifreyi döndürmeden
  yanıtlanabilmeli. API ve OLED bu yolu kullanacak.
- Wi-Fi şifresi hiçbir zaman `SystemState`'e, log'a veya API yanıtına girmemeli.

## Implementation Notes

- NVS bölümü dolduğunda yazma başarısız olur; bu durum ele alınmalı ve ERROR olarak
  raporlanmalı. Sessiz başarısızlık, ayarların kaydedilmediğini gizler.
- NVS init hatası (bölüm bozuk) durumunda yeniden biçimlendirme seçeneği vardır ancak
  bu **tüm ayarları siler** — otomatik yapılmadan önce loglanmalı ve raporlanmalı.
- Anahtar isimleri NVS uzunluk sınırına (15 karakter) uymalı.
- Yazma çağrıları `store` task'ından yapılmalı; çağıran task flash yazmasını beklememeli
  (§2.12). Bu task API'yi sağlar, kuyruk entegrasyonu TASK-015'te yapılır.
- Eski `EEPROM` alanından veri taşıma **yapılmayacak**; kullanıcı Wi-Fi bilgilerini yeniden
  girer. Gerekçe: eski yapı zaten güvenilmez ve şifre düz metin saklanmış.

## Files

- `src/hal/NvsStore.h` / `.cpp` (yeni)
- `src/hal/SecretStore.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Namespace bazlı tipli oku/yaz çalışıyor
- [ ] Sırlar ayrı namespace'te
- [ ] "Sır var mı?" sorgusu değeri döndürmeden yanıtlanabiliyor
- [ ] Yazma hataları raporlanıyor, sessiz başarısızlık yok
- [ ] NVS init hatası ele alınıyor ve loglanıyor
- [ ] Namespace temizleme (factory reset altyapısı) çalışıyor
- [ ] Anahtar isimleri uzunluk sınırına uygun

## Test Plan

- [ ] Yaz → yeniden başlat → oku döngüsü değeri koruyor
- [ ] Güç kesme simülasyonu (yazma sırasında reset) diğer anahtarları bozmuyor
- [ ] NVS dolu senaryosunda yazma hatası doğru raporlanıyor
- [ ] Bozuk NVS senaryosunda sistem boot ediyor ve durumu raporluyor
- [ ] Sır maskeleme doğrulandı: log ve API çıktısında şifre görünmüyor
- [ ] Namespace temizleme sonrası ayarlar varsayılana dönüyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§15)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — flash yazması çağıranı bloklamamalı
- [ ] Shared state güvenli mi? — çok task'lı NVS erişimi
- [ ] Memory problemi var mı?
- [ ] Error handling var mı? — dolu/bozuk NVS
- [ ] ESP32 resource kullanımı uygun mu? — flash aşınması
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`StoredData` blok deseni taşınmamalı**

## Definition of Done

Ortak DoD + güç kesme testi geçti + şifrenin hiçbir çıktıda görünmediği doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-30 · **Durum:** Karara bağlandı

## Doğrulanmış API

```text
nvs_flash_init / nvs_open / nvs_set_str / nvs_get_str
nvs_set_blob / nvs_get_blob / nvs_commit / nvs_erase_key / nvs_erase_all
NVS_KEY_NAME_MAX_SIZE = 16   → 15 kullanilabilir karakter
```

## Karar 1 — Arduino `Preferences` değil, ham NVS C API

```text
Problem:      Arduino `Preferences` sinifi mi, ham nvs_* API mi?
Constraints:  CODING_STANDARDS Y11: donus degeri kontrol edilmeden birakilmaz;
              hata kodu makine tarafindan karsilastirilabilir olmali (§16.2)
Approaches:   (a) Preferences — basit ama HATA YUTAR:
                  putString() basarisizlikta 0 doner, NEDEN'i soylemez
                  (dolu mu, bozuk mu, anahtar mi uzun — ayirt edilemez)
              (b) ham nvs_* — esp_err_t doner, ErrCode'a eslenebilir
Selected:     (b) — hata nedeni kaybolmamali. STORAGE_FULL ile
              STORAGE_WRITE_FAILED farkli davranislar gerektirir.
```

## Karar 2 — Namespace ayrımı ve sır maskeleme

```text
"cfg"  → konfigurasyon (TASK-015 kullanacak)
"sec"  → SIRLAR: Wi-Fi sifresi, arayuz parola hash'i

Ayri namespace SART: config okumalari sirlara ERISEMEZ. Bir JSON
serilestirici yanlislikla tum config namespace'ini dolasirsa sifre sizmaz.

Sir arayuzu MASKELEME destekler:
    hasSecret(key) → bool        // "var mi?" — DEGERI DONDURMEDEN
    getSecret(key, buf, len)     // yalnizca gercekten gerekli oldugunda

`hasSecret()` API ve OLED tarafindan kullanilacak (ARCHITECTURE §8.2:
sifre state'e, log'a, API yanitina ve OLED'e ASLA girmez).
```

## Karar 3 — NVS bölümü bozuksa: sessiz biçimlendirme YOK

```text
nvs_flash_init() su hatalari dondurebilir:
    ESP_ERR_NVS_NO_FREE_PAGES      → bolum dolu
    ESP_ERR_NVS_NEW_VERSION_FOUND  → farkli surumle yazilmis

Her ikisinin standart cozumu nvs_flash_erase() + yeniden init.
ANCAK bu TUM AYARLARI SILER — Wi-Fi bilgileri, kalibrasyon, esikler.

Selected: Biçimlendirme YAPILIR (aksi halde sistem hic calisamaz) ama
          SESSIZ DEGIL: CRITICAL loglanir, boot raporuna girer ve
          kullaniciya "ayarlariniz sifirlandi" olarak gorunur.
Gerekce:  ARCHITECTURE §16.4 — sessiz varsayilana donus yasak.
          Kullanici ayarlarinin neden gittigini bilmeli.
```

## Karar 4 — Eski EEPROM'dan veri taşınmayacak

```text
Mevcut sistem ayarlari ham EEPROM'da `StoredData{magic, Settings}` blogu
olarak tutuyordu. Tasima YAPILMAYACAK:
  · sifre orada DUZ METIN — tasimak guvenlik acigini surdurur
  · yapi zaten guvenilmez (tek blok, kismi yazmada tumu kaybolur)
  · kullanicinin Wi-Fi bilgisini yeniden girmesi tek seferlik kucuk maliyet
Karar REQUIREMENTS §7.1 ve ARCHITECTURE §15.2 ile tutarli.
```

## Karar 5 — Yazma çağıranı bloklamaz mı?

Flash yazma yavaştır. Ancak bu **sürücü katmanıdır** (L1): senkron çalışır ve
`esp_err_t` döndürür. "Çağıran beklemesin" kuralı üst katmanın sorumluluğudur —
`store` task'ı (TASK-059) bu sürücüyü kendi bağlamından çağıracaktır.

Sürücüye kuyruk koymak katman ihlali olurdu (D6: sürücüde iş kuralı yok).
Header'da bu sınır belgelendi.

## Kapsam dışı

- Config şeması → TASK-014 · Yükleme/migration → TASK-015
- Parola hash'i ve token → TASK-042 (burada yalnızca **saklama** var)
- Dosya sistemi → TASK-016

---

# STEP 3 — REVIEW RECORD

## Acceptance Criteria

- [x] Namespace bazlı tipli oku/yaz çalışıyor (`u32`, `string`, `blob`)
- [x] Sırlar **ayrı namespace**'te (`sec` ≠ `cfg`)
- [x] **"Sır var mı?" değeri döndürmeden yanıtlanıyor** — `exists()` yalnızca
      boyut sorgusu yapıyor, değeri belleğe getirmiyor
- [x] Yazma hataları raporlanıyor; `esp_err_t` → `ErrCode` eşlemesi neden
      bilgisini koruyor (`STORAGE_FULL` ≠ `STORAGE_WRITE_FAILED`)
- [x] NVS init hatası ele alınıyor; **yeniden biçimlendirme CRITICAL loglanıyor**
      ve `wasReformatted()` ile sorgulanabiliyor
- [x] Namespace temizleme çalışıyor (factory reset altyapısı)
- [x] Anahtar isimleri sınıra uygun — `wifi_pass` (9), `auth_hash` (9) ≤ 15

## Statik denetimler

```text
Sifre log/Serial cikisinda           : 0
Arduino Preferences kullanimi        : 0  (ham nvs_* API)
Kontrol edilmeyen donus degeri       : 0  (tum nvs_* cagrilari maplenip donuyor)
```

## Test Plan

- [x] Derleme SUCCESS, 0 uyarı (Flash +536 bayt)
- [x] Anahtar uzunlukları doğrulandı
- [x] Log satırlarında şifre olmadığı doğrulandı
- [ ] **Yaz → reset → oku döngüsü — donanım gerekiyor**
- [ ] **Yazma sırasında güç kesme — donanım gerekiyor**
- [ ] **NVS dolu senaryosu — donanım gerekiyor**
- [ ] **Bozuk NVS → biçimlendirme + rapor — donanım gerekiyor**
- [ ] **Flash dökümünde şifrenin düz metin olmadığı — donanım gerekiyor**
      (TASK-063 güvenlik denetiminin bir maddesi)

## Review Checklist

- [x] Architecture'a uygun mu? — §15.1 veri sınıflandırması, §15.2 EEPROM terki
- [x] Gereksiz abstraction var mı? — serbest fonksiyonlar; sınıf/şablon yok
- [x] Blocking işlem var mı? — flash yazma senkron ve **bilinçli**: bu bir L1
      sürücüsüdür, kuyruk koymak iş kuralı eklemek olurdu (D6). Çağıran task
      seçimi `store` task'ının (TASK-059) sorumluluğu; sınır header'da yazılı
- [x] Shared state güvenli mi? — NVS'in kendi kilitlemesi var; handle'lar
      çağrı ömrü boyunca lokal, sızdırılmıyor
- [x] Memory problemi var mı? — sabit tamponlar, heap yok
- [x] Error handling var mı? — **bu task'ın ana konusu.** Sessiz başarısızlık
      yok: dolu/bozuk/uzun anahtar/bulunamadı ayrı kodlara eşleniyor
- [x] ESP32 resource kullanımı uygun mu? — NVS aşınma dengelemesi devrede;
      tek blok yazma deseni terk edildi
- [x] Task sorumluluğu doğru mu? — sürücü task bilmiyor
- [x] **Eski kod gereksiz şekilde kopyalanmış mı? — hayır.** `StoredData`
      blok deseni taşınmadı. Eski EEPROM'dan **veri göçü de yapılmadı**:
      şifre orada düz metindi ve taşımak güvenlik açığını sürdürürdü.

## Durum

**TASK-013: TAMAMLANDI** (donanım testleri bekliyor).
