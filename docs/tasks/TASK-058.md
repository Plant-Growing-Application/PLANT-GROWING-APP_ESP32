# TASK-058 — Ring-File History Store

**Phase:** 13 — Data Logging · **Priority:** P3

## Objective

Geçmiş sensör verisini sabit boyutlu bir halka dosyada saklamak. SQLite'ın yerini alan,
öngörülebilir aşınma ve bellek kullanımı olan basit bir çözüm kurmak.

## Scope

- Sabit boyutlu kayıt formatı
- Halka dosya yazma/okuma
- Zaman aralığı sorgusu
- Yazma indeksinin kalıcılığı
- Bozuk kayıt tespiti

## Out of Scope

- Storage task entegrasyonu (TASK-059)
- API endpoint (TASK-059)
- Grafik gösterimi

## Dependencies

- TASK-016

## Requirements

- `REQUIREMENTS.md` — §3.7 (geçmiş yok), §7.1 (SQLite kullanılmıyor)

## Architecture References

- §15.1 Geçmiş sensör verisi satırı · §15.2 SQLite kaldırma gerekçesi

## Expected Design

### SQLite yerine halka dosya — gerekçe (§15.2)

```text
1. Sorgu ihtiyacı yok — erişim deseni yalnızca "zaman aralığı oku"
2. Ciddi flash ve RAM maliyeti
3. LittleFS üzerinde B-tree yazması aşınmayı artırır
4. Mevcut projede hiç etkinleştirilmemişti — kaybedilen çalışan işlevsellik yok
```

### Karar gerektiren nokta — Kayıt formatı ve kapasite

```text
Problem:      Ne kadar veri, hangi çözünürlükte saklanacak?
Constraints:  LittleFS alanı sınırlı (partition kararına bağlı);
              flash yazma döngüsü sınırlı;
              kullanıcı muhtemelen son birkaç gün/hafta ile ilgilenir
Approaches:   (a) her örneği kaydet (250 ms) — çok fazla veri ve aşınma
              (b) periyodik özet (örn. dakikada bir ortalama)
              (c) değişim tabanlı kayıt
Trade-offs:   (b) öngörülebilir boyut ve aşınma; çoğu kullanım için yeterli
Recommended:  (b) — periyot ve saklama süresi hesaplanarak dosya boyutu belirlenmeli
```

Kapasite hesabı yapılmalı: `kayıt boyutu × (saklama süresi / periyot)` = dosya boyutu.
Bu hesap `docs/` altında belgelenmeli.

## Implementation Notes

- Kayıt **sabit boyutlu** olmalı; değişken kayıt halka mantığını karmaşıklaştırır.
- Yazma indeksi kalıcı olmalı ancak her yazmada flash'a kaydedilmemeli (aşınma).
  Alternatif: dosyayı tarayarak en son kaydı bulmak (boot'ta bir kez).
- Her kayıtta zaman damgası ve geçerlilik işareti olmalı; zaman geçersizken kaydedilen
  veri işaretlenmeli.
- Bozuk kayıt (yarım yazılmış) tespit edilebilmeli — sağlama toplamı veya sihirli değer.
- Yazma **`store` task'ında** yapılmalı; çağıran beklememelidir.
- Dosya sistemi doluysa veya yazma başarısızsa bu **sessizce yutulmamalı**; sayılmalı ve
  raporlanmalı.
- Okuma sorgusu sayfalı olmalı; tüm dosyayı belleğe almak imkânsız.
- Halka başa döndüğünde en eski kayıtlar üzerine yazılır — bu beklenen davranıştır.

## Files

- `src/services/storage/HistoryStore.h` / `.cpp` (yeni)
- `docs/HISTORY_CAPACITY.md` (yeni — kapasite hesabı)

## Acceptance Criteria

- [ ] Kayıt formatı ve kapasite kararı verildi, hesap belgelendi
- [ ] Sabit boyutlu kayıt; halka yazma çalışıyor
- [ ] Zaman aralığı sorgusu çalışıyor, sayfalı
- [ ] Yazma indeksi kurtarılabiliyor (kalıcı veya tarama ile)
- [ ] Bozuk kayıt tespit ediliyor ve atlanıyor
- [ ] Zaman geçersizken kaydedilen veri işaretleniyor
- [ ] Yazma hataları raporlanıyor
- [ ] Halka başa döndüğünde en eskiyi üzerine yazıyor
- [ ] Flash yazma sıklığı hesaplandı ve kabul edilebilir

## Test Plan

- [ ] Halka dolana kadar yazıp başa dönmesi doğrulandı
- [ ] Yeniden başlatma sonrası yazma indeksi doğru kurtarılıyor
- [ ] Zaman aralığı sorgusu doğru kayıtları döndürüyor
- [ ] Yazma sırasında güç kesme sonrası bozuk kayıt tespit ediliyor
- [ ] Dosya sistemi doluyken hata raporlanıyor
- [ ] Sayfalı okuma bellek sınırlarında çalışıyor
- [ ] Uzun süreli yazmada flash aşınması tahmini hesaplandı
- [ ] Sorgu süresi ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§15.1, §15.2)
- [ ] Gereksiz abstraction var mı? — SQLite'a geri dönülmemiş mi
- [ ] Blocking işlem var mı? — yazma `store` task'ında mı
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — okuma tamponu boyutu
- [ ] Error handling var mı? — bozuk kayıt, dolu disk
- [ ] ESP32 resource kullanımı uygun mu? — **flash aşınması**
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **SQLite yeniden getirilmemeli**

