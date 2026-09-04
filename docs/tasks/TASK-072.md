# TASK-072 — Denetim Bulgularının Kapatılması (O2, O3, K1–K7)

Kaynak: sistem geneli kod denetimi. Bu task denetimde çıkan **orta ve düşük**
öncelikli bulguları kapatır. Yüksek öncelikli iki bulgu (Y1 geçmiş sıra
uyuşmazlığı, Y2 sensörlerin açılamaması) **kapsam dışıdır** — ikisi de veri
göçü / yeni API gerektiriyor ve ayrı ele alınacak.

---

## O2 — Config iki task arasında kilitsiz paylaşılıyordu

`AppCore::begin(config::get())` canlı `g_config`'e **işaretçi** tutuyor;
`app_core` her 100 ms okurken `net` task'ı ona yazıyor.

### Karar — yalnızca YAPILAR korunur, skalerler korunmaz

Yarış yüzeyi ilk bakışta göründüğünden küçük çıktı:

| Okuma | Boyut | Yırtılabilir mi |
|---|---|---|
| `safety.flowMinRate`, `maxRuntimeViolations`, `automation.mode`… | 1/2/4 bayt | **Hayır** — hizalı skaler okuma Xtensa'da atomik |
| `rules` (RuleSet) | 196 bayt | **Evet** |
| `actuators[i]` | 16 bayt | **Evet** |

Skalerleri de kilitlemek, kritik bölümü hiçbir kazanç olmadan uzatırdı.

### Uygulama

`ConfigService`'e bir spinlock ve iki kopyalama fonksiyonu eklendi:

```
copyRules(RuleSet&)                          — AutomationEngine tur basi cagirir
copyActuators(ActuatorConfig(&)[MAX_ACTUATORS]) — ActuatorManager tur basi cagirir
```

Yazarlar (`updateRules`, `updateActuator`) atamayı kilit altında yapar. Kilit
tek bir `memcpy` süresince tutulur (196 bayt ≈ 0,5 µs @240 MHz) — bloklamaz,
öncelik terslenmesi üretmez (ARCHITECTURE P3).

**Kilit neden `services/` içinde:** `core/` ve `domain/` FreeRTOS başlığı
include edemez (D5). `domain/` yalnızca kopyalama fonksiyonunu çağırır —
`rulesRevision()` için zaten kurulmuş aşağı yönlü bağımlılığın aynısı.

**`persist()` de kopya üzerinden yazar.** `store` task'ı canlı yapıyı doğrudan
NVS'e verseydi, `net` tam o anda yazarken yarı güncellenmiş bir bölüm
**flash'a kalıcı** olabilirdi. Kilit yalnızca kopyalama süresince tutulur; NVS
yazması (milisaniyeler) kritik bölümün dışında kalır.

**Reddedilen alternatifler:**
- *Tüm Config'i çift tamponlamak:* dört domain modülü `const Config*`
  önbelleğe alıyor; tampon takası bu işaretçileri geçersiz kılardı ve
  güvenlik-kritik dört modülün refaktörü gerekirdi. Kazanç/risk oranı kötü.
- *Okuyucu tarafında seqlock:* aynı refaktörü gerektirir.

### Bedeli

`ActuatorManager` bir turluk gecikmeyle çalışır: `request()` bu turun değil bir
önceki turun kopyasını kullanır. Config değişikliği en geç bir sonraki 100 ms'lik
turda etkili olur — başlıkta zaten vaat edilen davranış.

RAM: +276 bayt `.bss` (196 kural kopyası + 80 aktüatör kopyası).

---

## O3 — ÖZEL profilde sulama yoğunluğu butonları her zaman hata veriyordu

`cropSelected()` `'custom'` için `true` döndüğü için butonlar etkindi. Ama
`PUT /api/crop` yalnızca `{intensity}` gönderiyor, sunucu `crop`'u mevcut
değerden devralıyor ve `cropById(CUSTOM)` `nullptr` dönüyordu → her tıklamada
**"Ayar doğrulaması başarısız: Ürün"**.

### Karar — düğmeyi kilitle, nedenini söyle

Yoğunluk değişimi kural kümesini profilden **yeniden üretir**. Kullanıcı
kuralları elle düzenlemişse bu, düzenlemeleri uyarısız silmek olurdu. Doğru
yol, önizleme gösteren "Ürünü / Dönemi Değiştir" akışıdır.

Butonlar `ÖZEL`'de kilitli ve ipucu bunu açıklıyor.

---

## K1 — Config boyut nöbetçisinde 16 bayt kalmıştı

ELF sembol tablosundan ölçüldü: `sizeof(Config)` = **624 bayt**, sınır 640.

Sınır keyfîdir (bölümler ayrı NVS anahtarlarında saklanır; en büyüğü `rules`,
196 bayt — NVS blob sınırının onlarca katı altında). Nöbetçinin işi, yapının
**fark edilmeden** büyümesini engellemek.

16 bayt payla tek bir `uint32_t` ekleyen kişi, gerçek bir sorun olmadığı hâlde
derleme hatası alır ve sınırı düşünmeden yükseltirdi. Sınır **704**'e çıkarıldı
ve ölçülen değer assert mesajına yazıldı.

## K2 — Katalog alınamazsa sihirbaz çöküyordu

