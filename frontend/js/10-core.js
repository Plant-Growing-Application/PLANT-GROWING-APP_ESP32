/* SALIXUS — çekirdek: sözlükler, tema, durum deposu, bağlantı, komut yolu
 *
 * ══ MUTLAK KURAL: İYİMSER GÜNCELLEME YASAK ════════════════════════════════
 *   Tıkla → kart "BEKLİYOR" olur → komut gider → cihazdan ack ve gerçek
 *   telemetri gelene kadar kart GERÇEK röle durumunu gösterir.
 *   Tüm çizim YALNIZCA `store.state` üzerinden yapılır (ARCHITECTURE P5).
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * ══ İKİ SEVİYELİ ARAYÜZ ═══════════════════════════════════════════════════
 * Dört hedef: Bahçem · Bitkim · Kontrol · Ayarlar. Teknik ekranlar Ayarlar
 * altındaki alt sayfalarda ve `data-expert` işaretlidir. Hiçbir işlev
 * silinmedi — yalnızca varsayılan olarak görünmüyor.
 */

// ── DOM yardımcıları ───────────────────────────────────────────────────────

function el(id) {
  const n = document.getElementById(id);
  if (!n) console.error('[ui] DOM ogesi bulunamadi:', id);
  return n;
}

/// Sessiz arama — öge OLMAYABİLİR. `el()` her başarısızlığı konsola yazar ki
/// bir kimliği yeniden adlandırıp güncellemeyi unutmak sessizce geçmesin;
/// isteğe bağlı ögelerde o gürültü yanlış alarma dönüşür.
const el0 = (id) => document.getElementById(id);

const show = (n, on) => n && n.classList.toggle('hidden', !on);
const txt  = (n, s) => { if (n) n.textContent = s; };

/// HTML'e gömülen her cihaz metni buradan geçer: SSID ve zaman dilimi gibi
/// alanları kullanıcı yazar.
function esc(s) {
  return String(s == null ? '' : s).replace(/[&<>"']/g, (ch) => (
    { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]
  ));
}

/// Ağ adı gibi KULLANICININ YAZDIĞI metinlerin ekranda kapladığı yeri sınırlar.
///
/// ══ NEDEN GEREKLİ ═════════════════════════════════════════════════════════
/// Wi-Fi standardı SSID'ye 32 karaktere kadar izin verir ve boşluk şartı
/// yoktur: "MisafirAgi_2.4GHz_UstKat_Yeni" tek kelimedir. Izgara hücresi
/// böyle bir kelimeyi bölemez, hücre genişler ve YANINDAKİ sütunları
/// ezer — kart dağılır.
///
/// CSS üç noktası (`text-overflow`) dar ekranda bunu zaten yapar; buradaki
/// sınır GENİŞ ekranda da geçerlidir ve tek bir ağ adının kartın yarısını
/// yutmasını engeller. Tam ad `title` ile erişilebilir kalır.
///
/// DİKKAT: bu YALNIZCA GÖSTERİMDİR. Cihaza gönderilen SSID hiçbir yerde
/// kısaltılmaz — kırpılmış bir ağ adıyla bağlanma denemesi HER ZAMAN
/// başarısız olur.
const NAME_MAX_CHARS = 22;

function clipText(s, max = NAME_MAX_CHARS) {
  const v = String(s == null ? '' : s);
  return v.length > max ? v.slice(0, max - 1) + '…' : v;
}

function fmtUptime(ms) {
  if (typeof ms !== 'number' || isNaN(ms)) return '—';
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d > 0) return `${d}g ${h}s ${m}d`;
  if (h > 0) return `${h}s ${m}d`;
  return `${m}d ${s % 60}sn`;
}

const trimNum = (v) => String(Math.round(v * 100) / 100);

/// Milisaniyeyi insan ölçüsüne çevirir. Cihaz ms taşır; kullanıcı "900000"
/// gördüğünde bunun 15 dakika olduğunu kafadan hesaplamak zorunda kalmamalı.
function fmtDur(ms) {
  const n = Number(ms);
  if (!isFinite(n)) return '—';
  if (n < 1000) return n + ' ms';
  if (n < 60000) return trimNum(n / 1000) + ' sn';
  if (n < 3600000) return trimNum(n / 60000) + ' dk';
  return trimNum(n / 3600000) + ' sa';
}

const fmtSec = (s) => fmtDur((+s || 0) * 1000);

const pad2 = (n) => String(n).padStart(2, '0');
const minToTime = (m) => pad2(Math.floor((+m || 0) / 60)) + ':' + pad2((+m || 0) % 60);

function timeToMin(s) {
  const parts = String(s || '').split(':');
  const h = +parts[0], m = +parts[1];
  if (!isFinite(h) || !isFinite(m)) return 0;
  return ((h * 60 + m) % 1440 + 1440) % 1440;
}

// ── Sözlükler ──────────────────────────────────────────────────────────────
//
// TEK YER: cihazdan gelen her kısa ad (`waterPump`, `airTemp`, `0x0603`)
// buradan Türkçeye çevrilir. Jargonun arayüze sızmaması bu tablolara bağlı.

