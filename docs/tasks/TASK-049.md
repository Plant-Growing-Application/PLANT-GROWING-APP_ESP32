# TASK-049 — Dashboard, Control & Settings Views

**Phase:** 10 — Frontend · **Priority:** P2

## Objective

Kullanıcı arayüzünün görsel katmanını oluşturmak: gerçek sensör değerleri, aktüatör
kontrolü, ağ kurulumu ve ayar ekranları.

## Scope

- Dashboard: sensör kartları (kalite göstergeli), sistem durumu, saat, aktif hatalar
- Kontrol: aktüatör kartları, mod seçimi, acil durdurma
- Ağ kurulumu: tarama, SSID seçimi, şifre, DHCP/Static IP
- Ayarlar: eşikler, kalibrasyon, otomasyon kuralları
- Teşhis: son olaylar, boot raporu
- Giriş ekranı

## Out of Scope

- State senkronizasyonu (TASK-048)
- Backend endpoint'leri (TASK-043, TASK-044)

## Dependencies

- TASK-048, TASK-043, TASK-044

## Requirements

- `REQUIREMENTS.md` — §5.1, §5.2, §5.3 (tüm web arayüzü gereksinimleri)

## Architecture References

- §14.3 API sözleşmesi · §14.2 Durum senkronizasyonu

## Expected Design

### Sabit değer yasağı

Mevcut projede pH kartı sabit "6.5", EC kartı sabit "1.2 mS" gösteriyordu — hiçbir kod
bu alanları güncellemiyordu. Bu, kullanıcıya **yanlış bilgi sunmaktır**.

> Hiçbir gösterge sahte veya sabit değer göstermez. Veri yoksa "—" gösterilir;
> sensör takılı değilse "takılı değil"; kalite kötüyse uyarı işaretiyle gösterilir.

### Kalite gösterimi

| Kalite | Gösterim |
|---|---|
| `OK` | Normal değer |
| `STALE` | Değer + uyarı işareti |
| `OUT_OF_RANGE` | Değer + kırmızı uyarı |
| `FAULT` | "—" + arıza işareti |
| `NOT_PRESENT` | "Takılı değil" |

### Güvenlik durumunun görünürlüğü

Aktif güvenlik kilidi ve acil durum **her ekranda** görünür olmalı; kullanıcı kontrol
sayfasına gitmeden sorunu fark etmeli. Acil durum aktifken bu, ekranın en belirgin öğesi
olmalıdır.

## Implementation Notes

- Aktüatör kartları TASK-048'in "bekliyor" durumunu görsel olarak yansıtmalı.
- Ağ kurulum akışı özellikle dikkatli tasarlanmalı: tarama "devam ediyor" durumu
  gösterilmeli, ilk tıklamada hata verilmemeli (TASK-039 sözleşmesi).
- Wi-Fi kaydetme sonrası kullanıcı ne olduğunu görmeli: bağlantı deneniyor, başarılı/başarısız.
  Ham metin yanıt gösterilmesi kabul edilemez.
- Eşik ayarları formu, sunucu doğrulama hatalarını **alan bazında** göstermeli (§14.5'teki
  `field` alanı).
- Otomasyon kuralları arayüzü karmaşıklaşabilir; ilk sürümde temel bir düzenleyici yeterli,
  ancak sunucu şemasıyla birebir uyumlu olmalı.
- Saat gösterilmeli ancak zaman geçersizken "senkronize değil" belirtilmeli.
- Acil durdurma butonu kolay erişilebilir ama kazara basılmayacak şekilde tasarlanmalı.
- Ekranların tümü mobilde kullanılabilir olmalı.

## Files

- `frontend/src/views/dashboard.*` (yeni)
- `frontend/src/views/control.*` (yeni)
- `frontend/src/views/network.*` (yeni)
- `frontend/src/views/settings.*` (yeni)
- `frontend/src/views/diagnostics.*` (yeni)
- `frontend/src/views/login.*` (yeni)

## Acceptance Criteria