`openWizard()` hata durumunda gövdeye mesaj yazıp çıkıyordu ama "Devam"
düğmesi **etkin kalıyordu**. Tıklanınca `renderWizard()` adım 0'da doğrudan
`store.catalog.crops` üzerinde dönüyor ve **TypeError** atıp kipi donduruyordu.

Koruma `renderWizard()`'ın **en başına** kondu (ilk denemede adım 1'e
konmuştu — çökme adım 0'da olduğu için işe yaramadı, tarayıcı testinde
yakalandı). Düğmeler kilitleniyor, açık bir hata metni gösteriliyor.

## K3 — Sistem ayarlarında iki isteğin mesajı çakışıyordu

Form iki bölüme yazıyor (sistem + otomasyon) ve her biri aynı mesaj kutusunu
kullanıyordu: birincisi başarısız, ikincisi başarılı olduğunda kullanıcı
**"✔ yazıldı"** görüyordu.

Artık ikisi toplanıp tek sonuç raporlanıyor; kısmi başarı açıkça
**"Kısmen kaydedildi — <alan>: <hata>"** olarak yazılıyor.

## K4 — `hal::aht20::begin()` iki kez çağrılıyordu

Sıcaklık ve nem sarmalayıcılarının ikisi de çağırıyor. İkinci çağrı durum
makinesini sıfırlıyordu; çip o anda meşgulse `readStatus()` başarısız olur,
birinci çağrının başarısı silinir ve **iki sensör birden arızalı** görünürdü.

`begin()` artık idempotent: çip zaten hazırsa `OK` dönüp çıkıyor.

## K5 — BH1750 ilk okumada 0 lüks döndürüyordu

Sürekli modda ilk ölçüm ~120 ms sonra hazır olur. O ana kadar yazmaç **0**
döner ve bu, gerçek bir "karanlık" ölçümünden ayırt edilemez.

`FIRST_MEASUREMENT_MS` (200 ms) dolmadan `read()` **false** dönüyor; çağıran
bunu FAULT'a çeviriyor ve arayüz kısa süre "okunamıyor" gösteriyor. Uydurma
bir sıfırdan iyidir.

## K6 — `plantedAt` sessizce yok sayılıyordu

`is<int64_t>()` yanlışsa değer mevcut hâlinde kalıyordu ve kullanıcı yazdığı
tarihin kaydedildiğini sanıyordu. Artık alan varsa ya geçerli bir tam sayıdır
ya da istek **alan adıyla** reddedilir (ARCHITECTURE §16.4).

## K7 — Katalog tamponu firmware'in en büyük `.bss` nesnesiydi

6 KB'lık `g_json`, doğrudan boş heap'ten düşen kalıcı bir maliyetti.

Katalog artık `AsyncResponseStream` ile akıtılıyor — `HistoryApi`'nin 22 KB'lık
yanıtı için verilen kararın aynısı. Paylaşılan tampon 2 KB'a indi.

**Ölçülen kazanç: RAM %25,4 → %24,1** (≈ 4,3 KB).

---

## Doğrulama

Tümü sahte cihaz sunucusuyla tarayıcıda koşturuldu:

- [x] O3 — normal profilde butonlar etkin, `ÖZEL`'de kilitli + ipucu doğru
- [x] K2 — katalog hatasında çökme yok, düğmeler kilitli
- [x] K3 — kısmi hata "Kısmen kaydedildi", tam başarı "✔"
- [x] Regresyon: donanım işaretleme → domates/çiçeklenme profili → 5 kural
      (Bol yoğunluk 240 sn'ye kırpıldı) → OTOMATİK mod → panel; konsolda hata yok
- [x] `pio run` temiz, 0 uyarı

## Dokunulan dosyalar

```
src/services/ConfigService.{h,cpp}   spinlock + copyRules/copyActuators, persist kopyasi
src/domain/AutomationEngine.cpp      kural kumesinin tutarli kopyasi
src/domain/ActuatorManager.cpp       kisit tablosunun tutarli kopyasi
src/core/Config.h                    boyut nobetcisi 640 -> 704, olculen deger yazildi
src/hal/Aht20.cpp                    begin() idempotent
src/hal/Bh1750.{h,cpp}               ilk olcum hazir olana kadar okuma reddedilir
src/interfaces/web/api/CropApi.cpp   katalog akisla gonderilir, plantedAt reddedilir
frontend/js/20-crop.js               OZEL'de yogunluk kilidi, sihirbaz katalog korumasi
frontend/js/40-expert.js             sistem kaydinda tek sonuc raporu
```

## Kapsam dışı bırakılanlar

| Bulgu | Neden ertelendi |
|---|---|
| **Y1** geçmiş slot sırası uyuşmazlığı | `SENSOR_SLOTS` 6→8 kayıt boyutunu 24→28 bayt yapar ve **mevcut geçmiş dosyasını geçersiz kılar**; göç kararı gerekiyor |
| **Y2** sensörlerin açılamaması | Yeni bir API yüzeyi (`/api/config/sensors`) + arayüz + çalışma anında sürücü başlatma yolu gerekiyor |
| **O1** OLED yeni donanımı görmüyor | `UI_SENSORS`/`UI_ACTS` genişletmesi + ekran yerleşimi; ayrı iş |
| GPIO ISR'sinin IRAM'e alınması | TASK-071'de belgelendi; ayrı değerlendirilmeli |