const SENSOR_META = {
  waterTemp: { label: 'Su Sıcaklığı', short: 'Su Sıc.', icon: '🌡', unit: '°C', digits: 1, min: 5, max: 40 },
  flow:      { label: 'Su Akışı', short: 'Akış', icon: '🚿', unit: 'L/dk', digits: 2, min: 0, max: 8 },
  ph:        { label: 'pH (Asitlik)', short: 'pH', icon: '🧪', unit: '', digits: 2, min: 4, max: 9 },
  ec:        { label: 'Besin Yoğunluğu (EC)', short: 'Besin', icon: '🧂', unit: 'mS/cm', digits: 2, min: 0, max: 4 },
  level:     { label: 'Su Seviyesi', short: 'Su', icon: '💧', unit: '', digits: 0, min: 0, max: 2 },
  humidity:  { label: 'Ortam Nemi', short: 'Nem', icon: '💨', unit: '%', digits: 0, min: 0, max: 100 },
  airTemp:   { label: 'Hava Sıcaklığı', short: 'Hava', icon: '🌤', unit: '°C', digits: 1, min: -10, max: 50 },
  light:     { label: 'Işık', short: 'Işık', icon: '☀️', unit: 'lx', digits: 0, min: 0, max: 50000 },
};

/// Panelin "Bugün" satırında gösterilecek ölçümler ve SIRALARI.
///
/// Hepsini göstermiyoruz: sekiz kart bir özet değil, bir duvardır. Buradaki
/// dördü kullanıcının günlük olarak müdahale ettiği şeylerdir; kalanlar
/// Bitkim ekranında tam kartlarıyla durur. Takılı olmayan bir sensör
/// listeden DÜŞER ve yerine sıradaki gelir — "Yok" yazan bir kutu, bilgi
/// değil gürültüdür.
const VITAL_ORDER = ['level', 'waterTemp', 'ph', 'ec', 'airTemp', 'humidity', 'light', 'flow'];
const VITAL_COUNT = 4;

/// Hedef bandı OLAN sensörler. Ürün profili yalnızca bunlar için "iyi/kötü"
/// söyleyebilir; akış ve seviye ürüne değil DONANIMA bağlıdır.
const CROP_TARGET_KEY = {
  ph: 'ph', ec: 'ec', waterTemp: 'waterTemp', airTemp: 'airTemp', humidity: 'humidity',
};

const LEVEL_TEXT = ['KRİTİK', 'DÜŞÜK', 'YETERLİ'];
const LEVEL_PERCENT = [20, 55, 100];

const ACT_META = {
  waterPump:    { name: 'Su Pompası', desc: 'Besleme ve sirkülasyon', icon: '💧' },
  airPump:      { name: 'Hava Pompası', desc: 'Kök havalandırma', icon: '🫧' },
  growLight:    { name: 'Büyütme Işığı', desc: 'Günlük ışık süresi', icon: '💡' },
  heater:       { name: 'Isıtıcı', desc: 'Besin çözeltisi sıcaklığı', icon: '🔥' },
  nutrientPump: { name: 'Besin Pompası', desc: 'Gübre dozajı', icon: '🧪' },
};

/// Aktüatörün MEVCUT durumunu KİM belirledi (`core::ControlSource`).
///
/// Bu alan her telemetri paketinde geliyordu ve arayüz onu HİÇ okumuyordu.
/// Oysa kullanıcının en sık sorduğu soru bu: "pompayı ben mi açtım, program
/// mı?" Cevap pakette hazır duruyordu.
const SRC_TEXT = {
  0: '',                       // NONE — henüz kimse sürmedi
  1: 'Otomatik program',
  2: 'Sizin komutunuz',
  3: 'Güvenlik sistemi',
};

/// Aynı bilgi, rozete sığan hâli. "OTOMATİK PROGRAM" dar bir satırda cihaz
/// adını iki satıra kırıyordu; rozette bağlam zaten belli.
const SRC_SHORT = { 0: '', 1: 'Otomatik', 2: 'Elle', 3: 'Güvenlik' };

/// Ölçüm durumunun TEK SÖZCÜKLÜK karşılığı — kompakt kartlar için.
/// Uzun cümle `sensorStatus().text` içinde kalır.
const LEVEL_WORD = {
  ok: 'İdeal', warn: 'Dikkat', bad: 'Sorun', off: 'Yok', unknown: '—',
};

/// Ayarlarda "bu cihaz bağlı mı" diye sorulabilecek olanlar.
/// Su ve hava pompası listede YOK: onlar sistemin temelidir ve kapatmak
/// sulamayı tamamen durdurur — o karar Gelişmiş ekranına aittir.
const OPTIONAL_ACTUATORS = ['growLight', 'heater', 'nutrientPump'];

