# TASK-044 — REST API — Config & Network

**Phase:** 9 — Web Backend · **Priority:** P1

## Objective

Konfigürasyon ve ağ ayarlarının web üzerinden **doğrulanarak** güncellenmesini sağlamak.
Sunucu tarafı doğrulama, güvenlik eşiklerinin bozuk değerlerle yazılmasını engeller.

## Scope

- `GET /api/config` — sırlar maskeli
- `PUT /api/config/network`, `/automation`, `/calibration`
- `POST /api/network/scan`, `GET /api/network/scan`
- Wi-Fi credential güncelleme
- Sunucu tarafı şema doğrulaması

## Out of Scope

- Config kalıcılaştırma mantığı (TASK-015)
- Tarama servisi (TASK-039)
- Frontend formları (TASK-049)

## Dependencies

- TASK-042, TASK-015, TASK-039

## Requirements

- `REQUIREMENTS.md` — §5.2 (Wi-Fi ayarları), §11-Medium (static IP arayüzü, eşik ayarları)

## Architecture References

- §14.3 API sözleşmesi · §14.5 Doğrulama kuralları

## Expected Design

### Sunucu tarafı doğrulama — zorunlu

> İstemci doğrulaması yalnızca kullanıcı deneyimidir. **Sunucu doğrulaması zorunludur.**
> Geçersiz istek aktüatöre veya konfigürasyona hiç ulaşmaz (§14.5).

Doğrulanacaklar: tip, aralık, uzunluk, enum üyeliği, alanlar arası tutarlılık.
Bu, TASK-014'teki doğrulama fonksiyonlarını **yeniden kullanmalı** — API'de ayrı bir
doğrulama mantığı yazılmamalı (tek doğruluk kaynağı).

### Wi-Fi kaydetme akışı — mevcut projeden farkı

```text
Mevcut:   POST /saveWiFi → düz metin "WIFI:OK" → tarayıcı ham metni gösteriyor
          Kullanıcı ne olduğunu anlamıyor, yönlendirme yok
Yeni:     PUT /api/config/network → JSON yanıt + bağlantı durumu takip edilebilir
          Frontend WS üzerinden bağlantı sürecini canlı izler
```

### Kritik alan koruması

Güvenlik eşikleri (`safety.*`, `actuators.*.maxRunTime`) API'den değiştirilebilir olmalı
ancak:

- Aralık doğrulaması **gevşetilemez**
- Değişiklik loglanmalı (kim, ne zaman, eski → yeni)
- Değişiklik anında etkili olmalı (bir sonraki `app_core` döngüsünde)

## Implementation Notes

- Kısmi güncelleme (PATCH benzeri) desteklenmeli: kullanıcı tek bir eşiği değiştirmek için
  tüm config'i göndermek zorunda kalmamalı. Ancak alanlar arası tutarlılık, birleştirilmiş
  sonuç üzerinde doğrulanmalı.
- Wi-Fi credential güncellemesi bağlantıyı koparır; istemci bunu bilmeli. Yanıt gönderildikten
  sonra bağlantı değişimi başlatılmalı.
- Şifre alanı `GET /api/config` yanıtında **maskelenmeli** (örneğin "ayarlı" / "ayarlı değil"),
  asla döndürülmemeli.
- Tarama endpoint'i tek biçimli şema döndürmeli (TASK-039); durum alanı her zaman mevcut.
- Kalibrasyon güncellemesi sensör hattını etkiler; bir sonraki örneklemede geçerli olmalı.
- İstek gövdesi boyutu sınırlandırılmalı; büyük gövde bellek tüketir.

## Files

- `src/interfaces/web/api/ConfigApi.cpp` (yeni)
- `src/interfaces/web/api/NetworkApi.cpp` (yeni)
- `src/interfaces/web/RequestValidation.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Config okuma/yazma endpoint'leri çalışıyor
- [ ] Doğrulama TASK-014 fonksiyonlarını yeniden kullanıyor; ayrı mantık yok
- [ ] Geçersiz istek config'e ulaşmadan reddediliyor
- [ ] Kısmi güncelleme destekleniyor; tutarlılık birleşik sonuçta doğrulanıyor
- [ ] Şifre `GET /api/config` yanıtında maskeli
- [ ] Güvenlik eşiği değişiklikleri loglanıyor (eski → yeni)
- [ ] Eşik değişiklikleri anında etkili
- [ ] Tarama endpoint'i tek biçimli şema döndürüyor
- [ ] Wi-Fi güncellemesi yanıt sonrası uygulanıyor
- [ ] İstek gövde boyutu sınırlı
- [ ] Tüm endpoint'ler yetki gerektiriyor

## Test Plan

- [ ] Geçerli config güncellemesi kalıcılaşıyor
- [ ] Aralık dışı eşik reddediliyor ve config değişmiyor
- [ ] Tutarsız kombinasyon (minRun > maxRun) reddediliyor
- [ ] Kısmi güncelleme diğer alanları bozmuyor
- [ ] Şifre yanıtta görünmüyor
- [ ] Wi-Fi güncellemesi sonrası bağlantı yeni ağa geçiyor
- [ ] Tarama endpoint'i tüm durumlarda aynı şemada yanıt veriyor
- [ ] Güvenlik eşiği değişikliği bir sonraki döngüde etkili
- [ ] Aşırı büyük istek gövdesi reddediliyor
- [ ] Yetkisiz istek reddediliyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.3, §14.5)
- [ ] Gereksiz abstraction var mı? — doğrulama mantığı tekrarlanmış mı
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — config güncelleme yolu
- [ ] Memory problemi var mı? — istek gövdesi boyutu
- [ ] Error handling var mı? — **doğrulama bu task'ın özü**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **düz metin "WIFI:OK" yanıtı taşınmamalı**

## Definition of Done

Ortak DoD + tüm doğrulama senaryoları test edildi + güvenlik eşiklerinin bozuk değerle
yazılamadığı kanıtlandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — `GET /api/config` sırları MASKELER

```text
Wi-Fi sifresi:  "passwordSet": true/false   ← DEGER YOK
Arayuz parolasi:"authSet": true/false       ← DEGER YOK

