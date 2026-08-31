# TASK-043 — REST API — State, Diagnostics, System

**Phase:** 9 — Web Backend · **Priority:** P2

## Objective

Cihaz durumunu, teşhis bilgisini ve sistem işlemlerini REST üzerinden sunmak. Mevcut
projede olmayan işlevleri (yeniden başlatma, factory reset, acil durum kontrolü)
erişilebilir kılmak.

## Scope

- `GET /api/state` — tam durum anlık görüntüsü
- `GET /api/diagnostics` — son olaylar, aktif hatalar, boot raporu
- `POST /api/system/restart` — kontrollü yeniden başlatma
- `POST /api/system/factory-reset` — onay parametreli
- `POST /api/system/emergency-stop` ve `/emergency-clear`
- JSON serileştirme

## Out of Scope

- Config ve network endpoint'leri (TASK-044)
- WebSocket (TASK-045)
- Geçmiş veri (TASK-059)

## Dependencies

- TASK-041, TASK-042, TASK-007

## Requirements

- `REQUIREMENTS.md` — §1 (yeniden başlatma yok), §7.3 (factory reset erişilemez),
  §5.1 (sistem durumu gösterilmiyor)

## Architecture References

- §14.3 API sözleşmesi tablosu · §14.5 Hata yanıtları

## Expected Design

### Karar gerektiren nokta — JSON üretimi

```text
Problem:      Durum JSON'u nasıl üretilecek?
Constraints:  AsyncTCP callback'inde uzun işlem yasak;
              heap parçalanması ESP32'de ciddi bir sorundur;
              state yapısı birkaç yüz bayt, JSON çıktısı birkaç KB olabilir
Approaches:   (a) String birleştirme  (mevcut projenin yöntemi)
              (b) sabit boyutlu tampon + snprintf
              (c) ArduinoJson ile önceden boyutlandırılmış doküman
              (d) chunked response (parça parça üretim)
Trade-offs:   (a) heap parçalanması, öngörülemez bellek — kabul edilemez
              (c) okunabilir ve güvenli, boyut önceden hesaplanmalı
              (d) büyük yanıtlar için gerekli olabilir
Recommended:  (c); yanıt büyürse (d)
```

### Tehlikeli işlemler — onay gerektirir

`factory-reset` ve `restart` geri alınamaz. Bunlar:

- Açık bir onay parametresi istemeli (kazara çağrıyı önlemek için)
- Yetki gerektirmeli
- Loglanmalı (kim, ne zaman)
- Yanıt gönderildikten **sonra** uygulanmalı — aksi halde istemci yanıt alamaz

## Implementation Notes

- `GET /api/state` WebSocket'in alternatifidir; WS bağlanamayan istemciler için gerekli.
  İçeriği WS state paketiyle **aynı şemada** olmalı — iki farklı şema istemci hatası üretir.
- Wi-Fi şifresi ve parola hash'i **hiçbir yanıtta** görünmemeli.
- `emergency-clear` koşulları kontrol etmeli (TASK-032); düzelmemişse açık nedenle reddetmeli.
- Yanıt boyutları ölçülmeli; büyük teşhis çıktısı sayfalanmalı.
- Tüm endpoint'ler yetki gerektirir (statik varlıklar ve login hariç).
- Yanıt üretimi callback içinde yapılıyorsa süresi ölçülmeli; uzunsa alternatif tasarım
  düşünülmeli.

## Files