/// Ayarlarda "bu sensör takılı mı" diye sorulabilecek olanlar.
///
/// `level` ve `flow` listede YOK: ikisi güvenlik zincirinin girdisidir
/// (kuru çalışma tespiti, pompa kilidi) ve varsayılan olarak açıktır.
/// Cihaz `requireLevelSensor` açıkken seviye sensörünü kapatmayı zaten
/// reddeder — arayüzde de sunmuyoruz.
const OPTIONAL_SENSORS = ['ph', 'ec', 'humidity', 'airTemp', 'light'];

/// Sensör kartlarında gösterilecek kısa açıklama.
const SENSOR_DESC = {
  ph:       'Asitlik probu (analog)',
  ec:       'Besin yoğunluğu probu (analog)',
  humidity: 'Ortam nemi — AHT20 (I2C)',
  airTemp:  'Hava sıcaklığı — AHT20 (I2C)',
  light:    'Aydınlık düzeyi — BH1750 (I2C)',
  waterTemp:'Besin çözeltisi sıcaklığı (NTC)',
  flow:     'Su akışı — darbe sayıcı',
  level:    'Şamandıra (güvenlik)',
};

const MODE_TEXT = {
  booting: 'Başlatılıyor', running: 'Çalışıyor', degraded: 'Kısıtlı Mod',
  safe: 'Güvenli Mod', emergency: 'ACİL DURUM',
};

const NET_TEXT = {
  boot: 'Başlatılıyor', apOnly: 'Kurulum (AP Modu)', connecting: 'Ağa Bağlanıyor…',
  connected: 'Bağlı', backoff: 'Yeniden Denenecek', apFallback: 'AP + Yeniden Deneme',
};

const STAGE_TEXT = {
  seedling: 'Fide', vegetative: 'Gelişme', flowering: 'Çiçeklenme', fruiting: 'Meyve',
};

const INTENSITY_TEXT = { sparse: 'Az', normal: 'Normal', abundant: 'Bol' };

const DIFFICULTY_TEXT = { 1: 'Kolay', 2: 'Orta', 3: 'Zor' };

const ERR_TEXT = {
  0x0101: 'Sistem açılış aşaması başarısız oldu',
  0x0102: 'Önceki oturum watchdog ile sonlandı',
  0x0103: 'Görev oluşturulamadı',
  0x0104: 'Görev yanıt vermiyor',
  0x0105: 'Bellek kritik seviyede',
  0x0201: 'Kayıtlı ayar bulunamadı',
  0x0202: 'Ayar kaydı bozuk',
  0x0203: 'Firmware sürümü geriye alınmış',
  0x0204: 'Ayar doğrulaması başarısız',
  0x0301: 'Ayar deposu açılamadı',
  0x0302: 'Dosya sistemi bağlanamadı',
  0x0303: 'Belleğe yazma başarısız',
  0x0304: 'Depolama alanı dolu',
  0x0305: 'Bozuk kayıt tespit edildi',
  0x0401: 'Sensör takılı değil',
  0x0402: 'Sensör kablosu kopuk',
  0x0403: 'Sensör kablosunda kısa devre',
  0x0404: 'Sensör değeri beklenen aralığın dışında',
  0x0405: 'Sensör değeri değişmiyor (bayat)',
  0x0406: 'Gerçekçi olmayan ani değişim',
  0x0407: 'Akış sensöründen sinyal gelmiyor',
  0x0501: 'Henüz asgari çalışma süresi dolmadı',
  0x0502: 'Bekleme süresi dolmadı',
  0x0503: 'Azami çalışma süresi aşıldı, otomatik kapatıldı',
  0x0504: 'İstenen durum ile gerçek röle durumu uyuşmuyor',
  0x0601: 'Su seviyesi yetersiz — depoya su ekleyin',
  0x0602: 'Su seviyesi sensörü okunamıyor (güvenlik kilidi)',
  0x0603: 'Kuru çalışma tespit edildi',
  0x0604: 'Su akışı doğrulanamadı',
  0x0605: 'Acil durum kilidi aktif',
  0x0606: 'Güvenlik kilidi devrede',
  0x0701: 'Kayıtlı Wi-Fi ağı yok',
  0x0702: 'Wi-Fi parolası hatalı',
  0x0703: 'Wi-Fi ağı bulunamadı',
  0x0704: 'Ağ bağlantısı koptu',
  0x0705: 'Ağ bağlantısı zaman aşımına uğradı',
  0x0706: 'Ağ taraması başarısız',
  0x0707: 'Statik IP ayarı geçersiz',
  0x0801: 'Cihaz saati ayarlı değil',
  0x0802: 'Saat senkronizasyonu başarısız',
  0x0901: 'Yetkisiz istek',
  0x0902: 'Geçersiz istek',
  0x0903: 'Cihaz meşgul',
  0x0904: 'İstek çok büyük',
  0x0A01: 'Ekran yanıt vermiyor',
  0x0A02: 'Giriş kuyruğu dolu',
};

const errText = (c) => (c ? (ERR_TEXT[c] || ('Hata 0x' + c.toString(16).toUpperCase())) : '');

