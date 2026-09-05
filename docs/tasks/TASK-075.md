# TASK-075 — OLED Gezinme: İki Seviye, Belirgin Seçim, Açılışta IP

## Amaç

Cihazın başındaki kullanıcının üç somut şikâyeti:

1. **Sayfa değiştirmek uzun sürüyor.** Encoder tek bir listede hem sayfaları
   hem satırları geziyordu; `SENSORS`'tan `CONTROL`'e geçmek, ilgilenilmeyen
   sekiz ölçümün tamamını çevirmek demekti.
2. **Seçili satır belli değil.** Seçim, satır başına konan tek bir `>`
   karakteriydi.
3. **IP hiçbir yerde görünmüyor.** Adres yalnızca `NETWORK` sayfasındaydı.

## Karar 1 — İki seviyeli gezinme

```
SAYFA MODU   çevir → sayfa değiştir (7 sayfa, DÖNGÜSEL)
             BAS   → sayfanın içine gir
ÖĞE MODU     çevir → sayfa içindeki satırlar (uçlarda DURUR)
             BAS   → onay akışı (iki basış — değişmedi)
             GERİ  → sayfa moduna dön
```

Eski davranışta en uzak sayfaya ulaşmak `HOME → CROP(7) → SENSORS(8) →
CONTROL(6) → …` yani **21 detentten fazlaydı**. Sayfa modunda döngüsellikle
en uzak sayfa **3 detenttir**.

İmleç öğe modunda da **döngüseldir**. İlk sürümde uçta duruyordu ve sahada
şöyle göründü: kullanıcı çeviriyor, hiçbir piksel değişmiyor, ekran donmuş
sanılıyor. Sayfa değiştirmek de çözüm değil — sayfadan çıkışın tek yeri GERİ
tuşudur.

Uçlarda durmanın eski gerekçesi (imleç listenin sonunda ekran değiştirsin)
ortadan kalktı: sayfadan çıkış artık TEK BİR YERDEN olur, GERİ tuşu. Kullanıcı
bir aktüatör ararken kendini başka bir ekranda bulamaz.

## Karar 2 — GERİ tuşu her basışta TEK adım geri alır

```
onay bekliyor → onayı iptal et
öğe modunda   → sayfa moduna dön (imleç başa)
sayfa modunda → HOME
```

Bir basışta birden çok seviye atlamak, kullanıcının nerede olduğunu takip
etmesini imkânsız kılar.

### Öğe moduna YALNIZCA kullanıcı girer

İlk sürümde iki "bir basış kazandıralım" kestirmesi vardı — acil durum ekranı
ve kurulumdan ürün listesine geçiş, doğrudan öğe moduyla açılıyordu. İkisi de
sahada aynı sonucu verdi:

> Cihaz açılıyor, kullanıcı farkında olmadan **iki seviye derinde** başlıyor.
> Encoder'ı çeviriyor; tek öğelik listede imleç kımıldamıyor, sayfa
> değişmiyor. Ekran donmuş görünüyor. Çıkmak için varlığını bilmediği **iki
> geri basışı** gerekiyor.

Kural artık istisnasız: **hiçbir olay gezinmeyi kendiliğinden sayfanın içine
almaz.** Acil durumu temizlemek böylece üç basış eder (gir → onay → uygula);
bir güvenlik eylemi için bu bir bedel değil.

### İki zaman aşımı

| Sabit | Süre | Ne yapar |
|---|---|---|
| `FOCUS_IDLE_MS` | 20 sn | Onayı iptal eder, **öğe modundan çıkar** — ekran değişmez |
| `IDLE_RETURN_MS` | 60 sn | `HOME`'a döner |

Kısa olan **her ekranda** işler, acil durum ekranında ve acil durum aktifken
de. Erken çıkışların arkasına konsaydı, acil durumdayken sayfanın içinde
unutulan kullanıcı orada sonsuza kadar kalırdı.

Onayın da düşmesi bir güvenlik kazancıdır: on dakika önce açılmış bir
"onaylamak icin bas", knob'a değen birinin pompayı çalıştırması demektir.
Zaman aşımı hiçbir zaman eylem ÜRETMEZ, yalnızca iptal eder.

**`EMERGENCY` ekranının kendisi istisnadır:** "acil durum aktifken GERİ her
yerden `EMERGENCY`'ye döner" kuralı orada da uygulansaydı ekran KİLİTLENİRDİ —
öğe modundan çıkan kullanıcının başka bir sayfaya bakması (teşhis için gerekli)
imkânsız olurdu. ACİL rozeti zaten her ekranda kalıcı.

Acil durum ekranı doğrudan öğe moduyla açılır: tek öğesi "onayla ve temizle"dir
ve acil durumdan çıkış yolunu bir basış uzatmanın karşılığı yok.

## Karar 3 — Seçim `>` değil, TERS RENKLİ SATIR

`>` 5×7 pikseldir. Cihaz sera duvarında asılıyken bir metre mesafeden
seçilmiyor, kullanıcı hangi satırda olduğunu ancak ekrana eğilerek görüyordu.
Ters renge dönen satır aynı bilgiyi 128×10 piksellik bir yüzeyle anlatır.

Bunun için sürücüye iki fonksiyon eklendi:

```
hal::oled::fillHighlight()    BEYAZ dolgu
hal::oled::drawTextInverse()  SİYAH metin
```

Var olan `drawRect(..., filled=true)` **siyah** doldurur (bir alanı temizler).
İsmi yanıltıcıydı ve üç yerde ters çizime yol açmıştı:

