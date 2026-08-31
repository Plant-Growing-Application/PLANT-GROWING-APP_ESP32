# TASK-050 — ViewModelBuilder

**Phase:** 11 — Display · **Priority:** P2

## Objective

Snapshot'ı ekranda çizilebilir saf veriye dönüştüren katmanı kurmak. Display mantığının
donanımsız test edilebilmesini sağlamak.

## Scope

- ViewModel yapıları (ekran başına)
- Snapshot → ViewModel dönüşümü (saf fonksiyon)
- Değer biçimlendirme (birim, ondalık, kısaltma)
- Kalite → gösterim dönüşümü ("—", uyarı işareti)
- Değişim tespiti için karşılaştırma desteği

## Out of Scope

- Çizim (TASK-052)
- Navigasyon (TASK-051)
- OLED sürücüsü (TASK-020)

## Dependencies

- TASK-007

## Requirements

- `REQUIREMENTS.md` — §6 (display), §6.3 (sensör okuma ile çizimin karışması)

## Architecture References

- §13.1 ViewModel deseni · §13.2 Display kuralları

## Expected Design

### Saf dönüşüm

```text
   ViewModelBuilder(snapshot, activeScreen) → ViewModel
```

- Yan etkisi yok
- Donanıma dokunmaz
- Global duruma bakmaz
- Aynı girdi → aynı çıktı

Bu özellik sayesinde ekran mantığı **host tarafında test edilebilir** (TASK-064).
Mevcut projede ekran mantığı doğrudan `oled` nesnesine yazıyordu; test edilmesi imkânsızdı.

### Biçimlendirme kuralları

| Durum | Gösterim |
|---|---|
| Kalite `OK` | Değer + birim |
| Kalite `FAULT` | "—" |
| Kalite `NOT_PRESENT` | "yok" |
| Kalite `STALE` / `OUT_OF_RANGE` | Değer + uyarı işareti |
| Zaman geçersiz | "--:--" (sahte "00:00:00" **değil**) |
| IP yok | "bağlı değil" |

### Kaynak kısıtı

128×64 mono ekranda **çok az yer** vardır. ViewModel bu kısıtı bilmeli: uzun metinler
kısaltılmalı, öncelik sırası belirlenmeli. Ne gösterileceği kadar **ne gösterilmeyeceği**
de tasarım kararıdır.

## Implementation Notes

- ViewModel sabit boyutlu string tamponları kullanmalı; `String` sınıfı ve heap yok.
- Karşılaştırma desteği kirli alan tespiti için gerekli: aynı ViewModel ise ekran
  yeniden çizilmemeli (I2C yükü ve titreme).
- Wi-Fi şifresi ViewModel'e **girmez** (§8.2). Mevcut projede OLED'de açıkça gösteriliyordu.
- Sayısal biçimlendirme yerel ayara bağlı olmamalı; sabit format kullanılmalı.
- Ekran genişliğine sığmayan metinler için kısaltma stratejisi belirlenmeli (kesme,
  kaydırma, elipsis).
- Her ekran için ayrı ViewModel tipi mi, tek birleşik tip mi? Ayrı tipler daha açık ancak
  daha fazla kod; karar gerekçelendirilmeli.

## Files

- `src/interfaces/ui/ViewModels.h` (yeni)
- `src/interfaces/ui/ViewModelBuilder.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Dönüşüm saf; yan etkisi yok, donanıma dokunmuyor
- [ ] Kalite → gösterim dönüşümü tabloya uygun
- [ ] Zaman geçersizken sahte saat gösterilmiyor
- [ ] Sabit boyutlu tamponlar; heap yok
- [ ] Karşılaştırma desteği var (kirli alan tespiti için)
- [ ] Wi-Fi şifresi ViewModel'de yok
- [ ] Uzun metin kısaltma stratejisi tanımlı
- [ ] Host tarafında test edilebiliyor

## Test Plan

- [ ] Host tarafında sahte snapshot'lar ile tüm ekranların ViewModel'i üretildi
- [ ] Her kalite durumu doğru biçimlendiriliyor
- [ ] Zaman geçersizken "--:--" gösteriliyor
- [ ] Aynı snapshot için aynı ViewModel üretiliyor (saf fonksiyon kanıtı)
- [ ] Uzun SSID/IP değerleri doğru kısaltılıyor
- [ ] Karşılaştırma değişimi doğru tespit ediyor
- [ ] Şifrenin ViewModel'de olmadığı doğrulandı

## Review Checklist

- [ ] Architecture'a uygun mu? (§13.1)
- [ ] Gereksiz abstraction var mı? — ekran başına ayrı tip gerekli miydi
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? — snapshot kopyası kullanılıyor mu
- [ ] Memory problemi var mı? — heap yok, tampon boyutları
- [ ] Error handling var mı? — kalite gösterimi
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **doğrudan `oled.print()` deseni yasak**

## Definition of Done

Ortak DoD + host tarafında tüm ekran ViewModel'leri test edildi + saflık doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — TEK birleşik ViewModel, ekran başına ayrı tip DEĞİL

```text
Ayri tipler daha acik ama: 7 ekran × (tip + donusturucu + karsilastirici)
= 21 birim kod. 128×64 bir ekranin gosterdigi veri kumesi ZATEN kucuk ve
ekranlar arasinda BUYUK OLCUDE ORTAK (durum cubugu her ekranda ayni).

