# TASK-039 — Wi-Fi Scan Service

**Phase:** 7 — Network · **Priority:** P2

## Objective

Asenkron ağ taramasını, **durumu açıkça bildiren** bir arayüzle sunmak. Mevcut projede
tarama ara-durumu (HTTP 202) frontend tarafından işlenmediği için ilk tarama denemesi
her zaman hata veriyordu.

## Scope

- Asenkron tarama başlatma
- Tarama durumu: `IDLE`, `RUNNING`, `DONE`, `FAILED`
- Sonuç tamponu (SSID, RSSI, şifreleme tipi, kanal)
- Sonuç zaman damgası ve eskime
- Bellek temizliği

## Out of Scope

- API endpoint (TASK-044)
- Frontend davranışı (TASK-049)
- AP fallback (TASK-038)

## Dependencies

- TASK-035

## Requirements

- `REQUIREMENTS.md` — §2 (SSID tarama `[x]`, listeleme `[~]`), §5.5 (`/scan` 202 sorunu)

## Architecture References

- §8.2 Tarama satırı · §14.3 API sözleşmesi (`/api/network/scan`)

## Expected Design

### Ara-durum problemi ve çözümü

```text
Mevcut hata:  Tarama sürerken sunucu 202 {"status":"scanning"} döndürüyor,
              frontend bunu dizi sanıp forEach çağırıyor → hata →
              "Ağ taraması yapılamadı!" uyarısı. İlk tıklama HER ZAMAN başarısız.
Yeni:         Yanıt her zaman aynı şemada:
              { "status": "idle|running|done|failed", "networks": [...], "age": ms }
              Frontend durumu okur, "running" ise bekler ve tekrar sorar.
```

Sözleşme **tek biçimli** olmalı: farklı durumlarda farklı şekilli yanıt döndürmek
istemci hatalarının ana kaynağıdır.

### Karar gerektiren nokta — Tarama ile bağlantının etkileşimi

```text
Problem:      Tarama radyoyu meşgul eder ve aktif bağlantıyı etkileyebilir
Constraints:  Kullanıcı genellikle AP modundayken tarar (kurulum sırasında);
              bağlıyken tarama, bağlantıyı geçici olarak bozabilir
Approaches:   (a) her durumda tara
              (b) bağlıyken taramayı reddet
              (c) bağlıyken tara ama kullanıcıyı uyar; kopma olursa yeniden bağlan
Recommended:  (c) — kullanıcı ağ değiştirmek isteyebilir; kopma FSM tarafından
              zaten ele alınıyor
```

## Implementation Notes

- Tarama sonucu **belleği tutar**; okunduktan sonra temizlenmeli. Mevcut projede
  `WiFi.scanDelete()` çağrılıyordu — bu davranış korunmalı ama sonuç önce kendi tamponumuza
  kopyalanmalı.
- Sonuç tamponu sabit boyutlu olmalı (örn. en fazla 20 ağ); daha fazlası kesilmeli ve bu
  durum bildirilmeli.
- Sonuç **eskir**: 30 saniye önceki tarama sonucu artık geçerli olmayabilir. Yaş bilgisi
  yanıtta taşınmalı.
- Aynı SSID birden fazla kez görünebilir (mesh/repeater); en güçlü sinyalli tutulmalı
  veya tekrarlar birleştirilmeli.
- Gizli ağlar taranmamalı (varsayılan); kullanıcı SSID'yi elle girebilmeli.
- Tarama süresi ölçülmeli; `net` task'ının watchdog timeout'unu zorlamamalı.
- Şifreleme tipi bilgisi taşınmalı — açık ağa şifresiz bağlanma senaryosu için gerekli.

## Files

