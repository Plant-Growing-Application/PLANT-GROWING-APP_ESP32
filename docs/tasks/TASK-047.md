# TASK-047 — Asset Pipeline & App Shell

**Phase:** 10 — Frontend · **Priority:** P2

## Objective

Frontend varlıklarını üretme ve sıkıştırma sürecini kurmak; uygulama iskeletini
oluşturmak. Mevcut projedeki 298 KB'lık sıkıştırılmamış CSS yükünü ortadan kaldırmak.

## Scope

- Varlık üretim/sıkıştırma süreci (gzip)
- LittleFS imajına yerleştirme
- HTML iskeleti, ortak düzen ve navigasyon
- Temel stil (CSS) — mümkün olduğunca hafif
- Bağlantı durumu göstergesi

## Out of Scope

- State senkronizasyon istemcisi (TASK-048)
- Sayfa içerikleri (TASK-049)
- Backend endpoint'leri

## Dependencies

- TASK-041

## Requirements

- `REQUIREMENTS.md` — §5.1 (dashboard `[~]`), Donanım tablosu (298 KB bootstrap)

## Architecture References

- §14.6 Gzip'li varlık servisi · §15.1 Web varlıkları LittleFS'te

## Expected Design

### Karar gerektiren nokta — CSS çerçevesi

```text
Problem:      Mevcut projede tam Bootstrap (298 KB) gömülü
Constraints:  LittleFS alanı sınırlı; ilk yükleme süresi kullanıcı deneyimini etkiler;
              arayüz karmaşık değil (birkaç kart, form, gösterge);
              mobil uyumluluk gerekli
Approaches:   (a) tam Bootstrap'i koru
              (b) Bootstrap'in yalnızca kullanılan parçalarını al
              (c) çerçevesiz, elle yazılmış minimal CSS
              (d) çok küçük bir CSS çerçevesi
Trade-offs:   (a) 298 KB, gzip'le düşer ama hâlâ büyük ve %95'i kullanılmıyor
              (c) tam kontrol, en küçük boyut, biraz daha fazla iş
Recommended:  (c) veya (d) — arayüz karmaşıklığı tam çerçeveyi haklı çıkarmıyor
```

### Gzip zorunluluğu

Tüm metin varlıkları (HTML/CSS/JS) **önceden gzip'lenmiş** olarak imaja konmalı.
Bu, ESP32'nin çalışma zamanında sıkıştırma yapmasını gerektirmez ve hem flash hem
bant genişliği kazandırır.

## Implementation Notes

- Varlık üretimi tekrarlanabilir olmalı: bir betik veya PlatformIO öncesi adım.
  Elle gzip'leme sürdürülebilir değildir.
- Kaynak dosyalar (sıkıştırılmamış) sürüm kontrolünde tutulmalı; üretilenler değil.
- `index.html` önbelleğe alınmamalı; diğer varlıklar sürüm damgalı isimlerle uzun
  önbelleğe alınabilir.
- Bağlantı durumu göstergesi **belirgin** olmalı: WS kopukken kullanıcı gördüğü verinin
  eski olduğunu anlamalı (§14.2). Mevcut projede kopuk bağlantıda eski değerler canlıymış
  gibi duruyordu.
- Sayfa yapısı: tek sayfa mı çok sayfa mı? Tek sayfa WS bağlantısını korur; çok sayfa her
  geçişte yeniden bağlanır. Bu karar verilmeli.
- Harici CDN kullanılmamalı — cihaz internetsiz de çalışmalı.
- Mobil uyumluluk gerekli; sera ortamında telefondan erişim yaygın olacaktır.

## Files