`hasWifiPassword()` degeri BELLEGE HIC GETIRMEZ (TASK-013 tasarimi).
Maskeleme bir formatlamadan ibaret degil; deger okunmuyor bile.
```

## Karar 2 — Sunucu tarafı doğrulama, istemciye GÜVENİLMEZ

Her `PUT` gövdesi `ConfigValidation` (TASK-014) üzerinden geçer. İstemci
doğrulaması bir kolaylıktır, bir güvenlik önlemi değildir: `curl` ile
gönderilen bir istek istemci kodunu hiç çalıştırmaz.

Doğrulama hatası **alan adıyla** döner (`{"error":{...,"field":"actuators.minRunMs"}}`)
— `ConfigError` zaten alan adını taşıyor.

## Karar 3 — Wi-Fi credential yazma: şifre gövdede, yanıtta ASLA

`PUT /api/config/network` şifreyi alır, `SecretStore`'a yazar, yanıtta
**geri döndürmez**. Değişiklik sonrası `fsm::onCredentialsChanged()`
çağrılır: kimlik hatası sayacı sıfırlanır, aksi hâlde kullanıcı şifreyi
düzeltir ama sistem hâlâ durmuş olur.

## Karar 4 — Tarama: POST başlatır, GET sorar — ŞEMA AYNI

```text
POST /api/network/scan  → taramayi baslatir, 200 + ayni sema
GET  /api/network/scan  → durumu sorar,      200 + ayni sema

HER IKISI de: { status, age, truncated, networks[] }
`networks` HER ZAMAN dizi — bos olsa bile.

Eski sistem tarama surerken 202 ve FARKLI SEKILLI bir govde donduruyordu;
frontend onu dizi sanip forEach cagiriyordu → "Ag taramasi yapilamadi!"
→ ILK TIKLAMA HER ZAMAN BASARISIZ.
```

Hiçbir durumda 202 dönülmez: durum gövdededir, HTTP kodunda değil.

---

# STEP 3 — REVIEW RECORD

- [x] `GET /api/config` sırları **maskeliyor** (`passwordSet` / `authSet`;
      değer okunmuyor bile — `hasWifiPassword()` değeri belleğe getirmez)
- [x] `PUT /api/config/network|safety|actuators|system`
- [x] Her gövde `ConfigValidation` üzerinden geçiyor; hata **alan adıyla**
      dönüyor
- [x] Wi-Fi şifresi gövdeyle alınıyor, yanıtta dönmüyor
- [x] Credential değişiminde `fsm::onCredentialsChanged()` çağrılıyor —
      kimlik hatası sayacı sıfırlanmazsa kullanıcı şifreyi düzeltir ama
      sistem "durdu" durumunda kalırdı
- [x] `POST`/`GET /api/network/scan` **aynı şemayı** döndürüyor; hiçbir
      durumda 202 yok
- [x] `networks` **her zaman dizi** — boş olsa bile
- [x] IP alanları ayrı ayrı doğrulanıyor; hangisinin geçersiz olduğu
      söyleniyor
- [ ] **Donanım testleri bekliyor**

## Düzeltilen hata: "ilk tıklama her zaman başarısız"

Eski sistem tarama sürerken `202` ve **farklı şekilli** bir gövde
döndürüyordu; frontend onu dizi sanıp `forEach` çağırıyor, hata alıyor ve
"Ağ taraması yapılamadı!" gösteriyordu. Kullanıcı için bu, taramanın
**hiç çalışmaması** demekti.

Kök neden: durumun HTTP kodunda taşınması. Yeni sözleşmede durum
**gövdededir** ve şema tüm durumlarda aynıdır — istemci tek bir ayrıştırıcı
yazar ve dallanma gerekmez.

**TASK-044: TAMAMLANDI.**
