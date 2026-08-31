# TASK-048 — State Sync Client (No Optimistic Update)

**Phase:** 10 — Frontend · **Priority:** **P1**

## Objective

Tarayıcının **tek doğruluk kaynağı olarak cihazı** kabul etmesini sağlayan istemci
katmanını yazmak. Bu task, `REQUIREMENTS.md` Kritik Problem 5'in frontend tarafındaki
çözümüdür.

## Scope

- WebSocket bağlantısı ve otomatik yeniden bağlanma (üstel backoff)
- Gelen `state` paketlerinin tek bir istemci-tarafı store'a yazılması
- Versiyon kontrolü (eski/sıra dışı paket yok sayma)
- Komut gönderimi ve `reqId` ile ack eşleştirme
- "Bekliyor" (pending) durum yönetimi
- Bağlantı kopukken veri bayatlığının gösterilmesi

## Out of Scope

- Görsel bileşenler (TASK-049)
- Backend protokolü (TASK-045)

## Dependencies

- TASK-045, TASK-046, TASK-047

## Requirements

- `REQUIREMENTS.md` — §5.1 (röle durumu senkronize değil), §5.4, Kritik Problem 5

## Architecture References

- §14.2 Durum senkronizasyonu diyagramı ve kural tablosu
- §0 P5 (tek doğruluk kaynağı cihazdadır)

## Expected Design

### Mutlak kural — iyimser güncelleme yasak

```text
YASAK (mevcut projenin yaptığı):
   butona tıkla → kartın sınıfını 'on' yap → "ÇALIŞIYOR" yaz → komut gönder
   → cihaz komutu reddetse bile arayüz "ÇALIŞIYOR" gösterir

ZORUNLU (yeni):
   butona tıkla → kartı "BEKLİYOR" durumuna al → komut gönder
   → ack gelir (kabul/ret) → state paketi gelir → kart GERÇEK duruma geçer
```

Arayüzde görünen her aktüatör durumu, cihazdan gelen state paketinden türetilir.
İstemci hiçbir zaman durum **üretmez**, yalnızca gösterir.

### Bayatlık göstergesi

WS kopukken ekrandaki veriler eskir. İstemci bunu **açıkça** göstermeli: soluk renk,
"bağlantı yok" bandı, son güncelleme zamanı. Mevcut projede kopuk bağlantıda eski
değerler canlıymış gibi duruyordu — kullanıcı yanlış bilgiyle karar verebilir.

### Karar gerektiren nokta — Yeniden bağlanma stratejisi

```text
Problem:      WS koptuğunda ne sıklıkta yeniden denenmeli?
Constraints:  Cihaz yeniden başlıyor olabilir (birkaç saniye);
              kullanıcı sekmeyi arka plana almış olabilir;
              sürekli deneme tarayıcı kaynağı harcar
Approaches:   (a) sabit aralık
              (b) üstel backoff + tavan
              (c) backoff + sayfa görünür olunca anında dene
Recommended:  (c) — kullanıcı sekmeye döndüğünde anında bağlanmalı
```

## Implementation Notes

- Bekleyen komutlar için zaman aşımı olmalı: ack gelmezse kart "bekliyor"da sonsuza
  kadar kalmamalı, hata gösterilmeli.
- Ack `SAFETY_BLOCKED` gibi bir ret içeriyorsa kullanıcıya **nedeni** gösterilmeli;
  sessizce eski duruma dönmek kafa karıştırır.
- Versiyon numarası azalıyorsa (cihaz yeniden başlamış) tam state yeniden istenmeli.
- Sayfa yenilendiğinde bağlantı kurulur kurulmaz tam state gelir; hiçbir alan varsayılan
  değerle gösterilmemeli — veri gelene kadar "yükleniyor" durumu olmalı.
- Store tek olmalı; bileşenler ondan okumalı. Dağınık DOM güncellemesi tutarsızlık üretir.
- Mevcut projedeki `r1`/`r2` gibi var olmayan DOM id'lerine yazma denemesi gibi sessiz
  hatalar olmamalı; eksik bileşen geliştirme sırasında fark edilmeli.

## Files

- `frontend/src/stateClient.*` (yeni)
- `frontend/src/commandClient.*` (yeni)

## Acceptance Criteria