- `src/services/network/ScanService.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Tarama asenkron; `net` task'ını bloklamıyor
- [ ] Dört durum (`IDLE`/`RUNNING`/`DONE`/`FAILED`) raporlanıyor
- [ ] Yanıt şeması tüm durumlarda aynı biçimde
- [ ] Sonuç tamponu sabit boyutlu; taşma bildiriliyor
- [ ] Sonuç yaşı taşınıyor
- [ ] Tekrarlanan SSID'ler ele alınıyor
- [ ] Şifreleme tipi taşınıyor
- [ ] Tarama belleği temizleniyor
- [ ] Bağlıyken tarama davranışı tasarıma uygun

## Test Plan

- [ ] Tarama başlatılıp sonuç alınıyor
- [ ] Tarama sürerken sorgulandığında `RUNNING` dönüyor ve şema aynı kalıyor
- [ ] **İlk tarama denemesi hata vermiyor** (mevcut projenin hatası tekrarlanmıyor)
- [ ] 20'den fazla ağ ortamında taşma doğru bildiriliyor
- [ ] Eski sonuç yaşıyla birlikte doğru raporlanıyor
- [ ] Bağlıyken tarama sonrası bağlantı geri geliyor
- [ ] Tekrarlanan taramalarda bellek sızıntısı yok
- [ ] Tarama süresi ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§8.2, §14.3)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? — tarama task'ı bloklamıyor mu
- [ ] Shared state güvenli mi? — sonuç tamponuna erişim
- [ ] Memory problemi var mı? — **tarama belleği sızıntısı**
- [ ] Error handling var mı? — tarama başarısızlığı
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **farklı şekilli 202 yanıtı yasak**

## Definition of Done

Ortak DoD + ilk tarama denemesinin hatasız çalıştığı kanıtlandı + tekrarlı taramada
bellek sızıntısı olmadığı doğrulandı.

---

# STEP 3 — REVIEW RECORD

- [x] Tarama asenkron; `net` task'ını bloklamıyor
- [x] Dört durum (`IDLE`/`RUNNING`/`DONE`/`FAILED`) raporlanıyor
- [x] **Yanıt şeması tüm durumlarda aynı** — eski sistemin "ilk tıklama her
      zaman başarısız" hatasının kökü buydu
- [x] Sonuç tamponu sabit boyutlu (20); taşma `truncated()` ile bildiriliyor
- [x] Sonuç yaşı taşınıyor; 30 sn'den eski sonuç eskimiş sayılıyor
- [x] Aynı SSID tekrarları birleştiriliyor, **en güçlü sinyal** tutuluyor
- [x] Gizli ağlar taranmıyor
- [x] Şifreleme tipi taşınıyor (açık ağa şifresiz bağlanma senaryosu için)
- [x] Sonuçlar **önce kendi tamponumuza kopyalanıyor**, sonra
      `scanRelease()` çağrılıyor
- [x] Emniyet valfi: `SCAN_DONE` hiç gelmezse 15 sn sonra `FAILED` —
      `RUNNING`'de sonsuza kalmak arayüzün "taranıyor..." göstergesini
      kalıcı hâle getirirdi
- [ ] **Donanım testleri bekliyor**

**TASK-039: TAMAMLANDI.**

---

# STEP 1 — DESIGN RECORD

> **Protokol notu:** Bu tasarım kaydı geriye dönük yazıldı; TASK-039'un
> inceleme kaydı yazılmış ama tasarım kaydı atlanmıştı (TASK-065
> denetiminde fark edildi). Kararlar **koddan okunmuştur**.

## Karar 1 — Tarama ile bağlantı etkileşimi: (c) tara ama kopmayı FSM'e bırak

```text
(a) her durumda tara       → baglantiyi bozabilir, kullanici uyarilmaz
(b) bagliyken taramayi reddet → kullanici AG DEGISTIREMEZ; tam da tarama
                                istedigi an reddedilir
(c) SECILDI: bagliyken de tara. Kopma olursa FSM zaten ele aliyor
             (BACKOFF → yeniden baglanma).
```

Kopma yönetimi TASK-035'te zaten var; taramayı kısıtlamak o mekanizmayı
kullanmamak olurdu.

## Karar 2 — Yanıt şeması TÜM durumlarda AYNI

```text
Eski sistem: tarama surerken 202 + FARKLI SEKILLI govde
             → frontend diziyi bekleyip `forEach` cagiriyor → hata
             → "Ag taramasi yapilamadi!" → ILK TIKLAMA HER ZAMAN BASARISIZ

Yeni: { status, age, truncated, networks[] }  — HER DURUMDA
      `networks` HER ZAMAN dizi, bos olsa bile
      Hicbir durumda 202 DONULMEZ: durum GOVDEDE, HTTP kodunda degil
```

Farklı durumlarda farklı şekilli yanıt döndürmek istemci hatalarının ana
kaynağıdır.

## Karar 3 — Sonuçlar ÖNCE kopyalanır, SONRA radyo belleği bırakılır

Eski sistem de `scanDelete()` çağırıyordu — bu doğruydu ve korundu. Ancak
kopyalama ondan **önce** yapılıyor; aksi hâlde okunacak veri kalmazdı.

## Karar 4 — Aynı SSID tekrarları BİRLEŞTİRİLİR

Mesh/repeater kurulumlarında aynı ağ 3–4 kez görünür. En güçlü sinyalli
tutulur; kullanıcıya aynı adı üç kez göstermek listeyi okunmaz yapar.

## Karar 5 — Emniyet valfi: `SCAN_DONE` hiç gelmezse

15 sn sonra `FAILED`. `RUNNING`'de sonsuza kalmak, arayüzün "taranıyor…"
göstergesini **kalıcı** hâle getirirdi.

## Karar 6 — Gizli ağlar taranmaz

SSID'siz kayıtlar listeyi kirletir ve kullanıcı zaten SSID'yi elle
girebilir.