// ── Güvenlik kilidi bitleri ────────────────────────────────────────────────
//
// `domain/models/SafetyState.h::Interlock` ile AYNI değerler. Cihaz bu maskeyi
// her telemetri paketinde yayınlıyordu ama arayüz onu HİÇ ÇÖZMÜYORDU —
// kullanıcı "Acil Durumu Temizle"ye basıyor, hiçbir şey olmuyor ve nedenini
// göremiyordu (TASK-074).
const ILK = {
  EMERGENCY:     1 << 0,
  LEVEL_LOW:     1 << 1,
  LEVEL_FAULT:   1 << 2,
  DRY_RUN:       1 << 3,
  MAX_RUNTIME:   1 << 4,
};

/// Bir kilidin ne anlama geldiği ve kullanıcının NE YAPMASI gerektiği.
const ILK_TEXT = {
  [ILK.EMERGENCY]:   'Acil durum kilidi mandallanmış',
  [ILK.LEVEL_LOW]:   'Su seviyesi yetersiz — depoya su ekleyin',
  [ILK.LEVEL_FAULT]: 'Su seviyesi sensörü okunamıyor — şamandıra kablolarını kontrol edin',
  [ILK.DRY_RUN]:     'Kuru çalışma tespit edildi',
  [ILK.MAX_RUNTIME]: 'Azami çalışma süresi tekrar tekrar aşıldı',
};

/// Maskedeki kilitleri okunur cümlelere çevirir.
function interlockList(mask) {
  const out = [];
  Object.keys(ILK).forEach((k) => {
    const bit = ILK[k];
    if ((mask & bit) !== 0) out.push(ILK_TEXT[bit]);
  });
  return out;
}

/// Acil durumun TEMİZLENMESİNİ engelleyen kilitler.
///
/// Cihaz tarafında `SafetyMonitor::acknowledge()` YALNIZCA canlı su seviyesi
/// koşulunu kontrol eder (`evaluateLevel`). Kuru çalışma ve süre aşımı
/// mandalları temizlenecek olanlardır, engel değildir — bu yüzden listede
/// yoklar. Burada farklı bir küme kullanmak, kullanıcıya cihazın uygulamadığı
/// bir kural anlatmak olurdu.
const CLEAR_BLOCKERS = ILK.LEVEL_LOW | ILK.LEVEL_FAULT;

function qualityText(q) {
  return {
    ok: 'Ölçüldü', stale: 'Değişmiyor', outOfRange: 'Aralık dışı',
    fault: 'Arızalı', notPresent: 'Takılı değil',
  }[q] || q;
}

// ── Durum deposu ───────────────────────────────────────────────────────────

const store = {
  token: null,
  state: null,        // cihazdan gelen son onaylı durum
  crop: null,         // /api/crop yanıtı (aktif ürün + hedef bantlar)
  catalog: null,      // /api/crops yanıtı (bir kez alınır)
  config: null,       // /api/config yanıtı
  linked: false,
  lastStateAt: 0,     // son `state` paketinin alındığı an (bayatlık sayacı)
  pending: new Map(), // reqId -> { target, timer }
  rejected: null,
  quick: null,        // ana sayfadaki hızlı sulama: { want, at } — sonucu bildirmek için
  lastBlock: {},      // aktüatör -> { code, at }: cihazın bildirdiği son engel nedeni
  seq: 1,
  historyData: null,
  activeChartSensor: 'ph',
  rules: [],          // kural düzenleyicinin YEREL kopyası
  handover: null,     // kurulum bitti: { ssid, ip } — cihaz yeniden başlıyor
};

// ── Tema ───────────────────────────────────────────────────────────────────
//
// ══ NEDEN localStorage ═════════════════════════════════════════════════════
// Tema bir GÖRÜNTÜLEME tercihidir, cihaz durumu değil. Cihaza yazmak üç şeyi
// bozardı: (1) aynı cihaza bakan iki kişi birbirinin temasını değiştirirdi,
// (2) NVS'e her dokunuş yazma döngüsü harcar, (3) AP modunda henüz yetki
// yokken tema uygulanamazdı. Tarayıcı tarafında saklamak doğru katman.
//
// ══ ÜÇ DEĞER, İKİ SONUÇ ════════════════════════════════════════════════════
// Tercih `light | dark | auto`; UYGULANAN tema her zaman `light` veya `dark`.
// `auto` işletim sistemini izler ve sistem gece moduna geçtiğinde canlı
// olarak değişir.
//
// İlk damgalama `index.html` içindeki satır içi betiktedir — ilk boyamadan
// önce çalışır, bu yüzden yanlış temayla bir kare bile görünmez.

const THEME_KEY = 'salixus.theme';
const THEME_TEXT = { light: 'Açık', dark: 'Koyu', auto: 'Sistem' };

/// Kaydedilmiş tercih: `light` · `dark` · `auto` (varsayılan).
function themePref() {
  try {
    const v = localStorage.getItem(THEME_KEY);
    return (v === 'light' || v === 'dark') ? v : 'auto';
  } catch (e) { return 'auto'; }   // gizli sekme / depolama kapalı
}

