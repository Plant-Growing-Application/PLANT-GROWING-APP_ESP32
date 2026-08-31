# İnceleme Şablonu (STEP 3 — REVIEW)

> Kaynak: TASK-003 · Kullanım: her task'ın STEP 3 adımında
>
> `IMPLEMENTATION_PLAN.md` her task'a 9 maddelik bir Review Checklist verir.
> Bu şablon o maddeleri **tekrarlamaz**; her birinin **nasıl kontrol edileceğini**
> söyler. Amaç, checklist'in bir niyet beyanı değil bir prosedür olmasıdır.

---

## Kullanım

1. Aşağıdaki tabloyu sırayla uygula.
2. Her madde için **kanıt** yaz — "kontrol edildi" yeterli değildir; ne bakıldığı
   ve ne bulunduğu yazılır.
3. Bir madde N/A ise nedenini yaz.
4. Sonucu task dosyasının `STEP 3 — REVIEW RECORD` bölümüne ekle.

---

## 1. Architecture'a uygun mu?

| Kontrol | Nasıl |
|---|---|
| İlgili ARCHITECTURE bölümü okundu mu | Task'ın "Architecture References" bölümündeki her bölüm |
| Modül doğru katmanda mı | Dosya yolu ile `ARCHITECTURE.md` §17 karşılaştırılır |
| Bağımlılık yönü doğru mu | `CODING_STANDARDS.md` §1 tablosundaki D1–D6 aramaları |

## 2. Gereksiz abstraction var mı?

| Kontrol | Nasıl |
|---|---|
| Tek uygulaması olan arayüz | Sanal fonksiyon sayısı ve somut sınıf sayısı sayılır |
| Kullanılmayan genelleme | Yalnızca bir yerden çağrılan "genel" fonksiyon |
| ARCHITECTURE §0 yasak listesi | DI container, fabrika, generic event bus, dinamik kayıt |

> Bu madde **en sık atlanan** maddedir. ESP32'de soyutlamanın maliyeti faydasını
> geçebilir. Soyutlama yalnızca **birden fazla somut uygulama** varken haklıdır.

## 3. Blocking işlem var mı?

| Kontrol | Nasıl |
|---|---|
| Task döngüsünde bekleme | `grep -n "delay(\|while" <dosya>` — Y3 deseni |
| Sonsuz döngü | `grep -n "while *( *true\|while *( *1"` — Y4 deseni |
| Callback içinde uzun iş | AsyncTCP callback'inde dosya/flash/ağ erişimi |
| Kritik bölgede bloklama | Mutex tutarken log/IO çağrısı |
| Ölçüm | Döngü süresi ölçüldü mü, task bütçesine sığıyor mu |

## 4. Shared state güvenli mi?

| Kontrol | Nasıl |
|---|---|
| Global mutable değişken | `.cpp` üst seviyesinde `static` olmayan tanım — Y1 |
| Tek yazar kuralı | Bu alt-state'e başka kim yazıyor (Z1) |
| Donanım tek kapı | Bu kaynağa başka kim erişiyor (Z2) |
| ISR paylaşımı | ISR ile task arası veri `volatile` / kuyruk üzerinden mi |

## 5. Memory problemi var mı?

| Kontrol | Nasıl |
|---|---|
| Sıcak yolda ayırma | `new`, `malloc`, `String` toplama — Y10 |
| Yapı boyutu | `sizeof` ölçüldü ve kaydedildi mi |
| Stack | Watermark ölçüldü mü (task içeren işlerde) |
| Sızıntı | Tekrarlı işlemde heap eğilimi sabit mi |

## 6. Error handling var mı?

| Kontrol | Nasıl |
|---|---|
| Dönüş değeri kontrolü | Her `begin()` / init sonucu kullanılıyor mu — Y11 |
| Sessiz başarısızlık | Hata yolunda log var mı |
| Sessiz varsayılan | `memset` / varsayılan atama loglanıyor mu — Y13 |
| Kullanıcıya görünürlük | `ERROR`/`CRITICAL` API veya OLED'den görülebiliyor mu |
| Fail-safe | Güvenlik ilgiliyse en kötü durum varsayılıyor mu — Z7 |

## 7. ESP32 resource kullanımı uygun mu?

| Kontrol | Nasıl |
|---|---|
| ADC | Yalnızca ADC1 mi (Wi-Fi + ADC2 çakışması) |
| Pin sınıfı | `BoardPins.h` `static_assert`'leri geçiyor mu |
| Flash aşınması | Yazma sıklığı hesaplandı mı |
| Çekirdek | `xTaskCreatePinnedToCore` kullanıldı mı — Z9 |
| Periferi | PCNT / I2C birim bütçesi aşılmıyor mu |

## 8. Task sorumluluğu doğru mu?

| Kontrol | Nasıl |
|---|---|
| Yeni task gerekli miydi | `ARCHITECTURE.md` §6.4 okundu mu |
| Öncelik | Güvenlik döngüsü en yüksek öncelikte mi |
| Katman yasakları | UI sensör okuyor mu, servis çiziyor mu — Y7 |
| Watchdog | Kayıt var mı, besleme döngü sonunda mı — Z8 |

## 9. Eski kod gereksiz şekilde kopyalanmış mı?

> **En kritik madde.** `IMPLEMENTATION_PLAN.md` §1'in tamamı buna dayanır.

| Kontrol | Nasıl |
|---|---|
| Sınıf taşıma | Eski sınıf adı yeni kodda var mı |
| Fonksiyon taşıma | `legacy/` ile yeni dosya arasında yapı benzerliği |
| Algoritma taşıma | Eski formül "çalışıyor" diye korunmuş mu |
| Bug taşıma | Task dosyasında adı geçen eski hata tekrarlanmış mı |
| Global değişken | Eski global'ler yeni kodda karşılık bulmuş mu — Y1 |

Eski koda bakıldıysa **neden bakıldığı** yazılır: pin bilgisi, davranış referansı
veya problem tespiti. "Nasıl yazılacağı" için bakmak geçerli bir gerekçe değildir.

---

## 10. Ölü kod taraması (P7)

Her task sonunda çalıştırılır:

| Tarama | Beklenen |
|---|---|
| Bildirilip implement edilmemiş fonksiyon | 0 — Y8 |
| Hiç okunmayan struct alanı / sabit | 0 — Y12 |
| Hiç çağrılmayan sınıf / fonksiyon | 0 |
| Kullanılmayan include | 0 |

---

## Rapor İskeleti

```markdown
# STEP 3 — REVIEW RECORD

## Acceptance Criteria
- [x] ... (kanıt)
- [ ] ... (neden karşılanmadı)

## Test Plan
- [x] ... (sonuç/ölçüm)

## Review Checklist
- [x] Architecture'a uygun mu? — (ne kontrol edildi)
- [x] Gereksiz abstraction var mı? — (ne bulundu)
- [x] Blocking işlem var mı? — (tarama sonucu)
- [x] Shared state güvenli mi? — (tek yazar kanıtı)
- [x] Memory problemi var mı? — (ölçüm)
- [x] Error handling var mı? — (hata yolları)
- [x] ESP32 resource kullanımı uygun mu? — (kısıt kontrolü)
- [x] Task sorumluluğu doğru mu? — (katman kontrolü)
- [x] Eski kod gereksiz şekilde kopyalanmış mı? — (tarama + gerekçe)

## Bulgular
ISSUE-XXX kaydedildi: ...

## Durum
TASK-XXX: TAMAMLANDI / KOŞULLU TAMAMLANDI / BLOKE
```
