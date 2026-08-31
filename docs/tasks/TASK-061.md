# TASK-061 — Failure Injection & Degraded Mode Verification

**Phase:** 14 — Integration & Hardening · **Priority:** **P0**

## Objective

`ARCHITECTURE.md` §16.3'teki arıza → davranış matrisinin **her satırını** gerçek donanımda
kanıtlamak. Sistemin hiçbir arızada durmadığını ve güvenli davrandığını göstermek.

## Scope

- Her arıza senaryosunun kasıtlı olarak üretilmesi
- Beklenen degraded davranışın doğrulanması
- Güvenlik zincirinin arıza koşullarında test edilmesi
- Bulunan sapmaların düzeltilmesi
- Test sonuçlarının raporlanması

## Out of Scope

- Yeni özellik ekleme
- Kaynak profilleme (TASK-062)
- Performans optimizasyonu

## Dependencies

- TASK-060

## Requirements

- `REQUIREMENTS.md` — §9 (tüm hata yönetimi maddeleri), Kritik Problem 4

## Architecture References

- §16.3 Arıza → davranış matrisi · §16.4 Yasaklanan hata davranışları
- §7.2 Boot sonucu → mod · §12 Güvenlik mimarisi

## Expected Design

### Test matrisi (§16.3'ten türetilmiş)

| # | Arıza | Nasıl üretilir | Beklenen davranış |
|---|---|---|---|
| 1 | Sensör kopuk | Kablo çıkar | Kalite `FAULT`, otomasyonda kullanılmaz, güvenlik sensörüyse kilit |
| 2 | Sensör kısa devre | Uçları köprüle | Kalite `FAULT` |
| 3 | Su seviyesi sensörü arızası | Kablo çıkar | **Pompa kilitleniyor** (fail-safe) |
| 4 | Kuru çalışma | Hazne boşken pompa çalıştır | Pompa T saniye içinde duruyor, mandal |
| 5 | Akış sensörü arızası | Kablo çıkar | Pompa durduruluyor, ayrı arıza kodu |
| 6 | Wi-Fi kaybı | AP'yi kapat | Backoff, AP fallback; **otomasyon ve güvenlik etkilenmiyor** |
| 7 | Wi-Fi yanlış şifre | Şifreyi değiştir | Sınırlı deneme, kullanıcıya bildirim |
| 8 | WebSocket kopması | İstemciyi kapat | Sunucu kaynak sızdırmıyor |
| 9 | LittleFS bozuk | Bölümü boz | DEGRADED mod, sistem çalışıyor |
| 10 | NVS bozuk | Bölümü boz | Varsayılan config + WARNING |
| 11 | OLED kopuk | Kablo çıkar | Sistem tam çalışıyor, web erişilebilir |
| 12 | Flash dolu | Dosya sistemini doldur | Yazma hatası raporlanıyor, sistem çalışıyor |
| 13 | Task kilitlenmesi | Kasıtlı sonsuz döngü | WDT reset, neden kaydediliyor |
| 14 | Heartbeat kaybı | Task'ı durdur | Aktüatörler güvenli duruma, mod değişiyor |
| 15 | maxRunTime aşımı | Kısa maxRunTime ayarla | Pompa kapanıyor, WARNING |
| 16 | Geçersiz API isteği | Aralık dışı değer gönder | Reddediliyor, config değişmiyor |
| 17 | Güç kesintisi | Beslemeyi kes | Boot'ta röleler kapalı, config korunuyor |
| 18 | Yazma sırasında güç kesintisi | Config yazarken kes | Config bozulmuyor veya varsayılana düşüp loglanıyor |

## Implementation Notes

- Her test **gerçek donanımda** yapılmalı. Simülasyon veya kod incelemesi yeterli değildir —
  bu, güvenlik iddialarının kanıtıdır.
- Testler tekrarlanabilir olmalı; prosedür yazılı olmalı ki firmware değişikliğinden sonra
  tekrar çalıştırılabilsin.
- Bir testin başarısız olması durumunda düzeltme **bu task kapsamındadır** (entegrasyon
  hatası sayılır), ancak yeni özellik eklemek kapsam dışıdır.
- §16.4'teki yasak davranışların hiçbirinin kodda kalmadığı doğrulanmalı:
  `while(true)`, erken `return`, sessiz yutma, yalnızca `Serial.println` raporlama,
  kullanıcıya gösterilmeyen hata.
- Özellikle **3, 4, 14, 17** numaralı testler güvenlik iddialarının doğrudan kanıtıdır ve
  kesinlikle atlanamaz.
- Test sırasında pompanın zarar görmemesi için kuru çalışma testi kısa süreli ve gözetimli
  yapılmalı.

## Files

- `docs/FAILURE_TEST_REPORT.md` (yeni — 18 senaryonun sonuçları)
- Bulunan hataların düzeltmeleri (dosyalar bulgulara bağlı)

## Acceptance Criteria

