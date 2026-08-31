/* Sera Kontrol istemcisi — TASK-048 / TASK-049
 *
 * ══ MUTLAK KURAL: İYİMSER GÜNCELLEME YASAK ═════════════════════════════════
 *
 *   YASAK (eski proje):
 *     tıkla → kartın sınıfını 'on' yap → "ÇALIŞIYOR" yaz → komut gönder
 *     → cihaz REDDETSE BİLE arayüz "ÇALIŞIYOR" gösterir
 *
 *   ZORUNLU:
 *     tıkla → kart "BEKLİYOR" → komut gönder
 *     → ack (kabul/ret) → state paketi → kart GERÇEK duruma geçer
 *
 * Yapısal garanti: `render()` YALNIZCA `store.state`'ten okur ve `store.state`
 * yalnızca `onState()` içinde yazılır. Komut gönderen kod state'e dokunamaz.
 * ═══════════════════════════════════════════════════════════════════════════
 */
'use strict';

// ── Yardımcılar ────────────────────────────────────────────────────────────

/* Eksik DOM öğesi SESSİZ GEÇMEZ. Eski projede `r1`/`r2` gibi var olmayan
 * id'lere yazma denemesi vardı ve hiçbir şey olmuyordu — hata geliştirme
 * sırasında hiç fark edilmedi. */
function el(id) {
  const n = document.getElementById(id);
  if (!n) console.error('[ui] DOM ogesi bulunamadi:', id);
  return n;
}
const show = (n, on) => n && n.classList.toggle('hidden', !on);
const txt  = (n, s) => { if (n) n.textContent = s; };

function fmtUptime(ms) {
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600);
  const m = Math.floor(s % 3600 / 60);
  return d > 0 ? `${d}g ${h}s` : h > 0 ? `${h}s ${m}d` : `${m}d ${s % 60}sn`;
}

const SENSOR_META = {
  waterTemp: { label: 'Su sıcaklığı', unit: '°C', digits: 1 },
  flow:      { label: 'Debi',         unit: 'L/dk', digits: 2 },
  ph:        { label: 'pH',           unit: '',    digits: 2 },
  ec:        { label: 'EC',           unit: 'mS/cm', digits: 2 },
  level:     { label: 'Su seviyesi',  unit: '',    digits: 0 },
  humidity:  { label: 'Nem',          unit: '%',   digits: 0 },
};
const LEVEL_TEXT = ['KRİTİK', 'DÜŞÜK', 'YETERLİ'];

const ACT_META = {
  waterPump: 'Su pompası', airPump: 'Hava pompası',
  aux1: 'Yedek 1', aux2: 'Yedek 2',
};

const MODE_TEXT = {
  booting: 'Açılıyor', running: 'Çalışıyor', degraded: 'Kısıtlı',
  safe: 'Güvenli', emergency: 'ACİL DURUM',
};

const NET_TEXT = {
  boot: 'Başlıyor', apOnly: 'Kurulum (AP)', connecting: 'Bağlanıyor',
  connected: 'Bağlı', backoff: 'Yeniden denenecek', apFallback: 'AP + yeniden deneme',
};

/* Cihazdan gelen ErrCode → Türkçe açıklama. Cihaz metin taşımaz
 * (`LogRecord` 12 bayt); çeviri sunum katmanının işidir. */
const ERR_TEXT = {
  0x0501: 'Asgari çalışma süresi dolmadı',
  0x0502: 'Bekleme süresi dolmadı',
  0x0503: 'Azami çalışma süresi aşıldı',
  0x0504: 'Talep ile gerçek durum uyuşmuyor',
  0x0601: 'Su seviyesi yetersiz',
  0x0602: 'Seviye sensörü okunamıyor',
  0x0603: 'Kuru çalışma tespit edildi',
  0x0604: 'Akış doğrulaması başarısız',
  0x0605: 'Acil durum mandalı aktif',
  0x0606: 'Güvenlik engeli',
  0x0701: 'Kayıtlı ağ yok',
  0x0702: 'Wi-Fi parolası hatalı',
  0x0703: 'Ağ bulunamadı',
  0x0801: 'Saat senkronize değil',
};
const errText = (c) => (c ? (ERR_TEXT[c] || ('Hata 0x' + c.toString(16))) : '');

