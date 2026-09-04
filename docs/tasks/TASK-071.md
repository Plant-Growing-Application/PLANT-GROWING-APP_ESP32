# TASK-071 — Encoder: Yön Değişiminde Kaybolan Tık

## Bildirilen belirti

> "İleri doğru sürekli gidersem sorun yok. İleri gittim, bir geri geldim —
> o 1 tık çalışmıyor, 2. tıkta geri geliyor."

## Kök neden

`hal/InputDevices.cpp` içindeki biriktirici (`g_encSteps`) **yön değiştiğinde
sıfırlanmıyordu.**

Bir detent 4 quadrature geçişi üretir ve eşik 4'tür; teoride her detent sonunda
biriktirici sıfırda kalır. Pratikte kalmaz — **geçiş kaybolur**:

1. **Sıçrama filtresi.** 600 µs'lik zaman kapısı ara sıra geçerli bir geçişi
   eler. Daha kötüsü: reddedilen kenarda fonksiyon `return` ediyor ve
   `g_encState` **güncellenmiyordu**. Çözücünün "önceki durum"u gerçek
   pinlerden ayrışıyor, sonraki gerçek geçiş bayat duruma göre hesaplanıp
   tabloda **geçersiz** görünüyor ve sessizce kayboluyordu.
2. **GPIO ISR'si IRAM'de değil.** `attachInterrupt()` kesmeyi
   `ESP_INTR_FLAG_IRAM` olmadan kurar; `store` task'ı flash'a yazarken
   (geçmiş kaydı **her 60 saniyede**, config kaydı her ayar değişiminde)
   kesme geçici olarak çalışmaz. O sırada çevrilen encoder'ın geçişleri düşer.
   `IRAM_ATTR` işleyicinin kendisinde olsa da kayıt bayrağı olmadan etkisizdir.

Sonuç: biriktiricide **kalıcı bir artık** oluşur. Aynı yönde devam ederken bu
zararsızdır — emisyon yalnızca bir geçiş kayar, her detent yine bir tık üretir
ve kullanıcı farkı hissetmez:

```
ileri:  +3 →(+1) 4 ✅ TIK → 0 →(+1,+2,+3) 3 →(+1) 4 ✅ TIK → 0 …
```

Ama yön değişince artık **ters çalışır**: −4'e ulaşmak için önce +3'ü harcamak
gerekir, yani iki detent:

```
geri:   +3 →(−1) 2 →(−1) 1 →(−1) 0 →(−1) −1   ← 1. detent bitti, TIK YOK
        −2, −3, −4 ✅ TIK                       ← ancak 2. detentte
```

Tam olarak bildirilen davranış.

## Doğrulama (düzeltmeden önce/sonra)

Mantık Python'da birebir simüle edildi; senaryo: bir geçiş kaybolur, üç detent
ileri, iki detent geri.

```
--- DUZELTME YOK (eski davranis) ---
  ileri detent 1 (1 gecis KAYIP)     tik=0  kalan_artik=-3
  ileri detent 2                     tik=1  kalan_artik=-2
  ileri detent 3                     tik=1  kalan_artik=-2
  GERI detent 1                      tik=0  kalan_artik=+2      ← HATA
  GERI detent 2                      tik=1  kalan_artik=+2

--- DUZELTME VAR ---
  GERI detent 1                      tik=1  kalan_artik=+0      ← DÜZELDİ
  GERI detent 2                      tik=1  kalan_artik=+0
```

## Karar 1 — Çözücü `core/`'a taşındı ve SAFLAŞTIRILDI

Mantık ISR'nin gövdesindeydi ve **test edilemiyordu**: hatanın oluşması için
bir geçişin kaybolması gerekiyor ve bunu elle üretmek mümkün değil.

`core/EncoderDecode.h` donanım görmez — girdi iki pinin okunmuş hâli, çıktı bir
detent olayı. `pio test -e native` yön değiştirme senaryosunu sentetik olarak
koşturabiliyor.

`hal/InputDevices.cpp`'de yalnızca donanım kaldı: pin okuma, zaman kapısı,
kuyruğa olay basma.

## Karar 2 — Yön değişiminde artık ATILIR

```cpp
if ((delta > 0 && d.steps < 0) || (delta < 0 && d.steps > 0)) { d.steps = 0; }
```

Kısmi bir dönüş, kullanıcının "bir tık" saymadığı harekettir; onu ters yöndeki
ilk tıkın hesabına yazmak yanlıştır.

**Reddedilen alternatif:** her detent sonunda biriktiriciyi sıfırlamak. Bu,
kaybolan bir geçişten sonra ileri yönde de tık kaybettirirdi — şu an sorunsuz
çalışan yönü bozardı.

## Karar 3 — Reddedilen kenarda durum SENKRON kalır

Zaman kapısı bir kenarı elediğinde artık `encoderSyncPhase()` çağrılıyor:
sayılmıyor ama çözücünün bilinen durumu gerçek pinlere eşitleniyor. Artığın
ikinci kaynağı böylece kapandı.

Sıçrama reddi bozulmaz: 00→01→00→01 gibi çift sayıda sıçrama kenarı aynı
duruma geri döner ve sayılmaz.

## Karar 4 — Zaman damgası yalnızca GEÇERLİ geçişte tazelenir

Geçersiz bir kenar kapıyı ileri sarsaydı, hemen ardından gelen gerçek geçiş
elenirdi. Eski kodun davranışı korundu, sadece açıkça yazıldı.

## Karar 5 — Taşma koruması

`int8_t` sınırında sarma, işaret değişimi üretir ve encoder aniden ters yöne
dönmüş gibi görünürdü. Biriktirici `int16_t` üzerinden kırpılıyor.

## Testler (host)

| Test | Neden donanımda yapılamaz |
|---|---|
| `clean_rotation_counts_one_tick_per_detent` | temel davranış, hızlı regresyon |
| `reversal_after_lost_transition_still_ticks_on_first_detent` | **bildirilen hata** — kayıp geçiş elle üretilemez |
| `direction_change_discards_partial_movement` | yarım detent elle ayarlanamaz |
| `invalid_transitions_are_ignored` | çift bit geçişi elle üretilemez |
| `sync_phase_does_not_count` | zaman kapısı davranışı gözlenemez |
| `step_accumulator_cannot_overflow` | 300 detent döndürmek gerekir |

## Dokunulan dosyalar

```
src/core/EncoderDecode.h        YENI — saf cozucu + tablo static_assert'leri
src/hal/InputDevices.cpp        ISR yalnizca donanim; cozucuyu cagirir
test/test_domain/test_domain.cpp  6 yeni test
```

## Definition of Done

- [x] `pio run` temiz (0 uyarı)
- [x] Hata simülasyonla önce/sonra kanıtlandı
- [x] Quadrature tablosunun simetrisi `static_assert` ile kilitli
- [ ] **Gerçek encoder'da doğrulanmadı** — donanım elde yok. Simülasyon
      bildirilen belirtiyi birebir üretiyor ve düzeltme onu kapatıyor.

## Açık kalan (ayrı iş)

GPIO kesmesi hâlâ IRAM'de kayıtlı değil; flash yazarken geçiş kaybı **devam
eder**. Artık atma bunu ZARARSIZ kılar (yön değişimi artık etkilenmez) ama
uzun bir flash yazması sırasında hızlı çevrilen encoder yine tık kaybedebilir.
Kalıcı çözüm `gpio_install_isr_service(ESP_INTR_FLAG_IRAM)` ve tüm ISR
verisinin DRAM'e taşınmasıdır — ayrı bir task olarak değerlendirilmeli.
