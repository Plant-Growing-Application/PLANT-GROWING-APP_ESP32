# TASK-068 — Profil Uygulama Servisi ve Dönem İlerlemesi

## Amaç

Katalogdaki bir profili **güvenli biçimde** config'e uygulamak, kullanıcı
kuralları elle değiştirdiğinde bunu dürüstçe işaretlemek ve gelişim dönemini
gün sayısına göre ilerletmek.

## Karar 1 — Önizleme ve uygulama AYNI yolu kullanır

`preview()` hiçbir şey yazmaz, `apply()` yazar; ama ikisi de `computePlan()`
çağırır. İki ayrı hesap yazılsaydı, önizlemede "uygulanabilir" görünen bir
seçim uygulama anında reddedilebilirdi ve onay ekranı **yanıltıcı** olurdu.

Önizlemenin var olma nedeni: ürün seçmek mevcut kural kümesinin **üzerine
yazar**. Onaysız yapılan böyle bir işlem projenin kendi ilkesine aykırıdır
(`ConfigService`: "sessiz varsayılana dönüş yok").

## Karar 2 — Yazma sırası: önce doğrula, sonra iki yazma

```
1. seçimi doğrula        → geçersizse HİÇBİR ŞEY yazılmaz
2. kuralları üret + doğrula + yaz
3. ürün seçimini yaz     (1'de doğrulandı, burada başarısız olamaz)
```

Ters sırada yazılsaydı, geçersiz bir kural kümesinde config'te "çilek seçili
ama çilek kuralları yok" gibi tutarsız bir durum kalırdı.

3. adımın dönüş değeri yine de kontrol edilir: sessizce yutulan bir hata,
kuralların yazıldığı ama seçimin yazılmadığı bir duruma yol açardı.

## Karar 3 — Elle düzenleme profili `ÖZEL`'e düşürür

`PUT /api/config/rules` başarılı olduğunda `markCustomized()` çağrılır.

Çağrılmasaydı arayüz "Çilek profili aktif" demeye devam eder ve **yalan
söylerdi**: ekranda çilek yazarken kurallar artık çilek profilinin ürettiği
kurallar olmazdı.

`derivedFrom` saklanır ki arayüz "Özel (Çilek temelli)" diyebilsin — "Özel"
tek başına kullanıcıya hiçbir şey anlatmaz.

`autoStage` de kapatılır: dönem ilerlemesi kuralları yeniden üretir ve
kullanıcının elle yaptığı düzenlemeyi sessizce silerdi.

**Yalnızca yazma başarılıysa** işaretlenir; reddedilen bir gövde config'i
değiştirmediği için profil de bozulmamıştır.

## Karar 4 — Hedef bantlar `ÖZEL`'de de korunur

İlk uygulamada `CUSTOM` iken `/api/crop` hedef bant döndürmüyordu ve arayüz tüm
"iyi/kötü" yorumlarını kaybediyordu. **Tek bir çevrim süresini değiştirmenin
bedeli, tüm ölçüm rehberliğini kaybetmek olamaz.**

Bantlar **bitkiyi** anlatır, kuralları değil: kullanıcı bir kuralı
değiştirdiğinde saksıdaki bitki hâlâ meyve dönemindeki bir çilektir ve hâlâ
pH 5.5–6.2 ister. `CUSTOM` iken bantlar `derivedFrom` profilinden okunur;
`targetsFromProfile` bayrağı yanıtta taşınır.

## Karar 5 — Dönem ilerlemesi `net` task'ında, saatte bir

**Nerede:** config'e yazan tek bağlam `net` task'ıdır (web API de oradan
koşar); `app_core` config'i yalnızca okur. Mevcut `PUT /api/config/rules`
deseninin aynısı.

**Ne sıklıkta:** saatte bir. Dönem gün mertebesinde değişir; her 100 ms'lik ağ
turunda tarih hesabı yapmak yalnızca CPU yakar. En kötü durumda geçiş bir saat
gecikir — 21 günlük bir dönemde ölçülemez bir fark.

## Karar 6 — Üç koruma

1. **Zaman geçersizken ilerleme DURUR.** Donanımsal RTC yok (ISSUE-005). Güç
   kesintisi + ağ yokluğunda duvar saati geçersizdir; gün saymak meyve
   dönemindeki çileği fideye döndürüp EC hedefini yarıya indirirdi. Arayüz
   `autoStageActive` bayrağıyla "duraklatıldı — saat geçersiz" der; yalnızca
   `autoStage` bayrağını göstermek yanıltıcı olurdu.
2. **Dönem GERİ ALINMAZ.** `stageForDay` monotoniktir ama SNTP saati geriye
   çekerse daha küçük bir dönem hesaplanabilir. İleri gitmemek gecikme yaratır,
   geri gitmek bitkiyi aç bırakır.
3. **Dikim tarihi gelecekteyse gün sayısı 0'dır.** Negatif gün üretmek yerine
   dönem başlangıçta kalır.

Başarısız bir ilerleme **sessizce geçilmez**, WARNING olarak loglanır.

## Dokunulan dosyalar

```
src/services/CropService.{h,cpp}      YENI
src/tasks/NetworkTask.cpp             crop::tick(now)
src/interfaces/web/api/ConfigApi.cpp  markCustomized() bagi
```

## Definition of Done

- [x] `pio run` temiz
- [x] Geçersiz seçimde config'e hiçbir şey yazılmıyor
- [x] Elle düzenleme `CUSTOM` işaretliyor ve arayüz bunu söylüyor
- [x] `CUSTOM` iken hedef bantlar korunuyor (tarayıcıda doğrulandı)
- [ ] Dönem ilerlemesi **gerçek zamanda** doğrulanmadı (günler sürer); host
      testi `stageForDay` matematiğini kapsıyor