const prefersDark = () =>
  !!(window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);

/// Tercihi UYGULANAN temaya indirger.
const resolvedTheme = () => {
  const p = themePref();
  return p === 'auto' ? (prefersDark() ? 'dark' : 'light') : p;
};

/// `<html data-theme>`, tarayıcı çubuğu rengi ve ilgili kontroller.
///
/// Grafik yeniden çizilir: `<canvas>` bir CSS ağacı değildir, tema
/// değişince kendiliğinden renk değiştirmez — eski renklerle kalırdı.
function applyTheme() {
  const t = resolvedTheme();
  document.documentElement.setAttribute('data-theme', t);

  const meta = document.querySelector('meta[name="theme-color"]:not([media])');
  if (meta) meta.setAttribute('content', t === 'dark' ? '#1a2028' : '#e9edf3');

  paintThemeControls();
  if (typeof drawChart === 'function' && store.historyData) drawChart();
}

function setTheme(pref) {
  try { localStorage.setItem(THEME_KEY, pref); } catch (e) { /* yoksay */ }
  applyTheme();
}

/// Başlıktaki düğme ile Görünüm ekranındaki seçiciyi eşitler.
function paintThemeControls() {
  const pref = themePref();
  const dark = resolvedTheme() === 'dark';

  const icon = el0('themeIcon');
  if (icon) {
    // Düğme SONRAKİ durumu gösterir: koyudayken güneş, açıktayken ay.
    icon.innerHTML = dark
      ? '<circle cx="12" cy="12" r="4"></circle><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"></path>'
      : '<path d="M20 14.5A8.5 8.5 0 1 1 9.5 4a6.6 6.6 0 0 0 10.5 10.5z"></path>';
  }
  const btn = el0('themeBtn');
  if (btn) {
    const next = dark ? 'Açık' : 'Koyu';
    const label = next + ' temaya geç';
    btn.title = pref === 'auto'
      ? label + ' (şu an: sistem tercihi)'
      : label;
    btn.setAttribute('aria-label', label);
  }

  document.querySelectorAll('[data-theme-pick]').forEach((b) => {
    b.classList.toggle('active', b.dataset.themePick === pref);
  });
}

/// Başlıktaki düğme: GÖRÜNEN temayı ters çevirir. Her tıklama bir değişim.
///
/// ── NEDEN ÜÇLÜ DÖNGÜ DEĞİL ──────────────────────────────────────────────────
/// Önce `Açık → Koyu → Sistem → Açık` diye dönüyordu. Sistem tercihi koyu
/// olan bir kullanıcıda "Koyu" ve "Sistem" adımları AYNI görünüyordu: ikinci
/// tıklamada ekranda hiçbir şey değişmiyor, açığa dönmek için üçüncü tıklama
/// gerekiyordu. Bir aç/kapa düğmesinin bazen hiçbir şey yapmaması, düğmenin
/// bozuk olduğu anlamına gelir.
///
/// Simge düğmesi artık yalnızca iki uç arasında gidip geliyor; üç seçenekli
/// tercih (Açık · Koyu · Sistem) Ayarlar → Görünüm ekranında, adları
/// yazılı ve hangisinin seçili olduğu görünür hâlde duruyor.
function toggleTheme() {
  setTheme(resolvedTheme() === 'dark' ? 'light' : 'dark');
}

// Sistem gece moduna geçerse `auto` seçiliyken CANLI izle.
if (window.matchMedia) {
  const mq = window.matchMedia('(prefers-color-scheme: dark)');
  const onChange = () => { if (themePref() === 'auto') applyTheme(); };
  if (mq.addEventListener) mq.addEventListener('change', onChange);
  else if (mq.addListener) mq.addListener(onChange);   // eski WebKit
}

// ── Basit / Uzman modu ─────────────────────────────────────────────────────

const EXPERT_KEY = 'salixus.expert';

function isExpert() {
  try { return localStorage.getItem(EXPERT_KEY) === '1'; }
  catch (e) { return false; }   // gizli sekme / depolama kapalı
}

function setExpert(on) {
  try { localStorage.setItem(EXPERT_KEY, on ? '1' : '0'); } catch (e) { /* yoksay */ }
  applyExpertMode();
}

/// `data-expert` işaretli her şeyi moda göre gösterir/gizler.
///
/// Artık hiçbir SEKME uzman modda değil — teknik bölümler Ayarlar altındaki
/// alt sayfaların içinde. Bu yüzden "gizlenen sekmedeyken boş ekranda kalma"
/// durumu yapısal olarak imkânsız; eski geri dönüş kodu gereksizleşti.
function applyExpertMode() {
  const on = isExpert();
  document.querySelectorAll('[data-expert]').forEach((n) => {
    n.classList.toggle('hidden', !on);
  });
  const t = el0('expertToggle');
  if (t) t.checked = on;
}