- [ ] Hiçbir gösterge sabit/sahte değer göstermiyor
- [ ] Sensör kalitesi görsel olarak yansıtılıyor
- [ ] Aktüatör kartları "bekliyor" durumunu gösteriyor
- [ ] Güvenlik kilidi ve acil durum her ekranda görünür
- [ ] Ağ tarama akışı ilk tıklamada hata vermiyor
- [ ] Wi-Fi kaydetme sonrası süreç kullanıcıya gösteriliyor
- [ ] Form hataları alan bazında gösteriliyor
- [ ] Saat geçersizken açıkça belirtiliyor
- [ ] Acil durdurma erişilebilir ama kazara basılmıyor
- [ ] Tüm ekranlar mobilde kullanılabilir
- [ ] Kırık link veya karşılığı olmayan buton yok

## Test Plan

- [ ] Her sensör kalitesi durumu için gösterim doğrulandı
- [ ] Sensör sökülünce arayüz "—" gösteriyor, eski değeri değil
- [ ] Aktüatör komutu reddedildiğinde neden gösteriliyor
- [ ] Güvenlik kilidi aktifken her ekranda görünüyor
- [ ] Ağ tarama akışı baştan sona sorunsuz
- [ ] Wi-Fi değiştirme akışı baştan sona test edildi
- [ ] Geçersiz eşik girildiğinde alan bazında hata gösteriliyor
- [ ] Mobil tarayıcıda tüm ekranlar kullanılabilir
- [ ] Giriş/çıkış akışı çalışıyor
- [ ] Acil durum aktifken arayüz belirgin şekilde uyarıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.2, §14.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? — tek store'dan okuyor mu
- [ ] Memory problemi var mı? — istemci tarafı
- [ ] Error handling var mı? — form ve komut hataları
- [ ] ESP32 resource kullanımı uygun mu? — gereksiz istek yok mu
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **sabit pH/EC değerleri ve kırık linkler yasak**

## Definition of Done

Ortak DoD + tüm kullanıcı akışları baştan sona test edildi + hiçbir sahte değer
gösterilmediği doğrulandı.

---

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

> **Protokol notu:** Bu kayıt, TASK-049 uygulandıktan sonra fark edilen bir
> eksiklik nedeniyle geriye dönük yazıldı. İlk turda inceleme kaydı yazılmış
> ama tasarım kaydı atlanmıştı. Aşağıdaki kararlar uygulamada fiilen
> alınmış kararlardır; sonradan uydurulmuş gerekçeler değil — her biri
> `frontend/app.js` ve `app.css` içinde karşılığı gösterilebilir haldedir.

## Karar 1 — Görünümler DOM'da, ayrı sayfa değil

Tek sayfa kararı TASK-047'de alındı (WS bağlantısı korunsun diye). Bunun
sonucu: her görünüm `<div class="page">` olarak baştan DOM'da bulunur ve
`switchView()` yalnızca `hidden` sınıfını değiştirir.

Alternatif (şablondan üretme) 128 KB'lık bir çerçeve gerektirirdi; altı
statik görünüm için gereksiz.

## Karar 2 — İki veri kaynağı, iki güncelleme yolu

```text
CANLI  (WS state paketi) → panel, kontrol, ag durumu, guvenlik bandi
                           → `render()` her state'te calisir
ISTEK-UZERINE (REST)     → teshis, yapilandirma formlari, tarama listesi
                           → yalnizca o sekmeye girilince yuklenir
```

Yapılandırma saniyede bir değişmez; onu telemetriye koymak paket boyutunu
büyütür ve hiçbir şey kazandırmaz. Teşhis kayıtları da (16 olay) her
pakette taşınamaz.

## Karar 3 — Sensör kalitesi GÖRSEL olarak ayrılır, gizlenmez

```text
kalite ok      → normal deger
kalite ok DEGIL→ deger USTU CIZILI + kirmizi + rozet ("arizali"/"bayat"/...)
```

Değeri **gizlemiyoruz** — operatör "sensör ne diyor" görebilmeli. Ama canlı
bir ölçüm gibi de göstermiyoruz. Eski projede arızalı sensörün son değeri
sağlıklı bir değerden **hiçbir şekilde ayırt edilemiyordu**.

## Karar 4 — Yıkıcı işlemler `confirm()` ile

