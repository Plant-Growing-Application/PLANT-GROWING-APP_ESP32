/* SALIXUS — çekirdek: sözlükler, durum deposu, bağlantı, komut yolu
 *
 * ══ MUTLAK KURAL: İYİMSER GÜNCELLEME YASAK ════════════════════════════════
 *   Tıkla → kart "BEKLİYOR" olur → komut gider → cihazdan ack ve gerçek
 *   telemetri gelene kadar kart GERÇEK röle durumunu gösterir.
 *   Tüm çizim YALNIZCA `store.state` üzerinden yapılır (ARCHITECTURE P5).
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * ══ İKİ SEVİYELİ ARAYÜZ ═══════════════════════════════════════════════════
 * Önceki sürümde altı sekme eşit ağırlıktaydı ve "Ayarlar" altında altı dev
 * bölüm vardı: güvenlik eşikleri, 8 slotluk kural düzenleyici, milisaniye
 * cinsinden süre kutuları, POSIX TZ dizesi, FreeRTOS stack sayaçları.
 * Bunların hepsi GEREKLİ ama hiçbiri "çilek yetiştirmek isteyen biri"nin
 * ilk ekranda görmesi gereken şey değil.
 *
 * Çözüm SİLMEK DEĞİL, KATMANLAMAK: basit mod üç sekme gösterir, uzman modu
 * eskisinin tamamını geri açar. Tercih `localStorage`'da saklanır.
 */

// ── DOM yardımcıları ───────────────────────────────────────────────────────

function el(id) {
  const n = document.getElementById(id);
  if (!n) console.error('[ui] DOM ogesi bulunamadi:', id);
  return n;
}

const show = (n, on) => n && n.classList.toggle('hidden', !on);
const txt  = (n, s) => { if (n) n.textContent = s; };

/// HTML'e gömülen her cihaz metni buradan geçer: SSID ve zaman dilimi gibi
/// alanları kullanıcı yazar.
function esc(s) {
  return String(s == null ? '' : s).replace(/[&<>"']/g, (ch) => (
    { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]
  ));
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
  waterTemp: { label: 'Su Sıcaklığı', short: 'Su', unit: '°C', digits: 1, min: 5, max: 40 },
  flow:      { label: 'Su Akışı', short: 'Akış', unit: 'L/dk', digits: 2, min: 0, max: 8 },
  ph:        { label: 'pH (Asitlik)', short: 'pH', unit: '', digits: 2, min: 4, max: 9 },
  ec:        { label: 'Besin Yoğunluğu (EC)', short: 'EC', unit: 'mS/cm', digits: 2, min: 0, max: 4 },
  level:     { label: 'Su Seviyesi', short: 'Seviye', unit: '', digits: 0, min: 0, max: 2 },
  humidity:  { label: 'Ortam Nemi', short: 'Nem', unit: '%', digits: 0, min: 0, max: 100 },
  airTemp:   { label: 'Hava Sıcaklığı', short: 'Hava', unit: '°C', digits: 1, min: -10, max: 50 },
  light:     { label: 'Işık', short: 'Işık', unit: 'lx', digits: 0, min: 0, max: 50000 },
};

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
  pending: new Map(), // reqId -> { target, timer }
  rejected: null,
  seq: 1,
  historyData: null,
  activeChartSensor: 'ph',
  rules: [],          // kural düzenleyicinin YEREL kopyası
};

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
/// Gizlenen bir sekme AÇIKKEN uzman modu kapatılırsa kullanıcı boş bir
/// ekranda kalırdı; o durumda ana sekmeye dönüyoruz.
function applyExpertMode() {
  const on = isExpert();
  document.querySelectorAll('[data-expert]').forEach((n) => {
    n.classList.toggle('hidden', !on);
  });
  const t = el('expertToggle');
  if (t) t.checked = on;

  if (!on) {
    const activeTab = document.querySelector('.tab-btn.active');
    if (activeTab && activeTab.dataset.expert) switchView('garden');
  }
}

// ── WebSocket ──────────────────────────────────────────────────────────────

let ws = null;
let backoffStep = 0;
const BACKOFF = [1000, 2000, 4000, 8000, 15000];
let reconnectTimer = null;

function setLinked(on, reason) {
  store.linked = on;
  document.body.classList.toggle('stale', !on);
  show(el('linkBar'), !on);
  if (!on) txt(el('linkText'), reason || 'Bağlantı kesildi — gösterilen veriler eski');

  const cp = el('connPill');
  const cl = el('connLabel');
  if (cp && cl) {
    cp.classList.toggle('offline', !on);
    txt(cl, on ? 'Canlı' : 'Kopuk');
  }
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

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && !store.linked && store.token) {
    backoffStep = 0;
    connect();
  }
});

function onState(msg) {
  if (store.state && msg.v < store.state.v) {
    console.warn('[ws] versiyon geriye gitti — cihaz yeniden baslamis');
    store.pending.forEach((p) => clearTimeout(p.timer));
    store.pending.clear();
  }

  // Cihaz ürün/dönem değiştirdiyse (otomatik dönem ilerlemesi) hedef bantlar
  // bayatlamıştır; yeniden çekiyoruz. Aksi hâlde panel meyve dönemindeyken
  // çiçeklenme hedeflerine göre "iyi/kötü" derdi.
  const prev = store.state && store.state.crop;
  const now  = msg.crop;
  if (now && (!prev || prev.key !== now.key || prev.stage !== now.stage)) {
    loadCrop();
  }

  store.state = msg;
  render();
}

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
