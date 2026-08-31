# TASK-046 — Telemetry Publisher & Backpressure

**Phase:** 9 — Web Backend · **Priority:** P2

## Objective

Durum değişikliklerini WebSocket üzerinden verimli şekilde yayınlamak ve yavaş bir
istemcinin sistemi geriye doğru bloklamasını önlemek.

## Scope

- Değişim tabanlı yayın (versiyon karşılaştırması)
- Yayın hızı sınırı (en fazla 1 Hz veya değişimde)
- Yavaş istemcide paket düşürme (backpressure)
- Yayın yükünün ölçülmesi
- Olay bildirimleri (`event` mesajları)

## Out of Scope

- WS protokol şeması (TASK-045)
- Frontend işleme (TASK-048)

## Dependencies

- TASK-045, TASK-007

## Requirements

- `REQUIREMENTS.md` — §5.4 (sunucu veri push etmiyor, HTTP polling kullanılıyor)

## Architecture References

- §4.2 Versiyonlama kullanımı · §14.6 Yavaş istemci kuralı
- §3.1 Sensör → Web akışı

## Expected Design

### HTTP polling'in kaldırılması

```text
Mevcut:   Frontend her 600 ms'de GET /api/sensors
          → sürekli HTTP isteği, gereksiz trafik ve CPU
Yeni:     Sunucu değişimde push eder; polling yok
```

### Karar gerektiren nokta — Yayın tetikleme

```text
Problem:      State ne zaman yayınlanmalı?
Constraints:  Sensörler 250 ms'de bir güncelleniyor → her değişimde yayın çok sık;
              hiç değişmiyorsa yayın gereksiz;
              kullanıcı gecikme hissetmemeli;
              aktüatör durumu değişimi ANINDA görünmeli
Approaches:   (a) sabit periyot (örn. 1 Hz)
              (b) yalnızca versiyon değişince
              (c) hibrit: kritik değişimde anında, diğerlerinde hız sınırlı
Trade-offs:   (b) tek başına çok sık tetiklenir (sensör gürültüsü versiyonu artırır)
Recommended:  (c) — aktüatör/güvenlik/mod değişimi anında; sensör değerleri hız sınırlı
```

### Backpressure

Yavaş bir istemci (zayıf Wi-Fi, arka plandaki sekme) yazma kuyruğunu doldurabilir.
Bu durumda **telemetri paketi düşürülür** — istemci bir sonraki paketle güncel duruma
zaten yetişir. Ancak `ack` ve `event` mesajları düşürülmemeli; bunlar tekrarlanmaz.

## Implementation Notes

- Yayın `net` task'ından yapılmalı, ayrı task açılmamalı (§6.4).
- Paket boyutu ölçülmeli ve sınırlandırılmalı; her istemciye gönderilen büyük paket
  RAM'de çoğaltılır.
- Çok istemcili durumda tek serileştirme yapılıp aynı tampon paylaşılmalı; her istemci
  için ayrı JSON üretmek israftır.
- Kısmi/delta yayın düşünülebilir ancak karmaşıklık getirir; **önce tam state ile ölçüm
  yapılmalı**, gerçekten gerekliyse delta eklenmelidir. Erken optimizasyon yapılmamalı.
- Yayın yükü (bayt/saniye, CPU) ölçülmeli ve kaydedilmeli.
- İstemci yokken serileştirme **hiç yapılmamalı** — boşa CPU harcanmamalı.
- Değişim tespiti için `StateStore.version()` kullanılmalı; state kopyalayıp karşılaştırmak
  israftır.

## Files

- `src/interfaces/web/TelemetryPublisher.h` / `.cpp` (yeni)

## Acceptance Criteria

- [ ] Yayın tetikleme stratejisi seçildi ve gerekçelendirildi
- [ ] Kritik değişimler (aktüatör, güvenlik, mod) anında yayınlanıyor
- [ ] Sensör güncellemeleri hız sınırlı
- [ ] Değişim yoksa yayın yapılmıyor
- [ ] İstemci yokken serileştirme yapılmıyor
- [ ] Yavaş istemcide telemetri paketi düşürülüyor
- [ ] `ack` ve `event` mesajları düşürülmüyor
- [ ] Çok istemcide tek serileştirme paylaşılıyor
- [ ] Paket boyutu ve yayın yükü ölçüldü
- [ ] HTTP polling tamamen kaldırıldı

## Test Plan