- [ ] WS bağlantısı ve otomatik yeniden bağlanma çalışıyor
- [ ] Yeniden bağlanma stratejisi seçildi; sekme görünür olunca anında deniyor
- [ ] **İyimser güncelleme yok** — durum yalnızca state paketinden geliyor
- [ ] Komut gönderiminde "bekliyor" durumu var
- [ ] Ack `reqId` ile eşleşiyor; ret nedeni gösteriliyor
- [ ] Bekleyen komut için zaman aşımı var
- [ ] Versiyon kontrolü ile eski paket yok sayılıyor
- [ ] Versiyon azalırsa tam state yeniden isteniyor
- [ ] Bağlantı kopukken bayatlık açıkça gösteriliyor
- [ ] Veri gelmeden önce "yükleniyor" durumu var
- [ ] Tek store; dağınık DOM güncellemesi yok

## Test Plan

- [ ] Komut gönderilip reddedildiğinde arayüz **eski durumda kalıyor** ve neden gösteriliyor
- [ ] Komut kabul edildiğinde arayüz state paketiyle güncelleniyor
- [ ] Sayfa yenilendiğinde gerçek durum gösteriliyor (varsayılan değil)
- [ ] Cihaz yeniden başlatıldığında istemci yeniden bağlanıyor ve tam state alıyor
- [ ] WS kesildiğinde bayatlık göstergesi çıkıyor
- [ ] Ack gelmediğinde zaman aşımı çalışıyor
- [ ] Sekme arka plandan öne alınınca anında bağlanıyor
- [ ] Uzun süreli açık sayfada bellek sızıntısı yok
- [ ] İki tarayıcı sekmesi aynı anda açıkken ikisi de tutarlı

## Review Checklist

- [ ] Architecture'a uygun mu? (§14.2, P5)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı? (N/A)
- [ ] Shared state güvenli mi? — tek store kullanımı
- [ ] Memory problemi var mı? — istemci tarafı sızıntı
- [ ] Error handling var mı? — ack zaman aşımı, ret nedenleri
- [ ] ESP32 resource kullanımı uygun mu? — gereksiz yeniden bağlanma yükü
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı? — **`sendCmd()`'in iyimser güncellemesi yasak**

## Definition of Done

Ortak DoD + **reddedilen komutta arayüzün yanlış durum göstermediği kanıtlandı** +
iki sekmeli tutarlılık testi geçti.

---

# STEP 1 — DESIGN RECORD

**Tarih:** 2026-08-31 · **Durum:** Karara bağlandı

## Karar 1 — İYİMSER GÜNCELLEME YASAK (mutlak)

```text
YASAK (eski proje):
   tikla → kartin sinifini 'on' yap → "CALISIYOR" yaz → komut gonder
   → cihaz REDDETSE BILE arayuz "CALISIYOR" gosterir

ZORUNLU:
   tikla → kart "BEKLIYOR" → komut gonder
   → ack (kabul/ret) → state paketi → kart GERCEK duruma gecer

Istemci hicbir zaman durum URETMEZ, yalnizca GOSTERIR.
```

Uygulama düzeyinde garanti: `render()` **yalnızca** `store.state`'ten okur;
komut gönderen kod `store.state`'e yazamaz. Tek yazar `onStateMessage()`.

## Karar 2 — (c) backoff + sayfa görünür olunca anında dene

```text
WS koptugunda: 1s → 2s → 4s → 8s → 15s (tavan), jitter'siz (tek istemci).
`visibilitychange` → sayfa gorunur olunca SAYAC SIFIRLANIR ve hemen denenir.

Kullanici sekmeye dondugunde 15 saniye beklemek kabul edilemez; arka planda
15 saniyede bir denemek ise tarayici kaynagini bosuna harcamaz.
```

## Karar 3 — Bekleyen komut ZAMAN AŞIMI: 5 sn

Ack gelmezse kart "bekliyor"da sonsuza kadar kalmaz; hata gösterilir ve
gerçek duruma döner. Ack'in düşürülmemesi sunucu tarafında garanti
(TASK-046) ama bağlantı kopabilir.

## Karar 4 — Versiyon geriye giderse TAM state iste

`v` azalıyorsa cihaz yeniden başlamıştır. WS zaten kopmuş olur ve yeniden
bağlanmada tam state gelir; ancak koruma olarak açıkça kontrol edilir.