// ── Store — TEK doğruluk kaynağı ───────────────────────────────────────────

const store = {
  token: null,
  state: null,          // cihazdan gelen son tam görüntü; BAŞKA hiçbir yerde yazılmaz
  linked: false,
  pending: new Map(),   // reqId → { target, timer }
  seq: 1,
};

// ── WebSocket istemcisi ────────────────────────────────────────────────────

let ws = null;
let backoffStep = 0;
const BACKOFF = [1000, 2000, 4000, 8000, 15000];
let reconnectTimer = null;

function setLinked(on, reason) {
  store.linked = on;
  document.body.classList.toggle('stale', !on);
  show(el('linkBar'), !on);
  if (!on) txt(el('linkText'), reason || 'Bağlantı yok — gösterilen veriler eski');
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
    catch (e) { console.error('[ws] cozumlenemeyen mesaj', e); return; }

    if (msg.type === 'state') onState(msg);
    else if (msg.type === 'ack') onAck(msg);
  };

  ws.onclose = (ev) => {
    setLinked(false, ev.code === 1008
      ? 'Oturum geçersiz — yeniden giriş yapın'
      : 'Bağlantı yok — gösterilen veriler eski');

    if (ev.code === 1008) { store.token = null; showLogin(); return; }
    scheduleReconnect();
  };

  ws.onerror = () => { try { ws.close(); } catch (e) { /* zaten kapalı */ } };
}

function scheduleReconnect() {
  const wait = BACKOFF[Math.min(backoffStep, BACKOFF.length - 1)];
  backoffStep++;
  reconnectTimer = setTimeout(connect, wait);
}

/* Kullanıcı sekmeye döndüğünde 15 saniye beklemek kabul edilemez;
 * arka planda 15 saniyede bir denemek ise kaynağı boşuna harcamaz. */
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && !store.linked && store.token) {
    backoffStep = 0;
    connect();
  }
});

// ── Mesaj işleyiciler ──────────────────────────────────────────────────────

/* store.state'in TEK yazarı. */
function onState(msg) {
  // Versiyon geriye gittiyse cihaz yeniden başlamıştır. WS zaten kopar ve
  // yeniden bağlanmada tam state gelir; koruma olarak açıkça kontrol edilir.
  if (store.state && msg.v < store.state.v) {
    console.warn('[ws] versiyon geriye gitti — cihaz yeniden baslamis');
    store.pending.forEach((p) => clearTimeout(p.timer));
    store.pending.clear();
  }
  store.state = msg;
  render();
}

function onAck(msg) {
  const p = store.pending.get(msg.reqId);
  if (!p) return;

  clearTimeout(p.timer);
  store.pending.delete(msg.reqId);

  // result: 0=ACCEPTED 1=NO_CHANGE 2=REJECTED_SAFETY 3=REJECTED_MODE
  //         4=REJECTED_INVALID 5=DEFERRED_MIN_RUNTIME 6=DEFERRED_COOLDOWN 7=BUSY
  if (msg.result !== 0 && msg.result !== 1) {
    // Reddedildiyse NEDENİ gösterilir. Sessizce eski duruma dönmek kafa
    // karıştırır: kullanıcı butonun çalışmadığını sanır.
    p.reason = errText(msg.reason) ||
      (msg.result === 7 ? 'Cihaz meşgul, tekrar deneyin' : 'Komut reddedildi');
    store.rejected = { target: p.target, text: p.reason, at: Date.now() };
  }
  render();
}

// ── Komut gönderimi ────────────────────────────────────────────────────────

const PENDING_TIMEOUT_MS = 5000;