SECILDI: tek `UiModel`. Ekranlar ondan kendi ihtiyaclarini okur.
Boyut: ~200 bayt, sabit tamponlar, heap YOK.
```

## Karar 2 — Dönüşüm SAF

`build(snapshot, nav) → UiModel`. Yan etki yok, donanım yok, global yok.
Aynı girdi → aynı çıktı. Ekran mantığı **host tarafında test edilebilir**;
eski projede ekran mantığı doğrudan `oled` nesnesine yazıyordu ve test
edilmesi imkânsızdı.

## Karar 3 — Kalite → gösterim tablosu (§9.2'nin OLED karşılığı)

| Kalite | Gösterim |
|---|---|
| `OK` | değer + birim |
| `FAULT` | `—` (**eski değer ASLA gösterilmez**) |
| `NOT_PRESENT` | `yok` |
| `STALE` / `OUT_OF_RANGE` | değer + `!` |

**Arızalı sensörün eski değerini göstermek, mevcut projedeki en tehlikeli
gösterim hatasıydı.** Operatör 20 dakika önce donmuş bir pH değerine bakıp
gübre ekleyebilir.

## Karar 4 — Zaman geçersizken `--:--`, sahte `00:00` DEĞİL

Eski `getFormattedTime()` senkronize değilken sessizce `"00:00:00"`
döndürüyordu.

## Karar 5 — Wi-Fi şifresi ViewModel'e GİRMEZ

Eski projede WIFI sayfası şifreyi açıkça yazıyordu. `UiModel` içinde şifre
alanı **yoktur** — sızması için önce bir alan eklenmesi gerekir.

**İstisna:** AP kurulum şifresi (`apPassword`) gösterilir ve gösterilmek
ZORUNDADIR — kullanıcı cihaza bağlanmak için onu ekrandan okur. O şifre
cihazın kendi ürettiği kurulum şifresidir, kullanıcının ev ağı şifresi
değildir (TASK-038).

## Karar 6 — Karşılaştırma ile kirli tespiti

`UiModel` POD; `memcmp` ile karşılaştırılır. Aynıysa **çizilmez** — I2C
yükü ve titreme önlenir.

---

# STEP 3 — REVIEW RECORD

- [x] Tek birleşik `UiModel`; POD, sabit tamponlar, **heap yok**
- [x] `build()` **saf**: yan etki yok, donanım yok, global yok
- [x] Kalite → gösterim tablosu uygulandı
- [x] **Arızalı sensörün eski değeri gösterilmiyor** → `"--"`
- [x] Zaman geçersizken `--:--`, sahte `00:00` değil
- [x] IP yokken "bagli degil"
- [x] Wi-Fi şifresi için **alan yok** — sızması için önce alan eklenmesi gerekir
- [x] `sameAs()` `memcmp` ile; `build()` başında tam `memset` yapıldığı için
      dolgu baytları da deterministik
- [x] Sayısal biçimlendirme sabit format (`snprintf`), yerel ayara bağlı değil
- [ ] **`UiModel` boyutu ve çizim tetikleme oranı donanımda ölçülmedi**

## `apPassword` neden var

Kurulum AP şifresi **gösterilmek zorundadır**: kullanıcı cihaza bağlanmak
için onu 128×64 ekrandan okur (TASK-038). Bu, cihazın kendi ürettiği
kurulum şifresidir — kullanıcının ev ağı şifresi değildir ve o hiçbir
ekranda görünmez.

**TASK-050: TAMAMLANDI.**