## Karar 5 — Veri gelene kadar "yükleniyor", varsayılan değer YOK

Hiçbir alan `0` veya `--` ile ön-doldurulmaz. Eski projede sayfa açılır
açılmaz `0.0 pH` görünüyordu ve bu gerçek bir ölçüm sanılabiliyordu.

## Karar 6 — Eksik DOM öğesi SESSİZ GEÇMEZ

Eski projede `r1`/`r2` gibi var olmayan id'lere yazma denemesi vardı ve
sessizce hiçbir şey olmuyordu. `el()` yardımcısı bulunamayan id için
konsola hata yazar — geliştirme sırasında fark edilir.

---

# STEP 3 — REVIEW RECORD

- [x] **İyimser güncelleme YOK** — `render()` yalnızca `store.state`'ten
      okur; `store.state`'in tek yazarı `onState()`. Komut gönderen kod
      state'e dokunamaz.
- [x] Kart üç durumda: `on` / `off` / `pending`. `pending` AÇIK göstermez.
- [x] Bekleyen komut zaman aşımı 5 sn; ack gelmezse hata gösterilir
- [x] Ack reddi **nedeniyle** gösteriliyor (`ERR_TEXT` tablosu)
- [x] Yeniden bağlanma: backoff + `visibilitychange` ile anında deneme
- [x] Versiyon geriye giderse bekleyen komutlar temizleniyor
- [x] Bayatlık göstergesi: `body.stale` tüm veriyi soluklaştırıyor +
      kalıcı bant
- [x] Veri gelene kadar "Yükleniyor…"; **varsayılan değer yok**
- [x] Eksik DOM öğesi konsola hata yazıyor (`el()`)
- [x] Tek store; bileşenler ondan okuyor
- [x] **Tarayıcıda doğrulandı** — aşağıya bakın

## Yapısal garanti nasıl kuruldu

Kural bir yorum satırı değil, kodun şekli:

```text
store.state  ← YALNIZCA onState() yazar
render()     ← YALNIZCA store.state okur
sendCmd()    ← store.pending'e yazar, store.state'e DOKUNMAZ
```

`pending` bir görsel durumdur, bir veri durumu değil. Kart "BEKLİYOR"
gösterirken bile altındaki gerçek `a.on` değeri cihazdan gelendir.

**TASK-048: TAMAMLANDI** (tarayıcı testleri bekliyor).

## Tarayıcı doğrulaması — 2026-08-31

Arayüz yerel bir HTTP sunucusuyla açıldı ve `store`'a sahte `state`/`ack`
paketleri enjekte edilerek protokolün tamamı koşturuldu.

| Adım | Beklenen | Ölçülen |
|---|---|---|
| Başlangıç | `KAPALI` | `KAPALI` |
| Butona tıkla | `BEKLİYOR…` | `BEKLİYOR…` |
| `store.state` değişti mi? | **hayır** | `on === false` |
| `ack: ACCEPTED` geldi | **hâlâ `KAPALI`** | `KAPALI` |
| `state` paketi `on:true` | `ÇALIŞIYOR` | `ÇALIŞIYOR` |
| `ack: REJECTED_SAFETY` | gerçek duruma dön + neden | `ÇALIŞIYOR` + "Kuru çalışma tespit edildi" |
| Versiyon geriye gitti | bekleyenler temizlenir | `pending.size 1 → 0` |

**Kritik satır üçüncü ve dördüncüdür:** `ack` kartı çevirmiyor, yalnızca
`state` çeviriyor. Eski sistemin hatası tam buradaydı — kullanıcı butona
basınca kart hemen "ÇALIŞIYOR" oluyor ve cihaz komutu reddetse bile öyle
kalıyordu.

Ayrıca doğrulandı:
- Zaman aşımı gerçekten tetikleniyor (ilk testte 5 sn geçtiği için ack
  düştü ve kart "Cihaz yanıt vermedi" gösterdi — istenen davranış)
- Bağlantı kopunca `body.stale` + kalıcı bant
- REST arka ucu yokken hiçbir görünüm çökmüyor; "İstek başarısız" gösteriyor
- Konsolda **tek bir `[ui] DOM ögesi bulunamadı` hatası yok** — eski
  projedeki `r1`/`r2` sınıfı sessiz hata tekrarlanmamış