// ── WebSocket ──────────────────────────────────────────────────────────────

let ws = null;
let backoffStep = 0;
const BACKOFF = [1000, 2000, 4000, 8000, 15000];
let reconnectTimer = null;

function setLinked(on, reason) {
  store.linked = on;

  // ── BEKLENEN KOPMA ALARM DEĞİLDİR ────────────────────────────────────────
  // Kurulum devir tesliminde bağlantının kesilmesi PLANLIDIR: cihaz yeniden
  // başlıyor. Kırmızı "bağlantı kesildi" bandı burada yalan söyler ve
  // kullanıcıya tam da her şey yolundayken bir şeyin bozulduğunu düşündürür.
  // Durum rozeti yine de doğruyu söyler: canlı bağlantı yok.
  if (store.handover) {
    document.body.classList.remove('stale');
    show(el('linkBar'), false);
    const p = el0('connPill');
    const l = el0('connLabel');
    if (p && l) { p.classList.toggle('offline', !on); txt(l, on ? 'Canlı' : 'Kopuk'); }
    return;
  }

  // ── BANT YALNIZCA OTURUM AÇIKKEN ─────────────────────────────────────────
  // Giriş ekranında henüz bir bağlantı BEKLENMİYOR. Eskiden açılışta
  // `setLinked(false)` çağrılıyor ve kullanıcı daha parolasını yazmadan
  // kırmızı bir "bağlantı kesildi — gösterilen veriler eski" bandı
  // görüyordu: doğru olmayan, üstelik korkutan bir uyarı.
  const inApp = !!store.token;
  document.body.classList.toggle('stale', inApp && !on);
  show(el('linkBar'), inApp && !on);
  if (!on) txt(el('linkText'), reason || 'Bağlantı kesildi — gösterilen veriler eski');

  const cp = el0('connPill');
  const cl = el0('connLabel');
  if (cp && cl) {
    cp.classList.toggle('offline', !on);
    txt(cl, on ? 'Canlı' : 'Kopuk');
  }

  // ── KOPUNCA KONTROLLER KİLİTLENİR ────────────────────────────────────────
  // Güç düğmelerinin `disabled` durumu `store.linked`'e bakılarak ÇİZİM
  // sırasında belirlenir — ama bağlantı koptuğunda çizimi tetikleyecek bir
  // `state` paketi de gelmez. Sonuç: bağlantı yokken düğmeler etkin görünür,
  // kullanıcı basar ve komut hiçbir yere gitmez.
  //
  // `sendCmd` bunu zaten yakalıyor ("Bağlantı yok" der), ama basılabilir
  // görünen bir düğme yalan söyler. Durum değiştiğinde yeniden çiziyoruz.
  if (store.state) { render(); }
}

// ── Kurulum devir teslimi ──────────────────────────────────────────────────
//
// ══ NEDEN AYRI BİR EKRAN ═══════════════════════════════════════════════════
// Kurulumun son adımı bir AĞ DEĞİŞİKLİĞİDİR: telefon `Sera-XXXX` ağında,
// cihaz ise artık ev ağında. Kullanıcı telefonunu kendi ağına almadan cihaza
// BİR DAHA ulaşamaz ve yeni adresi bilmiyorsa aramanın hiçbir yolu yoktur.
//
// Bu yüzden son paketteki adres yakalanır ve ekranda TUTULUR: soket kopar,
// yeniden bağlanma denemeleri boşa gider, ama ekrandaki adres durur.
function enterHandover(net) {
  if (store.handover) { return; }   // ilk paket geçerli; tekrarları yok say

  store.handover = { ssid: net.ssid || '', ip: net.ip || '' };

  // Yeniden bağlanma zincirini durdur: cihaz kapanıyor, denemek boşuna ve
  // her deneme ekranda bir hata daha üretir.
  clearTimeout(reconnectTimer);
  try { if (ws) { ws.onclose = null; ws.onerror = null; ws.close(); } }
  catch (e) { /* zaten kapalı */ }
  ws = null;
  setLinked(false);

  renderHandover();
}

/// Devir teslim ekranını çizer ve gösterir.
function renderHandover() {
  const h = store.handover;
  const box = el0('handover');
  if (!h || !box) { return; }

  const addr = h.ip && h.ip !== '0.0.0.0' ? h.ip : '';

  const nameEl = el0('hoSsid');
  txt(nameEl, clipText(h.ssid) || 'ağınıza');
  if (nameEl) { nameEl.title = h.ssid || ''; }
  const link = el0('hoLink');
  if (link) {
    // Adres alınamadıysa TAHMİN YOK: yanlış bir adres, adres olmamasından
    // beterdir — kullanıcı ona güvenip cihazın bozulduğunu sanır.
    txt(link, addr ? 'http://' + addr : 'Cihaz ekranında yazan adres');
    if (addr) { link.href = 'http://' + addr; link.classList.remove('dead'); }
    else      { link.removeAttribute('href'); link.classList.add('dead'); }
  }
  show(box, true);
}