function sendCmd(target, action) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    store.rejected = { target, text: 'Bağlantı yok', at: Date.now() };
    render();
    return;
  }

  const reqId = 'r' + (store.seq++);

  // Ack gelmezse kart "bekliyor"da SONSUZA KADAR kalmaz.
  const timer = setTimeout(() => {
    store.pending.delete(reqId);
    store.rejected = { target, text: 'Cihaz yanıt vermedi', at: Date.now() };
    render();
  }, PENDING_TIMEOUT_MS);

  store.pending.set(reqId, { target, timer });
  ws.send(JSON.stringify({ type: 'cmd', reqId, target, action, seq: store.seq }));

  // Kart "BEKLİYOR"a geçer — AÇIK/KAPALI'ya DEĞİL.
  render();
}

// ── Görünüm ────────────────────────────────────────────────────────────────

let currentView = 'dashboard';

function switchView(name) {
  currentView = name;
  document.querySelectorAll('.tab').forEach((t) =>
    t.classList.toggle('active', t.dataset.view === name));
  document.querySelectorAll('.page').forEach((p) =>
    p.classList.toggle('hidden', p.id !== 'v-' + name));

  if (name === 'diagnostics') loadDiagnostics();
  if (name === 'settings') loadConfig();
}

function render() {
  const s = store.state;
  if (!s) return;   // veri gelene kadar "Yükleniyor…" kalır; varsayılan değer YOK

  renderStatus(s);
  renderSafetyBar(s);
  renderSensors(s);
  renderActuators(s);
  renderNetwork(s);
}

function renderStatus(s) {
  txt(el('sMode'), MODE_TEXT[s.system.mode] || s.system.mode);
  txt(el('sUptime'), fmtUptime(s.system.uptimeMs));
  txt(el('sHeap'), Math.round(s.system.freeHeap / 1024) + ' KB');

  // Saat geçersizken SAHTE DEĞER GÖSTERİLMEZ. Eski sistem "00:00:00"
  // gösteriyordu ve bu gerçek bir saat sanılıyordu.
  txt(el('sClock'), s.time.valid
    ? new Date(s.time.epoch * 1000).toLocaleTimeString('tr-TR')
    : 'geçersiz');
}

function renderSafetyBar(s) {
  const bar = el('safetyBar');
  if (!bar) return;

  if (s.safety.latched) {
    bar.className = 'safetybar critical';
    bar.textContent = 'ACİL DURUM: ' + (errText(s.safety.reason) || 'mandallandı') +
      ' — operatör onayı gerekiyor';
    show(bar, true);
  } else if (s.safety.interlocks) {
    bar.className = 'safetybar';
    bar.textContent = 'Güvenlik kilidi aktif: ' + (errText(s.safety.reason) || 'kilit');
    show(bar, true);
  } else {
    show(bar, false);
  }
}

function renderSensors(s) {
  const g = el('sensorGrid');
  if (!g) return;
  g.innerHTML = '';

  s.sensors.forEach((sn) => {
    const meta = SENSOR_META[sn.id] || { label: sn.id, unit: '', digits: 2 };
    const bad = sn.quality !== 'ok';

    /* Kalite → gösterim. OLED ile (TASK-050 Karar 3) BİREBİR aynı olmalı:
     * aynı veriyi iki arayüzün farklı göstermesi, hangisine bakıldığına göre
     * farklı karar verilmesi demektir.
     *
     *   fault / notPresent → SAYI YOK. Değer çöptür; üstü çizili göstermek
     *                        bile operatörün kafasına bir sayı sokar.
     *   stale / outOfRange → değer GÖSTERİLİR (bir zamanlar gerçek bir
     *                        okumaydı), rozetle işaretlenir.
     */
    const noValue = (sn.quality === 'fault' || sn.quality === 'notPresent');

    let value, unit;
    if (noValue) {
      value = (sn.quality === 'notPresent') ? 'yok' : '—';
      unit = '';
    } else if (sn.id === 'level') {
      value = LEVEL_TEXT[Math.round(sn.value)] || '?';
      unit = '';
    } else {
      value = Number(sn.value).toFixed(meta.digits);
      unit = meta.unit;
    }

    const d = document.createElement('div');
    d.className = 'card sensor' + (bad ? ' bad' : '') + (noValue ? ' novalue' : '');
    d.innerHTML =
      `<div class="name">${meta.label}` +
      `<span class="badge ${bad ? 'bad' : 'ok'}">${qualityText(sn.quality)}</span></div>` +
      `<div class="val">${value}<span class="unit">${unit}</span></div>`;
    g.appendChild(d);
  });
}