- [ ] 18 senaryonun tamamı gerçek donanımda çalıştırıldı
- [ ] Her senaryonun sonucu belgelendi
- [ ] Hiçbir senaryoda sistem durmuyor veya kilitlenmiyor
- [ ] Güvenlik senaryolarında (3, 4, 5, 14, 15) pompa korunuyor
- [ ] Wi-Fi arızasında otomasyon ve güvenlik etkilenmiyor
- [ ] Storage/OLED arızasında sistem DEGRADED modda çalışıyor
- [ ] §16.4'teki yasak davranışların kodda olmadığı doğrulandı
- [ ] Bulunan sapmalar düzeltildi
- [ ] Test prosedürü tekrarlanabilir şekilde yazıldı
- [ ] Kapsam dışı bulgular `ISSUES.md`'ye kaydedildi

## Test Plan

- [ ] Yukarıdaki 18 senaryonun her biri (test planının kendisi bu matristir)
- [ ] Her senaryodan sonra sistem normale dönebiliyor
- [ ] Aynı senaryolar arka arkaya tekrarlandığında tutarlı davranış
- [ ] Testler sırasında hiçbir aktüatör beklenmedik şekilde çalışmıyor

## Review Checklist

- [ ] Architecture'a uygun mu? (§16.3, §16.4)
- [ ] Gereksiz abstraction var mı?
- [ ] Blocking işlem var mı?
- [ ] Shared state güvenli mi? — arıza koşullarında
- [ ] Memory problemi var mı? — arıza sonrası sızıntı
- [ ] Error handling var mı? — **bu task'ın tamamı**
- [ ] ESP32 resource kullanımı uygun mu?
- [ ] Task sorumluluğu doğru mu?
- [ ] Eski kod gereksiz şekilde kopyalanmış mı?

## Definition of Done

Ortak DoD + **18 senaryonun tamamı donanımda kanıtlandı ve raporlandı** + güvenlik
senaryolarında pompanın korunduğu gösterildi.

---

# STEP 1 — DESIGN RECORD + STEP 3 — REVIEW RECORD

**Tarih:** 2026-08-31

## Bu task'ın özü DONANIM GEREKTİRİYOR

TASK-061 "her arıza senaryosunun **kasıtlı olarak üretilmesi**"ni istiyor:
sensör kablosunu çıkarmak, hazneyi boşaltmak, Wi-Fi'yi kapatmak, güç
kesmek. Bunların hiçbiri kod tarafında yapılamaz.

Yapılabilen ve **yapılan**: §16.3 matrisinin satır satır **kod
denetimi** — her arıza için bir yol var mı, yoksa yazıldı mı.

## Denetim sonucu

Dokuz satırın **sekizi** uygulanmıştı. Biri **hiç uygulanmamıştı**:

> **Heap kritik seviyede** | Periyodik izleme | Telemetri hızı düşürülür,
> geçmiş yazımı duraklatılır | Uyarı

`SYS_LOW_HEAP` yalnızca tahsis hatasında kullanılıyordu; periyodik izleme
ve degradasyon yoktu. Bu turda eklendi:

```text
core::LOW_HEAP_BYTES = 32 768   ← core/ icinde: her katman okur, bagimlilik yok

SystemSupervisor  → periyodik olcum, esik alti SYS_LOW_HEAP
WsProtocol::tick  → telemetri araligi ×4
StorageService    → gecmis yazimi DURAKLATILIR
```

Eşiğin `core/` içinde olması bilinçli: `domain/` izliyor, `services/` ve
`interfaces/` tepki veriyor ve **hiçbiri diğerine bağımlı olmuyor**.

## Ayrıca düzeltilen iki sessiz arıza

Bunlar §16.4'ün ("yasaklanan hata davranışları") ihlaliydi — ikisi de
kullanıcıya **başarı bildirip hiçbir şey yapmıyordu**:

1. **Config yazılmıyordu** (TASK-059'da bulundu): "Kaydedildi" mesajı,
   yeniden başlatmada kayıp ayarlar.
2. **`FACTORY_RESET` yok sayılıyordu** (bu turda): onay isteniyor,
   `{"ok":true}` dönüyor, hiçbir şey olmuyordu.

## YAPILMAYAN — arıza enjeksiyonunun kendisi

- [ ] Sensör kablosu çıkarma → kalite `FAULT` → pompa kilidi
- [ ] Hazne boşaltma → seviye kilidi → pompa reddi
- [ ] Kuru çalışma → akış doğrulama → mandal
- [ ] Wi-Fi kapatma → `BACKOFF` → AP fallback → güvenlik etkilenmemesi
- [ ] Dosya sistemi bozma → degraded çalışma
- [ ] OLED sökme → sistemin tam çalışması
- [ ] Task askıya alma → heartbeat kaybı → `forceAllOff()`
- [ ] Güç kesme → mandal kalıcılığı → boot raporu

**TASK-061: KOD DENETİMİ TAMAMLANDI, ARIZA ENJEKSİYONU YAPILMADI.**
Bu task donanım olmadan kapatılamaz.