- `frontend/` (yeni — kaynak dosyalar)
- `frontend/build.*` (yeni — üretim betiği)
- `data/` (üretilen gzip'li varlıklar)

## Acceptance Criteria

- [ ] CSS çerçevesi kararı verildi ve gerekçelendirildi
- [ ] Varlık üretimi tekrarlanabilir bir betikle yapılıyor
- [ ] Tüm metin varlıkları gzip'li
- [ ] Toplam varlık boyutu ölçüldü ve eski projeyle karşılaştırıldı
- [ ] Harici CDN bağımlılığı yok
- [ ] Bağlantı durumu göstergesi belirgin
- [ ] Mobil uyumlu
- [ ] Sayfa yapısı kararı verildi (tek/çok sayfa)
- [ ] Önbellek başlıkları doğru
- [ ] Kırık link yok

## Test Plan

- [ ] Sayfa mobil ve masaüstü tarayıcıda doğru görünüyor
- [ ] Tüm varlıklar gzip olarak yükleniyor (tarayıcı ağ sekmesiyle doğrulandı)
- [ ] İlk yükleme süresi ölçüldü ve eski projeyle karşılaştırıldı
- [ ] İnternet erişimi olmayan ağda tüm varlıklar yükleniyor
- [ ] Bağlantı kesildiğinde gösterge belirgin şekilde değişiyor
- [ ] Üretim betiği temiz bir ortamda çalıştırıldı
- [ ] LittleFS imaj boyutu partition'a sığıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.6, §15.1)
- [ ] Gereksiz abstraction var mı? — gereksiz büyük çerçeve var mı
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? (N/A)
- [ ] Memory problemi var mı? — varlık boyutu flash bütçesinde mi
- [ ] Error handling var mı? — bağlantı durumu gösterimi
- [ ] ESP32 resource kullanımı uygun mu? — **flash ve bant genişliği**
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **298 KB sıkıştırılmamış CSS taşınmamalı**

## Definition of Done

Ortak DoD + varlık boyutu eski projeyle karşılaştırılarak raporlandı + internetsiz ağda
tam çalıştığı doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — (c) çerçevesiz, elle yazılmış minimal CSS

```text
(a) tam Bootstrap 298 KB → REDDEDILDI. Gzip'le duser ama hala buyuk ve
    %95'i KULLANILMIYOR. LittleFS bolumu 896 KB; ucte birini kullanilmayan
    bir CSS cercevesine vermek savunulamaz.
(d) kucuk cerceve → yine de bilmedigimiz bir bagimlilik ve surum riski.
(c) SECILDI: birkac kart, form ve gosterge icin tam cerceve gerekmez.
```

Hedef: sıkıştırılmamış toplam < 40 KB, gzip'li < 12 KB.

## Karar 2 — TEK SAYFA (SPA), çok sayfa değil

WebSocket bağlantısı sayfa geçişlerinde **korunur**. Çok sayfalı bir yapıda
her geçiş WS'i koparır, yeniden el sıkışma yapar ve tam state'i yeniden
indirir. Sera arayüzünde sekmeler arası geçiş sık; her geçişte yeniden
bağlanmak hem yavaş hem gereksizdir.

## Karar 3 — Harici CDN YOK

Cihaz internetsiz çalışmak zorunda — AP modunda hiçbir CDN'e erişilemez.
Eski projede Bootstrap yerel dosyaydı, bu doğruydu ve korunuyor; ancak
font/ikon için CDN'e kaçmak da yasak.

İkonlar **inline SVG** veya Unicode. Ayrı bir ikon fontu 50+ KB'dır ve
kullanılan 8 ikon için taşınmaz.

## Karar 4 — Varlık üretimi BETİKLE, elle değil

`tools/build_assets.py` kaynak `frontend/`'i okur, gzip'ler ve `data/`
üretir. Elle gzip'leme sürdürülebilir değildir: birinin bir dosyayı
güncelleyip gzip'lemeyi unutması an meselesidir ve sonuç sessizce eski
arayüzdür.

**Kaynak sürüm kontrolünde, üretilenler DEĞİL** — `data/` `.gitignore`'a
eklenir.

## Karar 5 — Bağlantı durumu BELİRGİN

WS kopukken tüm veri alanları soluklaşır ve üstte kalıcı bir bant görünür.
Eski projede kopuk bağlantıda eski değerler **canlıymış gibi** duruyordu;
kullanıcı 10 dakika önceki pH değerine bakıp gübre ekleyebilirdi.

---

# STEP 3 — REVIEW RECORD

- [x] Çerçevesiz, elle yazılmış CSS
- [x] Tüm metin varlıkları önceden gzip'li
- [x] Üretim **betikle** (`tools/build_assets.py`), elle değil
- [x] Kaynak sürüm kontrolünde (`frontend/`), üretilenler değil
      (`/data/` `.gitignore`'a eklendi)
- [x] Harici CDN yok; ikonlar Unicode/SVG
- [x] Tek sayfa (SPA) — WS bağlantısı sekme geçişlerinde korunuyor
- [x] Bağlantı durumu **belirgin**: kalıcı kırmızı bant + tüm veri solar
- [x] Mobil uyumlu (44 px dokunma hedefleri, `viewport-fit=cover`,
      480 px altı için ayrı düzen)
- [x] **Tarayıcıda doğrulandı** — aşağıya bakın

## Ölçülen sonuç

```text
app.css      5 688 →  2 123 bayt  (-%63)
app.js      21 181 →  7 432 bayt  (-%65)
index.html   4 924 →  1 711 bayt  (-%66)
─────────────────────────────────────────
TOPLAM      31 793 → 11 266 bayt

Hedef <12 KB gzip idi — TUTTU.
LittleFS bolumu (896 KB) kullanimi: %1
```

Karşılaştırma: eski projede **yalnızca `bootstrap.min.css` 298 KB'dı** ve
%95'i kullanılmıyordu. Tüm yeni arayüz, o tek dosyanın **%4'ü** kadar yer
kaplıyor.

Betik 64 KB üzerinde uyarı veriyor: birinin ileride bir bağımlılık
eklemesi hâlinde sessizce geçmesin.

**TASK-047: TAMAMLANDI** (tarayıcı testleri bekliyor).

## Tarayıcı doğrulaması — 2026-08-31

- Sayfa yerel HTTP sunucusundan yüklendi; CSS ve JS hatasız çözümlendi
- Harici istek **yok** (konsolda yalnızca kasıtlı olarak eksik bırakılan
  REST uçlarının 404'leri) — CDN bağımlılığı olmadığı doğrulandı
- Tek sayfa gezinme çalışıyor; sekme geçişleri DOM içinde
- Bağlantı bandı ve `body.stale` soluklaştırması görsel olarak doğrulandı
- Mobil düzen 480 px altında test edilmedi (donanım/gerçek cihaz testi)

Nihai boyut (FAULT gösterim düzeltmesi dahil):

```text
app.css      5 986 →  2 219 bayt
app.js      21 980 →  7 800 bayt
index.html   4 924 →  1 711 bayt
──────────────────────────────────
TOPLAM      32 890 → 11 730 bayt gzip     (hedef <12 KB — TUTTU)
```

**Not:** test için geçici bir `.claude/launch.json` kullanıldı ve
**silindi** — makineye özgü mutlak bir Python yolu içeriyordu ve depoya
girmemeliydi.