function qualityText(q) {
  return { ok: 'ölçüldü', stale: 'bayat', outOfRange: 'aralık dışı',
           fault: 'arızalı', notPresent: 'yok' }[q] || q;
}

function renderActuators(s) {
  const g = el('actuatorGrid');
  if (!g) return;
  g.innerHTML = '';

  const pendingTargets = new Set([...store.pending.values()].map((p) => p.target));

  s.actuators.forEach((a) => {
    if (!ACT_META[a.id]) return;

    const pending = pendingTargets.has(a.id);
    const d = document.createElement('div');
    d.className = 'card act ' + (pending ? 'pending' : (a.on ? 'on' : 'off'));

    // "BEKLİYOR" durumu AÇIK göstermez. Gerçek durum yalnızca `a.on`'dan
    // gelir ve `a.on` cihazın GERÇEK pin okumasıdır.
    const stateText = pending ? 'BEKLİYOR…' : (a.on ? 'ÇALIŞIYOR' : 'KAPALI');

    let why = '';
    if (store.rejected && store.rejected.target === a.id &&
        Date.now() - store.rejected.at < 8000) {
      why = store.rejected.text;
    } else if (a.block) {
      why = errText(a.block);
    }

    d.innerHTML =
      `<div class="name muted">${ACT_META[a.id]}</div>` +
      `<div class="state">${stateText}</div>` +
      `<div class="why">${why}</div>`;

    const btn = document.createElement('button');
    btn.textContent = a.on ? 'Kapat' : 'Aç';
    btn.disabled = pending || !store.linked;
    btn.onclick = () => sendCmd(a.id, a.on ? 'off' : 'on');
    d.appendChild(btn);

    g.appendChild(d);
  });
}

function renderNetwork(s) {
  const c = el('netCard');
  if (!c) return;
  const n = s.network;
  c.innerHTML =
    `<p><b>${NET_TEXT[n.state] || n.state}</b></p>` +
    `<p class="muted small">SSID: ${n.ssid || '—'}</p>` +
    `<p class="muted small">IP: ${n.ip || '—'}</p>` +
    `<p class="muted small">Sinyal: ${n.rssi ? n.rssi + ' dBm' : '—'}</p>` +
    (n.apActive ? `<p class="muted small">Kurulum AP açık (${n.apClients} istemci)</p>` : '') +
    (n.lastError ? `<p class="err">${errText(n.lastError)}</p>` : '');
}

// ── REST çağrıları ─────────────────────────────────────────────────────────

async function api(path, opts = {}) {
  const o = Object.assign({ headers: {} }, opts);
  if (store.token) o.headers['Authorization'] = 'Bearer ' + store.token;
  if (o.body) o.headers['Content-Type'] = 'application/json';

  const r = await fetch(path, o);
  const body = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(body.error ? errText(body.error.code) || body.error.message
                                        : 'İstek başarısız');
  return body;
}

async function loadDiagnostics() {
  try {
    const d = await api('/api/diagnostics');
    const fc = el('faultCard');
    fc.innerHTML = d.active.length
      ? d.active.map((c) => `<p class="err">${errText(c)}</p>`).join('')
      : '<p class="muted">Aktif hata yok.</p>';

    el('logBody').innerHTML = d.events.slice().reverse().map((e) =>
      `<tr class="lvl-${e.level}"><td>${fmtUptime(e.t)}</td>` +
      `<td>${errText(e.code) || '0x' + e.code.toString(16)}</td>` +
      `<td>${e.d}</td></tr>`).join('');
  } catch (e) {
    el('faultCard').innerHTML = `<p class="err">${e.message}</p>`;
  }
}