function connect() {
  if (!store.token) return;
  clearTimeout(reconnectTimer);

  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(`${proto}://${location.host}/ws?token=${encodeURIComponent(store.token)}`);

  ws.onopen = () => { backoffStep = 0; setLinked(true); };

  ws.onmessage = (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); }
    catch (e) { console.error('[ws] cozumlenemeyen mesaj:', e); return; }
    if (msg.type === 'state') onState(msg);
    else if (msg.type === 'ack') onAck(msg);
  };

  ws.onclose = (ev) => {
    setLinked(false, ev.code === 1008
      ? 'Oturum geçersiz — yeniden giriş yapın'
      : 'Bağlantı kesildi — yeniden bağlanılıyor…');
    if (ev.code === 1008) {
      store.token = null;
      showLogin('Oturum geçersiz — yeniden giriş yapın.', 'err');
      return;
    }
    scheduleReconnect();
  };

  ws.onerror = () => { try { ws.close(); } catch (e) { /* zaten kapalı */ } };
}

function scheduleReconnect() {
  const wait = BACKOFF[Math.min(backoffStep, BACKOFF.length - 1)];
  backoffStep++;
  reconnectTimer = setTimeout(connect, wait);
}

// ── TELEMETRİ BEKÇİSİ ──────────────────────────────────────────────────────
//
// ══ NEDEN `onclose` YETMEZ ═════════════════════════════════════════════════
// Bir soket YARI ÖLEBİLİR: erişim noktası kapanır, cihazın fişi çekilir ya da
// ağ değişir — ve TCP kapanış el sıkışması hiç gelmez. Tarayıcı soketi
// dakikalarca `OPEN` (veya `CLOSING`) sayar, `onclose` tetiklenmez.
//
// Sonuç, bu proje için kabul edilemez olan şeydir: arayüz "Canlı" yazar,
// ölçümler DONMUŞTUR ve kullanıcı ekrandaki sayıyı ŞU ANKİ gerçek durum
// sanar. Pompanın "kapalı" göründüğü ekran on dakika öncesine ait olabilir.
//
// Bu yüzden bağlantının canlılığı soketin iddiasına değil, VERİ AKIŞINA
// bakılarak ölçülür: cihaz `telemetryIntervalMs` (varsayılan 1 sn) periyoduyla
// ve her kritik değişimde yayın yapar. Bu kadar süre hiç paket gelmediyse
// bağlantı fiilen ölüdür, soket ne derse desin.
//
// Eşik cömert: yavaş bir ağda birkaç paketin gecikmesi yanlış alarm
// üretmemeli. 15 saniye, en yavaş telemetri periyodunun (60 sn'lik üst
// sınırın değil, gerçekçi ayarların) kat kat üstüdür.
const STALE_AFTER_MS = 15000;
const WATCHDOG_TICK_MS = 3000;

/// Soketi bırakır ve yeni bir bağlantı kurar. `onclose` devre dışı bırakılır:
/// aksi hâlde ölü soketin geç gelen kapanış olayı İKİNCİ bir yeniden bağlanma
/// zincirlerdi ve iki soket birden açılırdı.
function forceReconnect(reason) {
  setLinked(false, reason);
  try {
    if (ws) { ws.onclose = null; ws.onerror = null; ws.onmessage = null; ws.close(); }
  } catch (e) { /* zaten kapalı */ }
  ws = null;
  clearTimeout(reconnectTimer);
  backoffStep = 0;
  reconnectTimer = setTimeout(connect, 500);
}

setInterval(() => {
  // Devir teslimde cihaz KASITLI olarak kapanıyor: bekçinin işi yok.
  if (store.handover) { return; }
  if (!store.token || !store.linked || !store.lastStateAt) { return; }
  if (Date.now() - store.lastStateAt < STALE_AFTER_MS) { return; }
  forceReconnect('Cihazdan veri gelmiyor — bağlantı yenileniyor');
}, WATCHDOG_TICK_MS);

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState !== 'visible') { return; }

  if (!store.linked && store.token && !store.handover) {
    backoffStep = 0;
    connect();
  }

  // Tema, sekmeye DÖNÜNCE de yeniden çözülür. `matchMedia` olayı normalde
  // yeterlidir ama bazı tarayıcılar onu gizli sekmeye göndermez: kullanıcı
  // telefonunu gece moduna alıp geri döndüğünde arayüz eski temada kalırdı.
  // Maliyeti bir öznitelik karşılaştırması.
  if (themePref() === 'auto') { applyTheme(); }
});