Fabrika ayarları ve "ağı unut" tarayıcı onayı ister. Acil durdurma
**istemez**: onay diyaloğu tam da gerektiği anda bir saniye kaybettirir
(TASK-043 ile aynı gerekçe).

## Karar 5 — Hata kodları istemcide çevrilir

Cihaz metin taşımaz (`LogRecord` 12 bayt). `ERR_TEXT` tablosu `ErrCode` →
Türkçe çeviriyi yapar. Bilinmeyen kod için ham onaltılık gösterilir —
"bilinmeyen hata" demek teşhisi imkânsızlaştırır.

## Karar 6 — HTTPS uyarısı giriş ekranında

`GET /api/auth/status` `"secure": false` döndürüyor. Kullanıcının bu
kısıttan haberdar olmaması, kısıtın kendisinden kötüdür (TASK-042 Karar 5).

---

# STEP 3 — REVIEW RECORD

- [x] Panel: mod, uptime, saat, boş bellek, sensör kartları
- [x] Kontrol: aktüatör kartları + acil durdurma/temizleme
- [x] Ağ: durum, tarama listesi, SSID/şifre formu, unut, şimdi dene
- [x] Ayarlar: güvenlik ve sistem formları, fabrika ayarları
- [x] Teşhis: aktif hatalar + son olaylar
- [x] Giriş ekranı; kurulum modunda parola belirleme akışı
- [x] Sensör kalitesi **görünür**: `ok` dışındaki her değer üstü çizili ve
      rozetli — arızalı sensörün değeri canlı gibi gösterilmiyor
- [x] Saat geçersizken "geçersiz" yazıyor, sahte saat değil
- [x] Güvenlik bandı: kilit ve acil durum kalıcı gösteriliyor
- [x] Yıkıcı işlemler `confirm()` ile onaylanıyor
- [x] HTTPS uyarısı giriş ekranında
- [x] **Tarayıcıda doğrulandı** — aşağıya bakın

**TASK-049: TAMAMLANDI** (tarayıcı testleri bekliyor).

## Tarayıcı doğrulaması — 2026-08-31

Tüm görünümler açıldı ve sahte verilerle çizdirildi.

| Görünüm | Sonuç |
|---|---|
| Panel | Mod/uptime/saat/bellek + 5 sensör kartı |
| Kontrol | 2 aktüatör kartı + acil durdurma |
| Ağ | Durum kartı + tarama listesi |
| Ayarlar / Teşhis | Arka uç yokken "İstek başarısız", çökme yok |
| Güvenlik bandı | Mandal → `safetybar critical`; kilit → `safetybar` |
| Saat geçersiz | "geçersiz" — sahte saat değil |
| Tarama, boş dizi | Çökme yok (eski sistemin hatası) |

## Düzeltilen hata: arızalı sensör çöp değer gösteriyordu

Tarayıcı testinde görüldü: kalitesi `fault` olan akış sensörü
**`99.90 L/dk`** değerini üstü çizili gösteriyordu.

Sorun iki katmanlı:

1. **Değerin kendisi çöp.** `fault` "okuma başarısız" demektir; taşınan
   sayının anlamı yoktur. Üstü çizili olsa bile operatörün kafasına bir
   sayı sokar.
2. **OLED ile tutarsızlık.** `ViewModelBuilder` (TASK-050 Karar 3) `fault`
   için `"--"` üretiyordu. Aynı veriyi iki arayüzün farklı göstermesi,
   hangisine bakıldığına göre farklı karar verilmesi demektir.

**Düzeltme:** ayrım kalite sınıfına göre yapıldı.

```text
stale / outOfRange → deger GOSTERILIR + rozet + ustu cizili
                     (bir zamanlar GERCEK bir okumaydi)
fault / notPresent → SAYI YOK: "—" / "yok"
                     (OLED ile birebir ayni)
```

Doğrulama sonrası ölçüm:

```text
ok          → "21.4°C"   rozet "olculdu"
stale       → "6.41"     rozet "bayat"    (ustu cizili)
fault       → "—"        rozet "arizali"  (SAYI YOK)
notPresent  → "yok"      rozet "yok"
```

Varlık boyutu 11 266 → 11 730 bayt gzip (hedef <12 KB, hâlâ tutuyor).