async function loadConfig() {
  try {
    const c = await api('/api/config');

    el('safetyForm').innerHTML =
      field('cfgFlowDelay', 'Akış doğrulama gecikmesi (ms)', c.safety.flowVerifyDelayMs) +
      field('cfgFlowMin', 'Asgari debi (L/dk)', c.safety.flowMinRate) +
      field('cfgGrace', 'Azami süre payı (ms)', c.safety.maxRuntimeGraceMs) +
      field('cfgViol', 'Acil duruma geçiş için ihlal sayısı', c.safety.maxRuntimeViolations) +
      checkbox('cfgReqLevel', 'Seviye sensörü zorunlu', c.safety.requireLevelSensor) +
      '<button id="saveSafety" class="primary">Kaydet</button><p id="safetyMsg" class="msg"></p>';

    el('systemForm').innerHTML =
      field('cfgTz', 'Zaman dilimi (POSIX TZ)', c.system.timezone, 'text') +
      field('cfgTelem', 'Telemetri aralığı (ms)', c.system.telemetryIntervalMs) +
      '<button id="saveSystem" class="primary">Kaydet</button><p id="systemMsg" class="msg"></p>';

    el('saveSafety').onclick = () => saveSection('/api/config/safety', 'safetyMsg', {
      flowVerifyDelayMs: +el('cfgFlowDelay').value,
      flowMinRate: +el('cfgFlowMin').value,
      maxRuntimeGraceMs: +el('cfgGrace').value,
      maxRuntimeViolations: +el('cfgViol').value,
      requireLevelSensor: el('cfgReqLevel').checked,
    });

    el('saveSystem').onclick = () => saveSection('/api/config/system', 'systemMsg', {
      timezone: el('cfgTz').value,
      telemetryIntervalMs: +el('cfgTelem').value,
    });
  } catch (e) {
    el('safetyForm').innerHTML = `<p class="err">${e.message}</p>`;
  }
}

const field = (id, label, val, type = 'number') =>
  `<label for="${id}">${label}</label><input id="${id}" type="${type}" value="${val}">`;
const checkbox = (id, label, on) =>
  `<label><input id="${id}" type="checkbox" ${on ? 'checked' : ''} style="width:auto"> ${label}</label>`;

async function saveSection(path, msgId, body) {
  const m = el(msgId);
  try {
    await api(path, { method: 'PUT', body: JSON.stringify(body) });
    m.className = 'msg'; txt(m, 'Kaydedildi.');
  } catch (e) {
    m.className = 'err'; txt(m, e.message);
  }
}

// ── Ağ tarama ──────────────────────────────────────────────────────────────

let scanPoll = null;

async function startScan() {
  clearInterval(scanPoll);
  try {
    // Yanıt HER DURUMDA aynı şemada gelir; ayrı bir "taranıyor" dalı yok.
    // Eski sistemin "ilk tıklama her zaman başarısız" hatasının kökü buydu.
    renderScan(await api('/api/network/scan', { method: 'POST' }));
    scanPoll = setInterval(pollScan, 1200);
  } catch (e) {
    txt(el('scanStatus'), e.message);
  }
}

async function pollScan() {
  try {
    const r = await api('/api/network/scan');
    renderScan(r);
    if (r.status !== 'running') clearInterval(scanPoll);
  } catch (e) { clearInterval(scanPoll); }
}

function renderScan(r) {
  const st = { idle: 'Hazır', running: 'Taranıyor…', done: 'Tamamlandı',
               failed: 'Tarama başarısız' }[r.status] || r.status;
  txt(el('scanStatus'), st + (r.truncated ? ' (liste kesildi)' : ''));

  // `networks` HER ZAMAN dizi — boş olsa bile. Bu garanti sayesinde
  // koşulsuz `map` çağrılabilir.
  el('scanList').innerHTML = r.networks
    .sort((a, b) => b.rssi - a.rssi)
    .map((n) => `<li data-ssid="${n.ssid}"><span>${n.ssid}${n.open ? ' 🔓' : ''}</span>` +
                `<span class="muted small">${n.rssi} dBm</span></li>`).join('');

  el('scanList').querySelectorAll('li').forEach((li) => {
    li.onclick = () => { el('ssid').value = li.dataset.ssid; el('wifiPw').focus(); };
  });
}