function onState(msg) {
  if (store.state && msg.v < store.state.v) {
    console.warn('[ws] versiyon geriye gitti — cihaz yeniden baslamis');
    store.pending.forEach((p) => clearTimeout(p.timer));
    store.pending.clear();
  }

  // Cihaz ürün/dönem DEĞİŞTİRDİYSE (otomatik dönem ilerlemesi) hedef bantlar
  // bayatlamıştır; yeniden çekiyoruz. Aksi hâlde panel meyve dönemindeyken
  // çiçeklenme hedeflerine göre "iyi/kötü" derdi.
  //
  // İLK PAKETTE ÇEKMİYORUZ: `prev` yokken "değişti" saymak, girişte
  // `afterLogin()`'in zaten yaptığı işi ikinci kez yapmaktı. İlk yükleme
  // oturum açmanın işidir; buranın işi DEĞİŞİMİ yakalamak.
  const prev = store.state && store.state.crop;
  const now  = msg.crop;
  if (now && prev && (prev.key !== now.key || prev.stage !== now.stage)) {
    loadCrop();
  }

  // ── ENGEL NEDENLERİ ANLIKTIR, YAKALANMAZSA KAYBOLUR ─────────────────────
  //
  // `block` cihazın SON deneme sonucudur. Otomasyon motoru saniyede birkaç
  // kez değerlendirdiği için bir sonraki başarılı turda sıfırlanır: neden
  // ekranda bir kare görünüp yok olur.
  //
  // Kullanıcının sorusu ("pompaya bastım, neden çalışmadı?") ise o kare
  // geçtikten SONRA soruluyor. Cevabı, geldiği anda saklıyoruz.
  if (Array.isArray(msg.actuators)) {
    const now = Date.now();
    msg.actuators.forEach((a) => {
      if (a.block) { store.lastBlock[a.id] = { code: a.block, at: now }; }
    });
  }

  store.state = msg;
  store.lastStateAt = Date.now();

  // ── KURULUM DEVİR TESLİMİ (§8.4) ────────────────────────────────────────
  // Cihaz ev ağına bağlandı ve yeni ayarlarla yeniden başlıyor. Bu paket,
  // telefon HÂLÂ kurulum ağındayken gelen son pakettir: yeni adresi burada
  // söylemezsek kullanıcı cihazı bir daha bulamaz.
  if (msg.network && msg.network.setupReboot) { enterHandover(msg.network); }

  render();
}

// ── "Ne kadar eski?" sayacı ────────────────────────────────────────────────
//
// Bağlantı koptuğunda ekranda DURAN sayılar hâlâ okunabilir olmalı — ama
// kullanıcı onları ANLIK sanmamalı. "Bağlantı kesildi" tek başına bunu
// söylemez; "son güncelleme 3 dakika önce" söyler.

function staleAgeText() {
  if (!store.lastStateAt) return '';
  const s = Math.round((Date.now() - store.lastStateAt) / 1000);
  if (s < 5) return '';
  if (s < 60) return ` — son güncelleme ${s} sn önce`;
  if (s < 3600) return ` — son güncelleme ${Math.floor(s / 60)} dk önce`;
  return ` — son güncelleme ${Math.floor(s / 3600)} sa önce`;
}

setInterval(() => {
  if (store.linked || !store.token || store.handover) return;
  txt(el0('linkText'), 'Bağlantı kesildi, yeniden bağlanılıyor' + staleAgeText());
}, 1000);

function onAck(msg) {
  const p = store.pending.get(msg.reqId);
  if (!p) return;

  clearTimeout(p.timer);
  store.pending.delete(msg.reqId);

  if (msg.result !== 0 && msg.result !== 1) {
    const reason = errText(msg.reason) ||
      (msg.result === 7 ? 'Cihaz meşgul, tekrar deneyin' : 'Komut güvenlik kilidiyle reddedildi');
    store.rejected = { target: p.target, text: reason, at: Date.now() };
  }
  render();
}

// ── Komut yolu ─────────────────────────────────────────────────────────────

const PENDING_TIMEOUT_MS = 5000;

function sendCmd(target, action) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    store.rejected = { target, text: 'Bağlantı yok', at: Date.now() };
    render();
    return;
  }

  const reqId = 'r' + (store.seq++);
  const timer = setTimeout(() => {
    store.pending.delete(reqId);
    store.rejected = { target, text: 'Cihaz yanıt vermedi (zaman aşımı)', at: Date.now() };
    render();
  }, PENDING_TIMEOUT_MS);

  store.pending.set(reqId, { target, timer });
  ws.send(JSON.stringify({ type: 'cmd', reqId, target, action, seq: store.seq }));
  render();
}

// ── REST ───────────────────────────────────────────────────────────────────

async function api(path, opts = {}) {
  const o = Object.assign({ headers: {} }, opts);
  if (store.token) o.headers['Authorization'] = 'Bearer ' + store.token;
  if (o.body) o.headers['Content-Type'] = 'application/json';

  const r = await fetch(path, o);
  const body = await r.json().catch(() => ({}));
  if (!r.ok) {
    // Hata kodu VE alan adı çağırana taşınır. Cihaz hangi alanın reddedildiğini
    // söylüyor; bunu düşürürsek form yalnızca "istek başarısız" diyebilir.
    const info = body.error || {};
    const err = new Error(errText(info.code) || info.message || 'İstek başarısız');
    err.code = info.code;
    err.field = info.field;
    err.status = r.status;
    throw err;
  }
  return body;
}