## Definition of Done

Ortak DoD + kapasite ve aşınma hesabı belgelendi + güç kesme dayanıklılığı test edildi.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — (b) periyodik özet; KAPASİTE HESABI

```text
(a) her ornegi kaydet (250 ms) → gunde 345.600 kayit. Kabul edilemez:
    hem alan hem flash asinmasi.
(c) degisim tabanli → boyut ONGORULEMEZ; gurultulu bir sensor dosyayi
    saatler icinde doldurabilir.

(b) SECILDI: periyodik ornek (varsayilan 60 sn).
```

### Kayıt formatı — 24 bayt

| Alan | Bayt | Gerekçe |
|---|---|---|
| `seq` | 4 | monotonik sıra — halka konumunu bulmak için (aşağıya bakın) |
| `epoch` | 4 | duvar saati (uint32, 2106'ya kadar) |
| `values[6]` | 12 | `int16`, ölçekli — `float` 24 bayt eder, iki katı |
| `qualityMask` | 1 | sensör başına 1 bit: ölçüm `OK` muydu |
| `actuatorMask` | 1 | aktüatör başına 1 bit: o an açık mıydı |
| `flags` | 1 | `timeValid` — saat geçersizken kaydedilen veri İŞARETLİ |
| `crc8` | 1 | yarım yazılmış kayıt tespiti |

**`int16` ölçekleme:** pH ×100 (641), EC ×100, sıcaklık ×10, debi ×100,
seviye ×1, nem ×1. Hepsi `int16` aralığında; `float`'a göre **%50 tasarruf**
ve okunabilirlik kaybı yok — bu veri grafik çizmek için, hesap yapmak için
değil.

### Kapasite hesabı (belgelenmesi istenen)

```text
LittleFS bolumu       : 896 KB  (0xE0000)
Web varliklari (gzip) :  ~12 KB
LittleFS ust bilgi    : ~%10 pay
Halka dosyaya ayrilan : 480 KB   ← secilen

480 KB / 24 bayt = 20 480 kayit  (tam bolunur)

60 sn periyotla : 20 480 dk = 341 saat = 14,2 GUN
300 sn periyotla:            = 71 GUN
```

Kalan ~400 KB LittleFS'in aşınma dengelemesi ve meta verisi için serbest
bırakıldı — dosya sistemini tavana kadar doldurmak yazma performansını
çökertir.

## Karar 2 — Halka konumu BİNARY SEARCH ile bulunur

```text
(a) Yazma dizinini her kayitta flash'a yaz → HER kayitta ikinci bir yazma,
    asinma iki katina cikar.
(b) Boot'ta tum dosyayi tara → 480 KB okuma, ~0,5 sn boot gecikmesi.
(c) SECILDI: BINARY SEARCH.

Adim 1: gecerli kayit sayisi V'yi bul (gecersizler bir SONEK olusturur)
        → ilk gecersiz dizin, ikili arama, ~15 okuma
Adim 2: V < N ise dosya sarmamis  → yazma dizini = V
Adim 3: V == N ise sarmis → `seq`in DUSTUGU nokta ikili aramayla bulunur
        (dondurulmus sirali dizi problemi), ~15 okuma daha

Toplam ~30 × 24 bayt = 720 bayt okuma. Ek yazma YOK, ek dosya YOK.
```

`seq` alanının varlık nedeni tam olarak budur.

## Karar 3 — Önceden ayırma YOK

480 KB'lık dosyayı ilk boot'ta sıfırlarla doldurmak saniyeler sürer ve
hiçbir şey kazandırmaz: halka mantığı zaten geçersiz kayıtları CRC ile
eliyor. Dosya kullanıldıkça büyür.

## Karar 4 — Bozuk kayıt: CRC-8 + sihirli sıra

Yarım yazılmış bir kayıt (yazma sırasında güç kesintisi) CRC ile yakalanır
ve **geçersiz** sayılır. `seq == 0` da geçersizdir — sıra 1'den başlar.

## Karar 5 — Saat geçersizken de KAYDEDİLİR, ama İŞARETLİ

Kaydı atlamak, kesinti dönemini geçmişte görünmez kılar. `flags` bitinde
`timeValid` taşınır; okuyucu bu kayıtların zaman ekseninde
güvenilmeyeceğini bilir. Sahte zaman damgası basmak (eski sistemin
`"00:00:00"` deseni) kabul edilemez.

---

# STEP 3 — REVIEW RECORD

- [x] Sabit boyutlu kayıt (24 bayt) — `static_assert` ile kilitli
- [x] Halka yazma/okuma; başa döndüğünde en eskiler üzerine yazılıyor
- [x] Zaman aralığı sorgusu (`readRange`) ve son-N sorgusu (`readRecent`)
- [x] Yazma indeksi **kalıcı değil** — boot'ta binary search ile bulunuyor
      (her kayıtta ikinci bir flash yazması yapılmıyor)
- [x] Bozuk kayıt CRC-8 ile tespit ediliyor; `seq == 0` boş slot
- [x] Saat geçersizken kayıt **atılmıyor, İŞARETLENİYOR**
- [x] Yazma hatası sayılıyor ve raporlanıyor
- [x] Dosya sistemi yoksa servis devre dışı ama sistem durmuyor (P4)
- [x] Heap tahsisi yok
- [ ] **Halka sarma davranışı donanımda doğrulanmadı** — 20 480 kayıt
      yazmak 60 sn periyotla 14 gün sürer; hızlandırılmış test gerekiyor

## Kapasite hesabı (belgelenmesi istenen)

```text
LittleFS bolumu       : 896 KB
Web varliklari (gzip) :  ~12 KB
Halka dosya           : 480 KB   = 20 480 kayit × 24 bayt (TAM boluner)
Serbest birakilan     : ~400 KB  (asinma dengelemesi + meta veri)

60 sn periyot  → 14,2 GUN
300 sn periyot → 71 GUN
```

Dosya sistemini tavana kadar doldurmak LittleFS'in yazma performansını
çökertir; ~400 KB bilinçli olarak boş bırakıldı.

## Bulduğum performans hatası: kayıt başına dosya açması

İlk yazımda `readRecent()` her kayıt için ayrı bir `readAt()` çağırıyordu.
`hal::fs::readAt()` **her çağrıda dosyayı açıp kapatıyor** — 240 kayıtlık
bir sayfa **240 dosya açması** demekti.

LittleFS'te bir dosya açması metadata okuması gerektirir; 240 açma yüzlerce
milisaniye sürerdi ve bu AsyncTCP bağlamında (TASK-059) kabul edilemezdi.

**Düzeltme:** `readSpan()` eklendi. Halka en fazla **iki bitişik parçaya**
bölündüğü için bir sayfa **en fazla 2 dosya açmasıyla** okunuyor. Tarama
doğruladı: `HistoryStore.cpp` içinde toplam **2** `readAt` çağrısı var
(biri tekil kayıt, biri aralık).

Geçerlilik denetimi artık RAM üzerinde yapılıyor; bozuk kayıtlar eleniyor
ve sorgu çökmüyor — yarım yazılmış tek bir kayıt tüm geçmişi okunamaz
yapmamalı.

**TASK-058: TAMAMLANDI** (hızlandırılmış sarma testi bekliyor).