// ── Giriş ──────────────────────────────────────────────────────────────────

let setupMode = false;

async function checkAuth() {
  try {
    const s = await api('/api/auth/status');
    setupMode = s.setupMode;
  } catch (e) { setupMode = false; }

  txt(el('loginHint'), setupMode
    ? 'İlk kurulum: cihaz için bir parola belirleyin (en az 8 karakter).'
    : 'Devam etmek için parolanızı girin.');
  show(el('pw2'), setupMode);
  show(el('pw2Label'), setupMode);
  el('loginBtn').textContent = setupMode ? 'Parolayı belirle' : 'Giriş';
}

function showLogin() {
  show(el('loginView'), true);
  show(el('appView'), false);
  checkAuth();
}

async function doLogin() {
  const e = el('loginErr');
  txt(e, '');
  const pass = el('pw').value;

  try {
    if (setupMode) {
      if (pass.length < 8) { txt(e, 'Parola en az 8 karakter olmalı.'); return; }
      if (pass !== el('pw2').value) { txt(e, 'Parolalar eşleşmiyor.'); return; }
      await api('/api/setup/password', { method: 'POST', body: JSON.stringify({ password: pass }) });
    }
    const r = await api('/api/auth/login', { method: 'POST', body: JSON.stringify({ password: pass }) });

    store.token = r.token;
    el('pw').value = ''; el('pw2').value = '';
    show(el('loginView'), false);
    show(el('appView'), true);
    connect();
  } catch (err) {
    txt(e, 'Giriş başarısız. Parolayı kontrol edin.');
  }
}

// ── Başlangıç ──────────────────────────────────────────────────────────────

function init() {
  document.querySelectorAll('.tab').forEach((t) => {
    t.onclick = () => switchView(t.dataset.view);
  });

  el('loginBtn').onclick = doLogin;
  el('pw').addEventListener('keydown', (e) => { if (e.key === 'Enter') doLogin(); });

  el('estopBtn').onclick = () => sendCmd('system', 'emergencyStop');
  el('eclearBtn').onclick = () => sendCmd('system', 'emergencyClear');

  el('scanBtn').onclick = startScan;
  el('refreshDiag').onclick = loadDiagnostics;

  el('saveWifi').onclick = async () => {
    const m = el('netMsg');
    try {
      await api('/api/config/network', {
        method: 'PUT',
        body: JSON.stringify({ ssid: el('ssid').value, password: el('wifiPw').value }),
      });
      el('wifiPw').value = '';
      m.className = 'msg'; txt(m, 'Kaydedildi, bağlanılıyor…');
    } catch (e) { m.className = 'err'; txt(m, e.message); }
  };

  el('forgetWifi').onclick = async () => {
    if (!confirm('Kayıtlı ağ silinsin mi? Cihaz kurulum AP moduna dönecek.')) return;
    try { await api('/api/network/forget', { method: 'POST' }); }
    catch (e) { txt(el('netMsg'), e.message); }
  };

  el('retryNow').onclick = () => api('/api/network/retry', { method: 'POST' })
    .then(() => txt(el('netMsg'), 'Yeniden deneniyor…'))
    .catch((e) => txt(el('netMsg'), e.message));

  el('factoryBtn').onclick = async () => {
    if (!confirm('TÜM ayarlar, ağ bilgileri ve parola silinecek. Emin misiniz?')) return;
    try {
      await api('/api/system/factory-reset?confirm=FACTORY_RESET', { method: 'POST' });
      store.token = null;
      showLogin();
    } catch (e) { alert(e.message); }
  };

  setLinked(false);
  showLogin();
}

document.addEventListener('DOMContentLoaded', init);