| Yer | Ne oluyordu |
|---|---|
| `drawEmergency` / `drawSetup` başlık bantları | Band hiç görünmüyor, başlık sıradan bir satır gibi duruyordu |
| Durum çubuğundaki ACİL rozeti | Rozet zemini yok; yanındaki mod yazısından farkı kalmıyordu |
| Wi-Fi sinyal çubukları | **Tam tersi**: dolu çubuklar görünmez, sinyal yokken dört boş çerçeve |

Üçü de düzeltildi.

Onay bekleyen satırda değer sütunu `ONAY?` olur: hangi satırın onay beklediği,
ekranın altındaki ipucu satırından bağımsız olarak satırın üzerinde yazar.

**Sayfa modunda hiçbir satır vurgulanmaz.** İmleç o modda kullanıcıya ait
değildir; vurgulanmış bir satır, çevirince oraya gidileceği sözünü verir ve bu
söz tutulmazdı.

## Karar 4 — Sayfa göstergesi ayırıcı çizginin şeridinde

```
▁▁ ▁▁ ██ ▁▁ ▁▁ ▁▁ ▁▁   ▼      SAYFA MODU: dilimler + konum, BAS = gir
████████████████████   ▲      ÖĞE MODU:   şeridin tamamı yanar, GERİ = çık
```

Mod farkı önce yalnızca 5×3 piksellik ok işaretiyle anlatılıyordu ve **kimse
fark etmedi**. Dolu şerit ile dilimlenmiş şerit arasındaki fark ekrana bakar
bakmaz görülür. Öğe modunda sayfa konumu gösterilmez — kullanıcı zaten sayfa
değiştirmiyor, çıkmayı bilmesi gerekiyor.

Boş ipucu satırı olan ekranlarda çıkış yolu ayrıca **yazıyla** durur:
"Geri: cikis".

"Kaçıncı sayfadayım, kaç sayfa var" sorusunun cevabı hiçbir yerde yazmıyordu.
Gösterge FAZLADAN SATIR HARCAMAZ — zaten çizilen ayırıcı çizginin yerini alır;
128×64'te bir satır, gösterilebilecek ölçümlerin beşte biridir.

Mod işareti iki seviyeli gezinmenin görsel karşılığıdır. İçeriği olmayan
sayfada (`HOME`, `NETWORK`, `ALERTS`) ▼ **çizilmez**: çalışmayan bir düğmeye
davet etmek cihazı takılmış gösterir (§12.2). Boş ipucu satırı olan ekranlarda
(`CONTROL`, `SYSTEM`) gesture bir kez açıkça yazılır: "Girmek icin bas".

## Karar 5 — IP açılış ekranında

`HOME`'un beşinci satırı boştaydı. Arayüzü açmak isteyen herkes önce cihazın
başına gidip sayfalar arasında dolaşmak zorundaydı; adres artık cihazın İLK
gösterdiği ekranda durur.

Bağlantı yokken model `"bagli degil"` taşır ve **bu da bilgidir**: boş bir
satır, kullanıcıya adresin ne olduğunu değil ekranın bozuk olduğunu
düşündürürdü.

## Karar 6 — Navigasyon durumu tek yapıda taşınır

`build()` çağrısı yedi ayrı parametreye çıkacaktı; art arda dizilmiş `bool`
argümanlarında sıra karışırsa derleyici bunu YAKALAMAZ ve ekran sessizce yanlış
modda çizilirdi. `NavState` bir POD olarak tanımlandı; `UiModel` `memcmp` ile
karşılaştırılmaya devam ediyor (yalnızca değişince çizim).

## Dokunulan dosyalar

```
src/hal/OledPanel.{h,cpp}            fillHighlight(), drawTextInverse()
src/interfaces/ui/Navigation.{h,cpp} iki seviyeli gezinme, sayfa göstergesi API'si
src/interfaces/ui/ViewModels.h       NavState; UiModel'e navFocus/pageIndex/
                                     pageCount/pageEnterable
src/interfaces/ui/ViewModelBuilder.{h,cpp}  build() NavState alıyor
src/interfaces/ui/ScreenFramework.h  COL_LABEL 2→4, PAD_RIGHT, BAR_H/BAR_DY
src/interfaces/ui/screens/Screens.cpp  seçim çubuğu, sayfa şeridi, HOME'da IP,
                                     ters renkli başlık bantları ve ACİL rozeti
src/interfaces/ui/UiService.cpp      NavState'i dolduruyor
```

## Definition of Done

- [x] Encoder sayfa modunda sayfaları, öğe modunda satırları geziyor
- [x] Açılışta (acil durum dâhil) encoder **doğrudan** sayfa değiştiriyor
- [x] İmleç liste içinde dönüyor; ölü tur yok
- [x] 20 sn hareketsizlikte onay ve öğe modu kendiliğinden düşüyor
- [x] GERİ tek adım geri alıyor; `EMERGENCY` ekranı kilitlenmiyor
- [x] Seçili satır ters renkli; onay bekleyen satır `ONAY?` gösteriyor
- [x] `HOME` IP adresini gösteriyor
- [x] İçeriği olmayan sayfada "gir" ipucu ve ▼ çizilmiyor
- [x] Wi-Fi çubukları, ACİL rozeti ve başlık bantları doğru renkte
- [x] `SENSORS` imleci kayıtlı sensör sayısıyla sınırlı (ölü tık yok)
- [ ] **`pio run` KOŞULMADI** — bu makinede derleyici yok (ISSUE-033);
      değişiklikler yalnızca gözle denetlendi
- [ ] Donanımda doğrulama: `docs/HARDWARE_TEST_PROCEDURE.md` gezinme adımları