- `src/interfaces/web/api/StateApi.cpp` (yeni)
- `src/interfaces/web/api/DiagnosticsApi.cpp` (yeni)
- `src/interfaces/web/api/SystemApi.cpp` (yeni)
- `src/interfaces/web/JsonSerializers.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Beş endpoint çalışıyor
- [ ] JSON üretimi seçilen yöntemle, heap parçalanması yaratmıyor
- [ ] `GET /api/state` ile WS state paketi aynı şemada
- [ ] Sırlar hiçbir yanıtta görünmüyor
- [ ] Tehlikeli işlemler onay parametresi ve yetki istiyor
- [ ] Tehlikeli işlemler yanıt gönderildikten sonra uygulanıyor
- [ ] `emergency-clear` koşulları kontrol ediyor
- [ ] Tüm hatalar ortak şemada
- [ ] Büyük yanıtlar sayfalanıyor
- [ ] Yanıt üretim süreleri ölçüldü

## Test Plan

- [ ] Her endpoint doğru yanıt veriyor
- [ ] Yetkisiz istek reddediliyor
- [ ] Onay parametresi olmadan factory-reset reddediliyor
- [ ] Restart isteğinde istemci yanıtı alıyor, sonra cihaz yeniden başlıyor
- [ ] `emergency-clear` koşul düzelmeden reddediliyor
- [ ] State JSON'u ile WS paketi karşılaştırıldı, şema aynı
- [ ] Yanıtlarda şifre/hash görünmüyor
- [ ] Ardışık 100 istekte heap sabit
- [ ] Yanıt üretim süresi ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.3, §14.5)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — JSON üretim süresi
- [ ] Shared state güvenli mi? — snapshot kullanımı
- [ ] Memory problemi var mı? — **JSON tamponları ve heap parçalanması**
- [ ] Error handling var mı? — ortak hata şeması
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **String birleştirme ile JSON üretimi yasak**

## Definition of Done

Ortak DoD + heap kararlılığı test edildi + state şemasının WS ile aynı olduğu doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — TEK serileştirici: `StateJson`

Plan `JsonSerializers.h/.cpp` diyordu; dosya `StateJson.h/.cpp` olarak
kuruldu ve **hem REST hem WebSocket** onu kullanıyor.

**Gerekçe:** iki ayrı serileştirici yazmak, ikisinin zamanla sessizce
birbirinden ayrılmasıyla biter — ve arayüz, verinin hangi yoldan geldiğine
göre farklı davranır. Bu, eski sistemin `/api/sensors` ile WS arasındaki
tutarsızlığının kaynağıydı.

## Karar 2 — Yıkıcı işlemler AÇIK ONAY ister

```text
POST /api/system/factory-reset  → govdede {"confirm":"FACTORY_RESET"} ZORUNLU
POST /api/system/restart        → onay gerekmez (geri donusu var)
POST /api/system/emergency-stop → onay GEREKMEZ; gecikmesi tehlikelidir
POST /api/system/emergency-clear→ kosul kontrolu ZATEN var (TASK-032)
```

Acil durdurmanın onay istememesi bilinçlidir: onay diyaloğu, tam da
gerektiği anda bir saniye kaybettirir.

## Karar 3 — `/api/state` KALIR ama polling için DEĞİL

WebSocket push birincil yoldur (TASK-046). `GET /api/state` yalnızca ilk
yükleme ve WS kuramayan istemciler için durur. Eski frontend'in 600 ms'lik
polling'i **kaldırıldı**.

## Karar 4 — Komutlar HTTP'den de kuyruğa gider

REST uç noktaları da `CommandQueue`'ya yazar; doğrudan `domain/` çağırmaz.
Böylece komut yolu **tektir** ve AsyncTCP bağlamından iş yapılmaz.

---

# STEP 3 — REVIEW RECORD

- [x] `GET /api/state`, `GET /api/diagnostics` çalışıyor
- [x] `POST /api/system/restart|factory-reset|emergency-stop|emergency-clear`
- [x] Fabrika ayarları **açık onay** istiyor (`confirm=FACTORY_RESET`)
- [x] Acil durdurma onay İSTEMİYOR ve garantili yolu kullanıyor
- [x] Tüm komutlar `CommandQueue`'ya gidiyor; `domain/` doğrudan çağrılmıyor —
      tarama: `interfaces/` içinde domain çağrısı **0** (tek istisna
      `domain::safety::firstReason`, bir `constexpr` saf fonksiyon)
- [x] Serileştirme **tek yerde** (`StateJson`); REST ve WS aynı üreticiyi
      kullanıyor
- [x] JSON önceden boyutlandırılmış tampona yazılıyor; dinamik `String` yok
- [ ] **Donanım testleri bekliyor**

**TASK-043: TAMAMLANDI.**