- [ ] Aktüatör durumu değişince istemci anında güncelleniyor (gecikme ölçüldü)
- [ ] Sensör değişimlerinde yayın hızı sınırı çalışıyor
- [ ] Değişim olmadığında trafik oluşmuyor
- [ ] Yavaş istemci simülasyonunda sistem yavaşlamıyor
- [ ] Yavaş istemcide `ack` mesajı kaybolmuyor
- [ ] Beş eşzamanlı istemcide CPU ve bellek ölçüldü
- [ ] Uzun süreli yayında heap sabit
- [ ] İstemci yokken CPU kullanımı ölçüldü

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.6, §4.2)
- [ ] Gereksiz abstraction var mı? — erken delta optimizasyonu yapılmış mı
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi?
- [ ] Memory problemi var mı? — **çok istemcili paket çoğaltma**
- [ ] Error handling var mı? — gönderim hatası
- [ ] ESP32 resource kullanımı uygun mu? — yayın yükü ölçüldü mü
- [ ] Task sorumluluğu doğru mu? — ayrı task açılmamış mı
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **600 ms polling deseni kaldırıldı mı**

## Definition of Done

Ortak DoD + yayın gecikmesi ve yükü ölçüldü + çok istemcili kararlılık doğrulandı.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — (c) hibrit tetikleme

```text
KRITIK degisim (aktuator / guvenlik / mod / ag durumu) → ANINDA yayin
Diger degisimler (sensor degerleri)                    → HIZ SINIRLI

Sensorler 250 ms'de bir guncelleniyor ve her guncelleme versiyonu artiriyor;
(b) "yalnizca versiyon degisince" tek basina saniyede 4 paket uretir ve
sensor gurultusu bunu bosuna yapar.

Hiz siniri config'ten: `system.telemetryIntervalMs` (varsayilan 1000 ms).
```

Kritik değişimin **anında** yayınlanması pazarlıksız: kullanıcı pompayı
açtığında kartın 1 saniye sonra güncellenmesi, komutun çalışmadığı izlenimi
verir ve kullanıcı tekrar basar.

## Karar 2 — Backpressure: telemetri DÜŞÜRÜLÜR, `ack`/`event` DÜŞÜRÜLMEZ

```text
Yavas istemci (zayif Wi-Fi, arka plandaki sekme) yazma kuyrugunu doldurur.

telemetri (`state`) → DUSURULUR. Istemci bir sonraki paketle guncel duruma
                      zaten yetisir; state KUMULATIF degil, TAM goruntudur.
ack / event         → DUSURULMEZ. Bunlar TEKRARLANMAZ; dusurulen bir ack
                      istemciyi sonsuza kadar "BEKLIYOR" durumunda birakir.
```

Bu ayrım, state'in tam görüntü olarak tasarlanmasının doğrudan kazancıdır.

## Karar 3 — HTTP polling KALDIRILDI

Eski frontend her 600 ms'de `GET /api/sensors` yapıyordu: sürekli HTTP
isteği, TCP el sıkışması, JSON üretimi ve CPU. Sunucu artık değişimde
push eder. `GET /api/state` **kalır** ama yalnızca ilk yükleme ve WS'siz
istemciler için.

## Karar 4 — Yayın `net` task'ından, ayrı task YOK

ARCHITECTURE §6.4: yeni task açmak için gerekçe gerekir. Telemetri
yayını ağ işidir ve `net` zaten 100 ms'de bir dönüyor.

---

# STEP 3 — REVIEW RECORD

- [x] Değişim tabanlı yayın (`version` karşılaştırması)
- [x] **Hibrit tetikleme**: kritik değişim anında, sensör değerleri hız sınırlı
- [x] Hız sınırı config'ten (`system.telemetryIntervalMs`)
- [x] Yavaş istemcide telemetri **düşürülüyor** (`canSend()` kontrolü);
      sayaç tutuluyor
- [x] `ack` ve `event` **düşürülmüyor**
- [x] HTTP polling kaldırıldı; `GET /api/state` yalnızca ilk yükleme için
- [x] Yayın `net` task'ından; ayrı task açılmadı (§6.4)
- [ ] **Yayın yükü ölçümü — donanım gerekiyor**

## `criticalChanged()` neyi kritik sayıyor

```text
mod · kilit maskesi · acil durum mandali · ag durumu · her aktuatorun
GERCEK durumu ve engel nedeni
```

Kullanıcı pompayı açtığında kartın 1 saniye sonra güncellenmesi, komutun
çalışmadığı izlenimi verir ve kullanıcı tekrar basar. Sensör gürültüsünün
saniyede 4 paket üretmesi ise gereksizdir. Ayrım tam da buradan geçiyor.

## Backpressure ayrımının dayanağı

`state` **kümülatif değil, tam görüntüdür**. Düşürülen bir paket bilgi
kaybı yaratmaz; istemci bir sonrakiyle güncel duruma yetişir. `ack` ise
tekrarlanmaz — düşürülen bir ack istemciyi sonsuza kadar "BEKLİYOR"da
bırakır. Bu, state'in tam görüntü olarak tasarlanmasının doğrudan kazancı.

**TASK-046: TAMAMLANDI** (yük ölçümü bekliyor).
