/* SALIXUS — Modern Precision Agriculture & Hydroponics Web Client
 *
 * ══ MUTLAK KURAL: İYİMSER GÜNCELLEME YASAK (No Optimistic UI) ══════════════
 *   Tıkla → Kart "BEKLİYOR" durumuna geçer → Komut gönderilir
 *   → Cihazdan ack (kabul/ret) ve gerçek state telemetrisi gelene kadar
 *     kart GERÇEK pin durumunu yansıtır.
 *
 *   Tüm çizim YALNIZCA `store.state` üzerinden yapılır.
 * ═══════════════════════════════════════════════════════════════════════════
 */
'use strict';

// ── DOM ve Yardımcı Fonksiyonlar ───────────────────────────────────────────

function el(id) {
  const n = document.getElementById(id);
  if (!n) console.error('[ui] DOM öğesi bulunamadı:', id);
  return n;
}

const show = (n, on) => n && n.classList.toggle('hidden', !on);
const txt  = (n, s) => { if (n) n.textContent = s; };

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

function fmtBytes(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

const SENSOR_META = {
  waterTemp: { label: 'Su Sıcaklığı', unit: '°C', digits: 1, min: 10, max: 40, idealMin: 18, idealMax: 24 },
  flow:      { label: 'Su Debisi',    unit: 'L/dk', digits: 2, min: 0, max: 8, idealMin: 1.5, idealMax: 5 },
  ph:        { label: 'pH Seviyesi',  unit: '', digits: 2, min: 4, max: 9, idealMin: 5.5, idealMax: 6.5 },
  ec:        { label: 'EC İletkenlik', unit: 'mS/cm', digits: 2, min: 0, max: 4, idealMin: 1.2, idealMax: 2.4 },
  level:     { label: 'Su Seviyesi',  unit: '', digits: 0, min: 0, max: 2, idealMin: 2, idealMax: 2 },
  humidity:  { label: 'Ortam Nemi',   unit: '%', digits: 0, min: 20, max: 95, idealMin: 50, idealMax: 75 },
};

const LEVEL_TEXT = ['KRİTİK', 'DÜŞÜK', 'YETERLİ'];
const LEVEL_PERCENT = [20, 55, 100];

const ACT_META = {
  waterPump: { name: 'Su Pompası', desc: 'Ana Besleme ve Sirkülasyon' },
  airPump:   { name: 'Hava Pompası', desc: 'Kök Havalandırma & Oksijen' },
  aux1:      { name: 'Yedek Röle 1', desc: 'Yardımcı Çıkış 1 (Aux)' },
  aux2:      { name: 'Yedek Röle 2', desc: 'Yardımcı Çıkış 2 (Aux)' },
};

const MODE_TEXT = {
  booting: 'Başlatılıyor',
  running: 'Çalışıyor',
  degraded: 'Kısıtlı Mod',
  safe: 'Güvenli Mod',
  emergency: 'ACİL DURUM',
};

const NET_TEXT = {
  boot: 'Başlatılıyor',
  apOnly: 'Kurulum (AP Modu)',
  connecting: 'Ağa Bağlanıyor…',
  connected: 'Bağlı',
  backoff: 'Yeniden Denenecek',
  apFallback: 'AP + Yeniden Deneme',
};

const ERR_TEXT = {
  0x0101: 'Sistem boot aşaması başarısız oldu',
  0x0102: 'Önceki oturum Watchdog (WDT) ile sonlandı',
  0x0103: 'FreeRTOS görevi oluşturulamadı',
  0x0104: 'Görev heartbeat sinyali kesildi',
  0x0105: 'Kritik düşük heap bellek seviyesi',
  0x0201: 'Kayıtlı yapılandırma bulunamadı',
  0x0202: 'Yapılandırma kaydı bozuk',
  0x0203: 'Firmware sürümü geriye alınmış',
  0x0204: 'Yapılandırma doğrulaması başarısız',
  0x0301: 'NVS depolama açılamadı',
  0x0302: 'LittleFS dosya sistemi bağlanamadı',
  0x0303: 'Flash belleğe yazma başarısız',
  0x0304: 'Depolama alanı dolu',
  0x0305: 'Bozuk telemetri kaydı tespit edildi',
  0x0401: 'Sensör donanımda takılı değil',
  0x0402: 'Sensör devresi açık/kopuk',
  0x0403: 'Sensör devresinde kısa devre',
  0x0404: 'Sensör değeri aralık dışında',
  0x0405: 'Sensör verisi bayatladı / değişmiyor',
  0x0406: 'Fiziksel olmayan değer sıçraması',
  0x0407: 'Akış sensöründen darbe sinyali gelmiyor',
  0x0501: 'Asgari çalışma süresi (minRun) dolmadı',
  0x0502: 'Bekleme süresi (cooldown) dolmadı',
  0x0503: 'Azami çalışma süresi aşıldı, otomatik kapatıldı',
  0x0504: 'Talep ile gerçek pin durumu uyuşmuyor',
  0x0601: 'Su seviyesi yetersiz (Kuru Çalışma Koruması)',
  0x0602: 'Seviye sensörü okunamıyor (Güvenlik Koruması)',
  0x0603: 'Kuru çalışma tespit edildi',
  0x0604: 'Akış doğrulaması başarısız oldu',
  0x0605: 'Acil durum mandalı aktif',
  0x0606: 'Güvenlik kilidi (veto) devrede',
  0x0701: 'Kayıtlı Wi-Fi ağı bulunamadı',
  0x0702: 'Wi-Fi parolası hatalı',
  0x0703: 'Wi-Fi ağı bulunamadı veya kapsama dışı',
  0x0704: 'Ağ bağlantısı koptu',
  0x0705: 'Ağ bağlantı zaman aşımı',
  0x0706: 'Ağ taraması başarısız oldu',
  0x0707: 'Statik IP yapılandırması geçersiz',
  0x0801: 'Cihaz saati senkronize değil',
  0x0802: 'SNTP saat senkronizasyonu başarısız',
  0x0901: 'Yetkisiz erişim isteği',
  0x0902: 'Geçersiz istek parametresi',
  0x0903: 'Cihaz komut kuyruğu meşgul',
  0x0904: 'İstek boyutu izin verilen sınırı aştı',
  0x0A01: 'OLED ekran yanıt vermiyor',
  0x0A02: 'Kullanıcı giriş kuyruğu dolu',
};

const errText = (c) => (c ? (ERR_TEXT[c] || ('Hata 0x' + c.toString(16).toUpperCase())) : '');

// ── Store (Tek Doğruluk Kaynağı) ───────────────────────────────────────────

const store = {
  token: null,
  state: null,          // Cihazdan gelen son onaylı durum
  linked: false,
  pending: new Map(),   // reqId -> { target, timer }
  rejected: null,
  seq: 1,
  historyData: null,
  activeChartSensor: 'ph',
  // Kural düzenleyicinin YEREL kopyası. Cihaza yalnızca "Kaydet" ile,
  // bütün olarak gider (ISSUE-021).
  rules: [],
};

// ── WebSocket İstemcisi ───────────────────────────────────────────────────

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

  ws.onopen = () => {
    backoffStep = 0;
    setLinked(true);
  };

  ws.onmessage = (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); }
    catch (e) { console.error('[ws] çözümlenemeyen mesaj:', e); return; }

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

  ws.onerror = () => {
    try { ws.close(); } catch (e) {}
  };
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

// ── Mesaj İşleyicileri ─────────────────────────────────────────────────────

function onState(msg) {
  if (store.state && msg.v < store.state.v) {
    console.warn('[ws] versiyon geriye gitti — cihaz yeniden başlamış');
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

  if (msg.result !== 0 && msg.result !== 1) {
    p.reason = errText(msg.reason) ||
      (msg.result === 7 ? 'Cihaz meşgul, tekrar deneyin' : 'Komut güvenlik vetosuyla reddedildi');
    store.rejected = { target: p.target, text: p.reason, at: Date.now() };
  }
  render();
}

// ── Komut Gönderimi ────────────────────────────────────────────────────────

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

// ── Görünüm ve Sayfa Geçişi ────────────────────────────────────────────────

let currentView = 'dashboard';

function switchView(name) {
  currentView = name;
  document.querySelectorAll('.tab-btn').forEach((t) =>
    t.classList.toggle('active', t.dataset.view === name));
  document.querySelectorAll('.page').forEach((p) =>
    p.classList.toggle('hidden', p.id !== 'v-' + name));

  if (name === 'diagnostics') loadDiagnostics();
  if (name === 'settings') loadConfig();
  if (name === 'charts') loadHistory();
}

// ── Render Motoru ──────────────────────────────────────────────────────────

function render() {
  const s = store.state;
  if (!s) return;

  renderStatus(s);
  renderSafetyBar(s);
  renderSensors(s);
  renderActuators(s);
  renderNetwork(s);
}

function renderStatus(s) {
  const modeName = MODE_TEXT[s.system.mode] || s.system.mode;
  txt(el('sMode'), modeName);
  txt(el('headMode'), modeName);

  const hmp = el('headModePill');
  if (hmp) {
    hmp.className = 'mode-pill ' + (s.system.mode === 'emergency' ? 'emergency' : (s.system.mode === 'safe' ? 'safe' : ''));
  }

  txt(el('sUptime'), fmtUptime(s.system.uptimeMs));
  txt(el('sHeap'), Math.round(s.system.freeHeap / 1024) + ' KB');

  txt(el('sClock'), s.time.valid
    ? new Date(s.time.epoch * 1000).toLocaleTimeString('tr-TR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    : 'Geçersiz Saat');
}

function renderSafetyBar(s) {
  const bar = el('safetyBar');
  if (!bar) return;

  if (s.safety.latched) {
    bar.className = 'safetybar critical';
    bar.textContent = 'ACİL DURUM KİLİDİ: ' + (errText(s.safety.reason) || 'Mandallandı') +
      ' — Operatör onayı gerekiyor';
    show(bar, true);
  } else if (s.safety.interlocks) {
    bar.className = 'safetybar';
    bar.textContent = 'Güvenlik Kilidi Aktif: ' + (errText(s.safety.reason) || 'Kilit Devrede');
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
    const meta = SENSOR_META[sn.id] || { label: sn.id, unit: '', digits: 2, min: 0, max: 100 };
    const bad = sn.quality !== 'ok';
    const noValue = (sn.quality === 'fault' || sn.quality === 'notPresent');

    let valueStr, unitStr, pct = 0;

    if (noValue) {
      valueStr = (sn.quality === 'notPresent') ? 'Yok' : '—';
      unitStr = '';
      pct = 0;
    } else if (sn.id === 'level') {
      const idx = Math.max(0, Math.min(2, Math.round(sn.value)));
      valueStr = LEVEL_TEXT[idx] || '?';
      unitStr = '';
      pct = LEVEL_PERCENT[idx] || 50;
    } else {
      const num = Number(sn.value);
      valueStr = num.toFixed(meta.digits);
      unitStr = meta.unit;
      const min = meta.min || 0;
      const max = meta.max || 100;
      pct = Math.max(5, Math.min(100, Math.round(((num - min) / (max - min)) * 100)));
    }

    const qClass = sn.quality === 'ok' ? 'ok' : (sn.quality === 'stale' ? 'warn' : 'bad');

    const card = document.createElement('div');
    card.className = `card sensor-card ${bad ? 'bad' : ''} ${noValue ? 'novalue' : ''}`;
    card.innerHTML = `
      <div class="sensor-header">
        <span class="sensor-title">${meta.label}</span>
        <span class="badge ${qClass}">${qualityText(sn.quality)}</span>
      </div>
      <div class="sensor-body">
        <span class="sensor-val">${valueStr}</span>
        <span class="sensor-unit">${unitStr}</span>
      </div>
      <div class="sensor-bar-wrap">
        <div class="sensor-bar" style="width: ${pct}%"></div>
      </div>
    `;
    g.appendChild(card);
  });
}

function qualityText(q) {
  return {
    ok: 'Ölçüldü',
    stale: 'Bayat',
    outOfRange: 'Aralık Dışı',
    fault: 'Arızalı',
    notPresent: 'Takılı Değil'
  }[q] || q;
}

function renderActuators(s) {
  const g = el('actuatorGrid');
  const miniG = el('miniActGrid');
  if (g) g.innerHTML = '';
  if (miniG) miniG.innerHTML = '';

  const pendingTargets = new Set([...store.pending.values()].map((p) => p.target));

  s.actuators.forEach((a) => {
    const meta = ACT_META[a.id];
    if (!meta) return;

    const pending = pendingTargets.has(a.id);
    const stateClass = pending ? 'pending' : (a.on ? 'on' : 'off');
    const stateText = pending ? 'BEKLİYOR…' : (a.on ? 'ÇALIŞIYOR' : 'KAPALI');

    let why = '';
    if (store.rejected && store.rejected.target === a.id && Date.now() - store.rejected.at < 8000) {
      why = store.rejected.text;
    } else if (a.block) {
      why = errText(a.block);
    }

    // Ana Kontrol Grid Kartı
    if (g) {
      const card = document.createElement('div');
      card.className = `card act-card ${stateClass}`;
      card.innerHTML = `
        <div>
          <div class="act-header">
            <div>
              <div class="act-name">${meta.name}</div>
              <div class="muted small">${meta.desc}</div>
            </div>
            <span class="act-state-badge">${stateText}</span>
          </div>
          <div class="act-metrics">
            <span>Çalışma: <b>${fmtUptime(a.runMs)}</b></span>
            <span>Döngü: <b>${a.cycles || 0}</b></span>
          </div>
          <div class="act-why">${why}</div>
        </div>
      `;

      const btn = document.createElement('button');
      btn.className = `btn ${a.on ? 'btn-danger' : 'btn-primary'} btn-block`;
      btn.textContent = a.on ? `${meta.name} Kapat` : `${meta.name} Başlat`;
      btn.disabled = pending || !store.linked;
      btn.onclick = () => sendCmd(a.id, a.on ? 'off' : 'on');
      card.appendChild(btn);

      g.appendChild(card);
    }

    // Dashboard Hızlı Özet Kartı
    if (miniG) {
      const miniCard = document.createElement('div');
      miniCard.className = `card act-card ${stateClass}`;
      miniCard.innerHTML = `
        <div class="act-header" style="margin-bottom:0">
          <div>
            <div class="act-name" style="font-size:13px">${meta.name}</div>
            <div class="muted small">${fmtUptime(a.runMs)}</div>
          </div>
          <span class="act-state-badge">${stateText}</span>
        </div>
      `;
      miniG.appendChild(miniCard);
    }
  });
}

function renderNetwork(s) {
  const c = el('netCard');
  if (!c) return;
  const n = s.network;
  c.innerHTML = `
    <div style="display:grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 12px;">
      <div>
        <span class="muted small">Ağ Durumu</span>
        <div style="font-weight:700; font-size:15px; color:var(--accent-bright); margin-top:2px;">${NET_TEXT[n.state] || n.state}</div>
      </div>
      <div>
        <span class="muted small">Bağlı SSID</span>
        <div style="font-weight:600; margin-top:2px;">${n.ssid || '—'}</div>
      </div>
      <div>
        <span class="muted small">Cihaz IPv4 Adresi</span>
        <div style="font-weight:600; margin-top:2px; font-family:monospace;">${n.ip || '—'}</div>
      </div>
      <div>
        <span class="muted small">Wi-Fi Sinyal Gücü (RSSI)</span>
        <div style="font-weight:600; margin-top:2px;">${n.rssi ? n.rssi + ' dBm' : '—'}</div>
      </div>
    </div>
    ${n.apActive ? `<div style="margin-top:12px; padding:8px 12px; background:rgba(56,189,248,0.08); border-radius:6px; font-size:12.5px; color:var(--info);">📡 Kurulum Erişim Noktası (AP) aktif — <b>${n.apClients}</b> istemci bağlı</div>` : ''}
    ${n.lastError ? `<div class="err" style="margin-top:8px;">Son Hata: ${errText(n.lastError)}</div>` : ''}
  `;
}

// ── REST API İstemcisi ────────────────────────────────────────────────────

async function api(path, opts = {}) {
  const o = Object.assign({ headers: {} }, opts);
  if (store.token) o.headers['Authorization'] = 'Bearer ' + store.token;
  if (o.body) o.headers['Content-Type'] = 'application/json';

  const r = await fetch(path, o);
  const body = await r.json().catch(() => ({}));
  if (!r.ok) {
    // Hata kodu VE alan adı çağırana taşınır. Cihaz hangi alanın reddedildiğini
    // söylüyor (`{error:{code,message,field}}`); bunu burada düşürürsek form
    // kullanıcıya yalnızca "istek başarısız" diyebilir.
    const info = body.error || {};
    const err = new Error(errText(info.code) || info.message || 'İstek başarısız');
    err.code = info.code;
    err.field = info.field;
    err.status = r.status;
    throw err;
  }
  return body;
}

// ── Canlı Geçmiş Grafiği (Canvas) ──────────────────────────────────────────

async function loadHistory() {
  const meta = el('chartMeta');
  txt(meta, 'Veriler alınıyor…');

  try {
    const data = await api('/api/history?count=120');
    store.historyData = data;
    txt(meta, `${data.count} kayıt görüntülendi (${data.stored} toplam)`);
    drawChart();
  } catch (e) {
    txt(meta, 'Geçmiş verisi yüklenemedi: ' + e.message);
  }
}

function drawChart() {
  const data = store.historyData;
  if (!data || !data.rows || !data.rows.length) return;

  const canvas = el('historyCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');

  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = rect.width * dpr;
  canvas.height = 260 * dpr;
  ctx.scale(dpr, dpr);

  const w = rect.width;
  const h = 260;
  const padL = 44, padR = 16, padT = 20, padB = 30;
  const plotW = w - padL - padR;
  const plotH = h - padT - padB;

  ctx.clearRect(0, 0, w, h);

  const fieldIdx = data.fields.indexOf(store.activeChartSensor);
  if (fieldIdx < 0) return;

  const rows = data.rows;
  const values = rows.map((r) => r.v[fieldIdx]);

  let minVal = Math.min(...values);
  let maxVal = Math.max(...values);
  if (minVal === maxVal) { minVal -= 1; maxVal += 1; }
  const padVal = (maxVal - minVal) * 0.15;
  minVal -= padVal;
  maxVal += padVal;

  // Izgara Çizgileri
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.07)';
  ctx.lineWidth = 1;
  ctx.fillStyle = '#64748b';
  ctx.font = '11px sans-serif';
  ctx.textAlign = 'right';

  const gridSteps = 4;
  for (let i = 0; i <= gridSteps; i++) {
    const y = padT + (plotH / gridSteps) * i;
    const v = maxVal - ((maxVal - minVal) / gridSteps) * i;
    ctx.beginPath();
    ctx.moveTo(padL, y);
    ctx.lineTo(w - padR, y);
    ctx.stroke();
    ctx.fillText(v.toFixed(1), padL - 8, y + 4);
  }

  // Alan ve Çizgi Çizimi
  const points = rows.map((r, i) => {
    const x = padL + (i / (rows.length - 1 || 1)) * plotW;
    const val = r.v[fieldIdx];
    const y = padT + plotH - ((val - minVal) / (maxVal - minVal)) * plotH;
    return { x, y, val, t: r.t };
  });

  // Degrade Alan
  const grad = ctx.createLinearGradient(0, padT, 0, padT + plotH);
  grad.addColorStop(0, 'rgba(16, 185, 129, 0.35)');
  grad.addColorStop(1, 'rgba(16, 185, 129, 0.0)');

  ctx.beginPath();
  ctx.moveTo(points[0].x, padT + plotH);
  points.forEach((p) => ctx.lineTo(p.x, p.y));
  ctx.lineTo(points[points.length - 1].x, padT + plotH);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Çizgi
  ctx.beginPath();
  points.forEach((p, idx) => {
    if (idx === 0) ctx.moveTo(p.x, p.y);
    else ctx.lineTo(p.x, p.y);
  });
  ctx.strokeStyle = '#10b981';
  ctx.lineWidth = 2.5;
  ctx.stroke();

  // Noktalar
  ctx.fillStyle = '#34d399';
  points.forEach((p) => {
    ctx.beginPath();
    ctx.arc(p.x, p.y, 2.5, 0, Math.PI * 2);
    ctx.fill();
  });
}

// ── Teşhis ve Görev Sağlığı ────────────────────────────────────────────────

async function loadDiagnostics() {
  try {
    const d = await api('/api/diagnostics');

    // Aktif Hatalar
    const fc = el('faultCard');
    if (fc) {
      fc.innerHTML = d.active.length
        ? d.active.map((c) => `<div class="err" style="padding:4px 0;">⚠ ${errText(c)}</div>`).join('')
        : '<p class="muted" style="color:var(--accent-bright)">✔ Sistemde aktif hata bulunmuyor.</p>';
    }

    // FreeRTOS Görev Sağlığı
    const tc = el('tasksCard');
    if (tc && d.tasks) {
      tc.innerHTML = d.tasks.map((t) => `
        <div class="card task-card">
          <div class="task-name">${t.name}</div>
          <div class="task-metric">Döngü Süresi: <b>${t.maxLoopUs} µs</b></div>
          <div class="task-metric">Boş Stack: <b>${t.minStack} B</b></div>
          <div class="task-metric">Taşma (Overrun): <b>${t.overruns}</b></div>
        </div>
      `).join('');
    }

    // Olay Günlüğü
    const lb = el('logBody');
    if (lb && d.events) {
      lb.innerHTML = d.events.slice().reverse().map((e) => `
        <tr class="lvl-${e.level}">
          <td>${fmtUptime(e.t)}</td>
          <td><b>${errText(e.code) || '0x' + e.code.toString(16)}</b></td>
          <td class="muted">${e.d || '—'}</td>
        </tr>
      `).join('');
    }
  } catch (e) {
    if (el('faultCard')) el('faultCard').innerHTML = `<p class="err">${e.message}</p>`;
  }
}

// ── Yapılandırma ve Ayarlar ────────────────────────────────────────────────

// Geçerli aralıklar. TEK DOĞRULUK KAYNAĞI CİHAZDIR: `core/Config.h` içindeki
// `namespace limits`. Buradaki kopya yalnızca bir KOLAYLIKTIR — kullanıcı
// sınır dışı değeri göndermeden önce görsün diye. Cihaz her gövdeyi yeniden
// doğrular; bu tablo bayatlarsa istek REDDEDİLİR, sessizce kabul edilmez.
const LIMITS = {
  minRunMs:            [0, 600000],        // ACTUATOR_MIN_RUN     0 – 10 dk
  maxRunMs:            [1000, 7200000],    // ACTUATOR_MAX_RUN     1 sn – 2 sa
  cooldownMs:          [0, 3600000],       // ACTUATOR_COOLDOWN    0 – 1 sa
  flowVerifyDelayMs:   [1000, 60000],      // FLOW_VERIFY_DELAY    1 – 60 sn
  flowMinRate:         [0.01, 1000],       // FLOW_MIN_RATE        L/dk
  maxRuntimeGraceMs:   [0, 60000],         // MAX_RUNTIME_GRACE    0 – 60 sn
  maxRuntimeViolations: [1, 20],           // validateSafety       1 – 20 kez
  manualOverrideMs:    [60000, 86400000],  // MANUAL_OVERRIDE      1 dk – 24 sa
  telemetryIntervalMs: [200, 60000],       // TELEMETRY_INTERVAL   200 ms – 60 sn
};

// Cihazın döndürdüğü alan adı → kullanıcıya gösterilecek ad. Doğrulama hatası
// ALAN ADIYLA geliyor (`{error:{code,message,field}}`); bunu göstermezsek
// kullanıcı formdaki hangi kutunun reddedildiğini bilemez.
const FIELD_TEXT = {
  'network.ssid': 'Wi-Fi ağ adı',
  'network.password': 'Wi-Fi parolası',
  'network.staticIp': 'Statik IP',
  'network.gateway': 'Ağ geçidi',
  'network.subnet': 'Alt ağ maskesi',
  'network.dns': 'DNS sunucusu',
  'safety.flowVerifyDelayMs': 'Akış doğrulama gecikmesi',
  'safety.flowMinRate': 'Asgari debi eşiği',
  'safety.maxRuntimeGraceMs': 'Azami süre toleransı',
  'safety.maxRuntimeViolations': 'İhlal tolerans sayısı',
  'actuators.index': 'Aktüatör numarası',
  'actuators.minRunMs': 'Asgari çalışma süresi',
  'actuators.maxRunMs': 'Azami çalışma süresi',
  'actuators.cooldownMs': 'Bekleme süresi',
  'actuators.relayIndex': 'Röle eşlemesi',
  'automation.mode': 'Çalışma modu',
  'automation.manualOverrideMs': 'Manuel müdahale süresi',
  'system.timezone': 'Zaman dilimi',
  'system.telemetryIntervalMs': 'Telemetri periyodu',
  'system.logLevel': 'Kayıt seviyesi',
  'index': 'Aktüatör numarası',
  'password': 'Parola',
};

// `logLevel` seri porta yazılacak EN DÜŞÜK seviyedir: 0 seçilirse her şey,
// 3 seçilirse yalnızca kritik olaylar yazılır (core/Types.h → LogLevel).
const LOG_LEVELS = [
  [0, 'INFO — tüm olaylar'],
  [1, 'WARNING — uyarı ve üstü'],
  [2, 'ERROR — hata ve üstü'],
  [3, 'CRITICAL — yalnızca kritik'],
];

/// HTML'e gömülen cihaz metni. `innerHTML` ile yazdığımız her değer buradan
/// geçer: SSID ve zaman dilimi gibi alanları kullanıcı yazar.
function esc(s) {
  return String(s == null ? '' : s).replace(/[&<>"']/g, (ch) => (
    { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]
  ));
}

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

const trimNum = (v) => String(Math.round(v * 100) / 100);

const durHint = (val, range) =>
  fmtDur(val) + '  ·  izin verilen: ' + fmtDur(range[0]) + ' – ' + fmtDur(range[1]);

// ── Form parçaları ─────────────────────────────────────────────────────────

const field = (id, label, val, type = 'number', step = 'any') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <input id="${id}" type="${type}" step="${step}" value="${esc(val)}">
  </div>
`;

/// Sayısal alan — birim etiketi ve sınırlarıyla.
const numField = (id, label, val, unit, range, step = 'any', note = '') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <div class="field-row">
      <input id="${id}" type="number" step="${step}" min="${range[0]}" max="${range[1]}"
             value="${esc(val)}">
      <span class="field-unit">${unit}</span>
    </div>
    <span class="field-hint">izin verilen: ${range[0]} – ${range[1]} ${unit}</span>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>
`;

/// Süre alanı — ms girilir, altında insan ölçüsü canlı gösterilir.
const durField = (id, label, val, range, note = '') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <div class="field-row">
      <input id="${id}" type="number" step="100" min="${range[0]}" max="${range[1]}"
             value="${esc(val)}" data-dur="1" data-lo="${range[0]}" data-hi="${range[1]}">
      <span class="field-unit">ms</span>
    </div>
    <span class="field-hint" id="${id}Hint">${durHint(val, range)}</span>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>
`;

const checkbox = (id, label, on) => `
  <div class="form-group" style="margin-top:10px;">
    <label style="display:flex; align-items:center; cursor:pointer;">
      <input id="${id}" type="checkbox" ${on ? 'checked' : ''}>
      <span>${label}</span>
    </label>
  </div>
`;

const selectField = (id, label, options, current, note = '') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <select id="${id}">
      ${options.map(([v, t]) =>
        `<option value="${esc(v)}"${String(v) === String(current) ? ' selected' : ''}>${t}</option>`
      ).join('')}
    </select>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>
`;

/// `data-dur` işaretli her kutuya canlı süre ipucu bağlar ve aralık dışına
/// çıkıldığında ipucunu kırmızıya çevirir — kullanıcı REDDEDİLECEK bir değeri
/// göndermeden önce görür.
function bindDurHints() {
  document.querySelectorAll('input[data-dur]').forEach((inp) => {
    const hint = el(inp.id + 'Hint');
    if (!hint) return;
    const range = [+inp.dataset.lo, +inp.dataset.hi];
    const update = () => {
      const v = +inp.value;
      hint.textContent = durHint(v, range);
      hint.classList.toggle('err', !isFinite(v) || v < range[0] || v > range[1]);
    };
    inp.addEventListener('input', update);
    update();
  });
}

// ── Yapılandırmayı yükle ve formları kur ───────────────────────────────────

async function loadConfig() {
  try {
    const c = await api('/api/config');

    renderSafetyConfig(c);
    renderAutomationConfig(c);
    renderActuatorConfig(c.actuators || []);
    renderSystemConfig(c);
    bindDurHints();

    // Kurallar AYRI uç noktadan gelir: 8 kural tek başına ~1,7 KB tutuyor ve
    // config yanıtının tamponuna sığmıyordu (`RULES_JSON_MAX` gerekçesi).
    loadRules();
  } catch (e) {
    const msg = `<p class="err">${esc(e.message)}</p>`;
    ['safetyForm', 'automationForm', 'actuatorConfigForm', 'systemForm'].forEach((id) => {
      if (el(id)) el(id).innerHTML = msg;
    });
  }
}

function renderSafetyConfig(c) {
  el('safetyForm').innerHTML = `
    ${durField('cfgFlowDelay', 'Akış Doğrulama Gecikmesi', c.safety.flowVerifyDelayMs,
      LIMITS.flowVerifyDelayMs,
      'Pompa açıldıktan sonra debinin oluşması için tanınan süre. Süre dolduğunda debi eşiğin altındaysa KURU ÇALIŞMA mandalı düşer.')}
    ${numField('cfgFlowMin', 'Asgari Debi Eşiği', c.safety.flowMinRate, 'L/dk',
      LIMITS.flowMinRate, '0.01',
      'Bu debinin altı kuru çalışma sayılır. Akış katsayısı donanımda doğrulanmadan eşiği sıkılaştırmayın (ISSUE-014).')}
    ${durField('cfgGrace', 'Azami Süre Toleransı', c.safety.maxRuntimeGraceMs,
      LIMITS.maxRuntimeGraceMs,
      'Aktüatörün azami çalışma süresi aşıldığında tanınan ek pay.')}
    ${numField('cfgViol', 'Acil Duruma Geçiş İhlal Sayısı', c.safety.maxRuntimeViolations, 'kez',
      LIMITS.maxRuntimeViolations, '1',
      'Azami süre bu kadar kez aşılırsa sistem acil duruma geçer ve mandal NVS’te kalıcı olur.')}
    ${checkbox('cfgReqLevel', 'Su seviyesi sensörü okunamıyorsa pompa kilitli kalsın',
      c.safety.requireLevelSensor)}
    <button id="saveSafety" class="btn btn-primary" style="margin-top:10px;">Güvenlik Ayarlarını Kaydet</button>
    <p id="safetyMsg" class="form-feedback"></p>
  `;

  el('saveSafety').onclick = () => saveSection('/api/config/safety', 'safetyMsg', {
    flowVerifyDelayMs: +el('cfgFlowDelay').value,
    flowMinRate: +el('cfgFlowMin').value,
    maxRuntimeGraceMs: +el('cfgGrace').value,
    maxRuntimeViolations: +el('cfgViol').value,
    requireLevelSensor: el('cfgReqLevel').checked,
  });
}

function renderAutomationConfig(c) {
  // Cihaz otomasyon bölümünü döndürmüyorsa (eski firmware) formu kurmak
  // yerine bunu SÖYLERİZ — boş bir form, kullanıcıya var olmayan bir ayarı
  // değiştirdiğini düşündürür.
  if (!c.automation) {
    el('automationForm').innerHTML =
      '<p class="err">Cihaz otomasyon yapılandırmasını döndürmüyor — firmware güncel mi?</p>';
    txt(el('autoBadge'), '—');
    return;
  }

  const a = c.automation;
  let isAuto = a.mode === 'auto';

  const paintBadge = () => {
    const badge = el('autoBadge');
    txt(badge, isAuto ? 'OTOMATİK' : 'MANUEL');
    badge.className = 'section-badge' + (isAuto ? ' badge-auto' : '');
  };
  paintBadge();

  el('automationForm').innerHTML = `
    <div class="warn-note">
      <b>OTOMATİK moda almadan önce:</b> pompanın yalnızca güvenlik izniyle
      çalıştığı donanımda kanıtlanmış olmalıdır
      (M4 kapısı — <code>docs/HARDWARE_TEST_PROCEDURE.md §2</code>).
      Mod değişikliği tek başına sulama başlatmaz: kural kümesi boşken
      otomasyon hiçbir aktüatörü sürmez.
    </div>
    ${selectField('cfgAutoMode', 'Çalışma Modu', [
      ['manual', 'MANUEL — yalnızca operatör komutu'],
      ['auto', 'OTOMATİK — kural motoru sürer'],
    ], a.mode)}
    ${durField('cfgOverride', 'Manuel Müdahale Süresi', a.manualOverrideMs,
      LIMITS.manualOverrideMs,
      'OTOMATİK modda operatör bir aktüatöre müdahale ederse otomasyon O AKTÜATÖR için bu süre boyunca susar, sonra kontrolü geri alır.')}
    <button id="saveAutomation" class="btn btn-primary" style="margin-top:10px;">Otomasyon Ayarlarını Kaydet</button>
    <p id="automationMsg" class="form-feedback"></p>
  `;

  el('saveAutomation').onclick = () => {
    const mode = el('cfgAutoMode').value;

    // MANUEL → OTOMATİK tek yönlü bir risk artışıdır: bu andan sonra
    // aktüatörler operatör komutu olmadan da sürülebilir. Onay istemek,
    // yanlışlıkla değişen bir açılır liste değerinin sonucu olmasını önler.
    if (mode === 'auto' && !isAuto && !confirm(
      'OTOMATİK moda geçiliyor.\n\n' +
      'Bu moddan sonra kural motoru aktüatörleri operatör onayı olmadan ' +
      'sürebilir. Güvenlik kilitleri devrede kalır, ancak M4 donanım ' +
      'doğrulaması tamamlanmadıysa bu adımı atmayın.\n\nDevam edilsin mi?')) {
      return;
    }

    saveSection('/api/config/automation', 'automationMsg', {
      mode: mode,
      manualOverrideMs: +el('cfgOverride').value,
    }, () => { isAuto = (mode === 'auto'); paintBadge(); });
  };
}

function renderActuatorConfig(list) {
  const host = el('actuatorConfigForm');
  if (!list.length) {
    host.innerHTML =
      '<div class="card"><p class="muted">Cihaz aktüatör yapılandırması döndürmedi.</p></div>';
    return;
  }

  host.innerHTML = list.map((a, i) => {
    const meta = ACT_META[a.id] || { name: a.id, desc: '' };
    return `
      <div class="card form-card cfg-act-card">
        <div class="cfg-act-head">
          <div>
            <h3>${esc(meta.name)}</h3>
            <p class="muted small">${esc(meta.desc)}</p>
          </div>
          <span class="badge ${a.enabled ? 'ok' : 'dim'}" id="cfgActBadge${i}">${a.enabled ? 'ETKİN' : 'DEVRE DIŞI'}</span>
        </div>
        ${checkbox('cfgActEn' + i, 'Bu aktüatör kullanılabilir', a.enabled)}
        ${durField('cfgActMin' + i, 'Asgari Çalışma Süresi', a.minRunMs, LIMITS.minRunMs,
          'Açıldıktan sonra en az bu kadar çalışır — röle titremesini önler.')}
        ${durField('cfgActMax' + i, 'Azami Çalışma Süresi', a.maxRunMs, LIMITS.maxRunMs,
          'Bu süreyi aşarsa güvenlik zinciri aktüatörü kapatır.')}
        ${durField('cfgActCd' + i, 'Bekleme Süresi (Cooldown)', a.cooldownMs, LIMITS.cooldownMs,
          'Kapandıktan sonra bu süre dolmadan yeniden açılmaz.')}
        <button id="saveAct${i}" class="btn btn-primary btn-sm">Kaydet</button>
        <p id="actMsg${i}" class="form-feedback"></p>
      </div>`;
  }).join('');

  list.forEach((a, i) => {
    el('saveAct' + i).onclick = () => {
      const minMs = +el('cfgActMin' + i).value;
      const maxMs = +el('cfgActMax' + i).value;

      // ALANLAR ARASI KURAL: cihaz da reddeder (`validateActuator`), ama
      // sebebini ağ turu beklemeden söylemek kullanıcıyı "neden olmadı"
      // sorusuyla baş başa bırakmaz.
      if (minMs >= maxMs) {
        const m = el('actMsg' + i);
        m.className = 'form-feedback err';
        txt(m, 'Asgari süre azami süreden küçük olmalıdır — aksi hâlde aktüatör ne açılabilir ne de kapanabilir.');
        return;
      }

      const enabled = el('cfgActEn' + i).checked;

      saveSection('/api/config/actuators', 'actMsg' + i, {
        index: i,
        enabled: enabled,
        minRunMs: minMs,
        maxRunMs: maxMs,
        cooldownMs: +el('cfgActCd' + i).value,
      }, () => {
        const badge = el('cfgActBadge' + i);
        badge.className = 'badge ' + (enabled ? 'ok' : 'dim');
        txt(badge, enabled ? 'ETKİN' : 'DEVRE DIŞI');
      });
    };
  });
}

function renderSystemConfig(c) {
  el('systemForm').innerHTML = `
    ${field('cfgTz', 'Zaman Dilimi (POSIX TZ)', c.system.timezone, 'text')}
    <span class="field-hint dim">Örnek: <code>TRT-3</code> — yaz saati kuralları da bu dizede yazılır.</span>
    ${durField('cfgTelem', 'Telemetri Periyodu', c.system.telemetryIntervalMs,
      LIMITS.telemetryIntervalMs,
      'Cihazın WebSocket üzerinden durum yayınlama hız sınırı.')}
    ${selectField('cfgLog', 'Seri Port Kayıt Seviyesi', LOG_LEVELS, c.system.logLevel,
      'Bu seviyenin altındaki olaylar seri porta yazılmaz.')}
    <button id="saveSystem" class="btn btn-primary" style="margin-top:10px;">Sistem Ayarlarını Kaydet</button>
    <p id="systemMsg" class="form-feedback"></p>
  `;

  el('saveSystem').onclick = () => saveSection('/api/config/system', 'systemMsg', {
    timezone: el('cfgTz').value,
    telemetryIntervalMs: +el('cfgTelem').value,
    logLevel: +el('cfgLog').value,
  });
}

/// Bir bölümü kaydeder. Hata ALAN ADIYLA gelirse onu da gösterir — "0x0204"
/// tek başına kullanıcıya hangi kutuyu düzelteceğini söylemez.
async function saveSection(path, msgId, body, after) {
  const m = el(msgId);
  m.className = 'form-feedback';
  txt(m, 'Kaydediliyor…');

  try {
    await api(path, { method: 'PUT', body: JSON.stringify(body) });
    m.className = 'form-feedback msg';
    txt(m, '✔ Değişiklikler cihaza yazıldı.');
    // Formu yeniden ÇİZMİYORUZ: yeniden çizim bu mesajı siler ve kullanıcı
    // kaydın olup olmadığını göremez. Değişen rozet yerinde güncellenir.
    if (after) after();
  } catch (e) {
    m.className = 'form-feedback err';
    const label = e.field ? (FIELD_TEXT[e.field] || e.field) : '';
    txt(m, label ? label + ': ' + e.message : e.message);
  }
}


// ── Otomasyon Kuralları (ISSUE-021) ────────────────────────────────────────
//
// Kural kümesi cihaza BÜTÜN olarak yazılır (`PUT /api/config/rules`): çakışma
// denetimi yalnızca küme bütününde anlamlı. Bu yüzden düzenleme yerel bir
// model üzerinde yapılır, "Kaydet" tek istekte tümünü gönderir.
//
// `kind` değiştiğinde diğer türün alanları KORUNUR — cihazdaki `Rule` yapısı
// da `union` değil (bkz. `core/Rule.h`). Kullanıcı türü yanlışlıkla
// değiştirip geri döndüğünde eşiklerini kaybetmez.

const MAX_RULES = 8;

const RULE_KINDS = [
  ['threshold', 'Eşik — sensör değerine göre'],
  ['window',    'Zaman Penceresi — gün içi saat aralığı'],
  ['cycle',     'Periyodik Çevrim — N sn açık / P sn periyot'],
];

const RULE_KIND_SHORT = { threshold: 'EŞİK', window: 'PENCERE', cycle: 'ÇEVRİM', inactive: 'BOŞ' };

const actOptions    = () => Object.keys(ACT_META).map((k) => [k, ACT_META[k].name]);
const sensorOptions = () => Object.keys(SENSOR_META).map((k) => [k, SENSOR_META[k].label]);

/// Yeni kural GÜVENLİ doğar: `enabled = false`. Eklenip kaydedilen bir kural,
/// operatör bilinçli olarak etkinleştirmeden hiçbir aktüatörü sürmez.
const newRule = () => ({
  kind: 'threshold', target: 'waterPump', enabled: false, priority: 10,
  minTriggerIntervalS: 60,
  sensor: 'ec', onThreshold: 1.2, offThreshold: 1.6,
  startMin: 360, endMin: 1080,
  cycleOnS: 900, cyclePeriodS: 7200,
});

/// Cihazdan gelen kaydı tamamlar: eksik alan varsayılana düşer, böylece tür
/// değiştirildiğinde boş kutu çıkmaz.
function normalizeRule(r) {
  const d = newRule();
  const o = Object.assign(d, r || {});
  o.enabled = !!o.enabled;
  return o;
}

const pad2 = (n) => String(n).padStart(2, '0');
const minToTime = (m) => pad2(Math.floor((+m || 0) / 60)) + ':' + pad2((+m || 0) % 60);

function timeToMin(s) {
  const parts = String(s || '').split(':');
  const h = +parts[0], m = +parts[1];
  if (!isFinite(h) || !isFinite(m)) return 0;
  return ((h * 60 + m) % 1440 + 1440) % 1440;
}

/// Saniyeyi insan ölçüsüne çevirir (çevrim alanları saniye taşır).
const fmtSec = (s) => fmtDur((+s || 0) * 1000);

/// Kuralın ne yaptığını TEK CÜMLEDE söyler. Eşik yönü iki eşikten türer
/// (`Rule.h`); kullanıcının bunu kutulara bakıp çıkarması beklenemez.
function ruleSummary(r) {
  const act = (ACT_META[r.target] || { name: r.target }).name;

  if (r.kind === 'threshold') {
    const meta = SENSOR_META[r.sensor] || { label: r.sensor, unit: '' };
    const u = meta.unit ? ' ' + meta.unit : '';
    const dir = (+r.onThreshold < +r.offThreshold) ? 'ALTINA düşünce' : 'ÜSTÜNE çıkınca';
    const back = (+r.onThreshold < +r.offThreshold) ? 'yükselince' : 'düşünce';
    return `${meta.label} ${r.onThreshold}${u} ${dir} <b>${act}</b> açılır, ` +
           `${r.offThreshold}${u} değerine ${back} kapanır.`;
  }

  if (r.kind === 'window') {
    const wrap = (+r.startMin > +r.endMin)
      ? ' <span class="warn-inline">(gece yarısını aşar)</span>' : '';
    return `Her gün ${minToTime(r.startMin)} – ${minToTime(r.endMin)} arasında ` +
           `<b>${act}</b> açık.${wrap}`;
  }

  return `Her ${fmtSec(r.cyclePeriodS)} içinde ${fmtSec(r.cycleOnS)} boyunca ` +
         `<b>${act}</b> açık.`;
}

// ── Çizim ──────────────────────────────────────────────────────────────────

const ruleSelect = (i, key, label, options, current) => `
  <div class="form-group">
    <label>${label}</label>
    <select data-ri="${i}" data-rk="${key}">
      ${options.map(([v, t]) =>
        `<option value="${esc(v)}"${String(v) === String(current) ? ' selected' : ''}>${t}</option>`
      ).join('')}
    </select>
  </div>
`;

const ruleNum = (i, key, label, val, step, unit, note) => `
  <div class="form-group">
    <label>${label}</label>
    <div class="field-row">
      <input type="number" step="${step}" value="${esc(val)}" data-ri="${i}" data-rk="${key}">
      <span class="field-unit">${unit || ''}</span>
    </div>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>
`;

const ruleTime = (i, key, label, minutes) => `
  <div class="form-group">
    <label>${label}</label>
    <input type="time" value="${minToTime(minutes)}" data-ri="${i}" data-rk="${key}">
  </div>
`;

function ruleKindFields(r, i) {
  if (r.kind === 'threshold') {
    const meta = SENSOR_META[r.sensor] || { label: r.sensor, unit: '', digits: 2 };
    const step = meta.digits === 0 ? '1' : (meta.digits === 1 ? '0.1' : '0.01');
    return `
      ${ruleSelect(i, 'sensor', 'Sensör', sensorOptions(), r.sensor)}
      ${ruleNum(i, 'onThreshold', 'Açma Eşiği', r.onThreshold, step, meta.unit)}
      ${ruleNum(i, 'offThreshold', 'Kapatma Eşiği', r.offThreshold, step, meta.unit,
        'Yön İKİ EŞİKTEN türer: açma &lt; kapatma ise değer düşünce açılır, açma &gt; kapatma ise değer yükselince açılır. Eşit olamaz — histerezis bandı sıfır olur.')}
      ${thresholdRangeWarning(r)}
    `;
  }

  if (r.kind === 'window') {
    return `
      ${ruleTime(i, 'startMin', 'Başlangıç Saati', r.startMin)}
      ${ruleTime(i, 'endMin', 'Bitiş Saati', r.endMin)}
      <span class="field-hint dim">Gece yarısını aşan pencere geçerlidir (22:00 – 02:00). Çizelgeler cihaz saatinin geçerli olmasını gerektirir.</span>
    `;
  }

  return `
    ${ruleNum(i, 'cycleOnS', 'Açık Kalma Süresi', r.cycleOnS, '1', 'sn', fmtSec(r.cycleOnS))}
    ${ruleNum(i, 'cyclePeriodS', 'Çevrim Periyodu', r.cyclePeriodS, '1', 'sn',
      fmtSec(r.cyclePeriodS) + ' — açık kalma süresi periyottan KISA olmalı')}
  `;
}

/// Eşiğin sensörün bilinen aralığı dışında olması, kuralın hiç
/// tetiklenmemesiyle sonuçlanır. Cihaz bunu kendi `validRange`'ine göre
/// reddeder; burada yalnızca UYARIRIZ — arayüzdeki aralık gösterim amaçlıdır,
/// cihazın kalibrasyon aralığı değildir.
function thresholdRangeWarning(r) {
  const meta = SENSOR_META[r.sensor];
  if (!meta) return '';
  let html = '';

  const out = [r.onThreshold, r.offThreshold].some(
    (v) => +v < meta.min || +v > meta.max);
  if (out) {
    html += `<span class="field-hint err">Eşiklerden biri sensörün beklenen aralığı (${meta.min} – ${meta.max}) dışında; cihaz bu kuralı reddedebilir.</span>`;
  }

  // Takılı olmayan bir sensöre bağlı kural DOĞRULAMADAN GEÇER ama hiç
  // tetiklenmez — kullanıcı kuralın neden çalışmadığını anlayamaz. Canlı
  // durumda sensörün yokluğunu görüyorsak söyleriz.
  const live = store.state && store.state.sensors
    ? store.state.sensors.find((sn) => sn.id === r.sensor) : null;
  if (live && (live.quality === 'notPresent' || live.quality === 'fault')) {
    html += `<span class="field-hint err">${meta.label} şu an “${qualityText(live.quality)}” — bu kural cihazda hiç tetiklenmez.</span>`;
  }

  return html;
}

function ruleCard(r, i) {
  return `
    <div class="card form-card rule-card${r.enabled ? '' : ' rule-off'}">
      <div class="rule-head">
        <div class="rule-title">
          <span class="badge ${r.enabled ? 'ok' : 'dim'}">${RULE_KIND_SHORT[r.kind] || '—'}</span>
          <h3>Kural ${i + 1}</h3>
        </div>
        <button class="btn btn-outline-danger btn-sm" data-del="${i}">Sil</button>
      </div>

      <p class="rule-summary">${ruleSummary(r)}</p>

      ${checkboxRule(i, 'enabled', 'Bu kural etkin', r.enabled)}
      ${ruleSelect(i, 'kind', 'Kural Türü', RULE_KINDS, r.kind)}
      ${ruleSelect(i, 'target', 'Hedef Aktüatör', actOptions(), r.target)}
      ${ruleKindFields(r, i)}
      ${ruleNum(i, 'priority', 'Öncelik', r.priority, '1', '',
        'Aynı aktüatörü hedefleyen kurallarda BÜYÜK olan kazanır. İki etkin kural aynı hedefe aynı önceliği veremez.')}
      ${ruleNum(i, 'minTriggerIntervalS', 'Asgari Tetikleme Aralığı', r.minTriggerIntervalS, '1', 'sn',
        'Kuralın bu süreden sık tetiklenmesini engeller — histerezisten ayrı, hızlı salınıma karşı ikinci koruma.')}
    </div>
  `;
}

const checkboxRule = (i, key, label, on) => `
  <div class="form-group" style="margin-top:10px;">
    <label style="display:flex; align-items:center; cursor:pointer;">
      <input type="checkbox" ${on ? 'checked' : ''} data-ri="${i}" data-rk="${key}">
      <span>${label}</span>
    </label>
  </div>
`;

function renderRules() {
  const host = el('rulesList');
  const rules = store.rules;

  txt(el('rulesBadge'), rules.length + ' / ' + MAX_RULES + ' kural');

  host.innerHTML = rules.length
    ? rules.map((r, i) => ruleCard(r, i)).join('')
    : '<div class="card"><p class="muted">Tanımlı kural yok — OTOMATİK modda bile hiçbir aktüatör kendiliğinden çalışmaz.</p></div>';

  el('addRule').disabled = rules.length >= MAX_RULES;
  bindRuleInputs();
}

function bindRuleInputs() {
  document.querySelectorAll('#rulesList [data-ri]').forEach((inp) => {
    const apply = () => {
      const r = store.rules[+inp.dataset.ri];
      if (!r) return;
      const key = inp.dataset.rk;

      if (inp.type === 'checkbox')   r[key] = inp.checked;
      else if (inp.type === 'time')  r[key] = timeToMin(inp.value);
      else if (inp.type === 'number') r[key] = +inp.value;
      else                            r[key] = inp.value;

      // Tür/sensör değişimi kartın alanlarını değiştirir; gerisi yalnızca
      // özet cümleyi etkiler. Her tuş vuruşunda tüm listeyi yeniden çizmek
      // odağı kaybettirirdi.
      renderRules();
    };
    inp.onchange = apply;
  });

  document.querySelectorAll('#rulesList [data-del]').forEach((b) => {
    b.onclick = () => {
      store.rules.splice(+b.dataset.del, 1);
      renderRules();
      txt(el('rulesMsg'), 'Kural listeden çıkarıldı — değişiklik KAYDEDİLMEDİ.');
      el('rulesMsg').className = 'form-feedback';
    };
  });
}

// ── Yükleme ve kaydetme ────────────────────────────────────────────────────

async function loadRules() {
  try {
    const data = await api('/api/config/rules');
    store.rules = (data.rules || []).map(normalizeRule);
    renderRules();
    el('rulesMsg').className = 'form-feedback';
    txt(el('rulesMsg'), '');
  } catch (e) {
    el('rulesList').innerHTML = `<div class="card"><p class="err">${esc(e.message)}</p></div>`;
  }
}

/// Cihazın `validateRules` denetiminin İSTEMCİ KOPYASI. Cihaz her hâlükârda
/// yeniden doğrular; buradaki amaç, kullanıcıya hangi kuralın neden
/// reddedileceğini ağ turu beklemeden söylemek.
function validateRulesClient(rules) {
  for (let i = 0; i < rules.length; i++) {
    const r = rules[i];
    const n = i + 1;

    if (r.kind === 'threshold') {
      if (+r.onThreshold === +r.offThreshold) {
        return `Kural ${n}: açma ve kapatma eşiği eşit olamaz — histerezis bandı sıfır olur ve röle gürültüyle çırpınır.`;
      }
    } else if (r.kind === 'window') {
      if (+r.startMin === +r.endMin) {
        return `Kural ${n}: başlangıç ve bitiş saati aynı olamaz.`;
      }
    } else if (r.kind === 'cycle') {
      if (!(+r.cyclePeriodS > 0)) {
        return `Kural ${n}: çevrim periyodu sıfır olamaz.`;
      }
      if (!(+r.cycleOnS > 0) || +r.cycleOnS >= +r.cyclePeriodS) {
        return `Kural ${n}: açık kalma süresi periyottan kısa olmalıdır — eşit veya uzun olması hiç kapanmayan bir çevrim demektir.`;
      }
    }
  }

  for (let i = 0; i < rules.length; i++) {
    for (let j = i + 1; j < rules.length; j++) {
      const a = rules[i], b = rules[j];
      if (!a.enabled || !b.enabled) continue;
      if (a.target === b.target && +a.priority === +b.priority) {
        const act = (ACT_META[a.target] || { name: a.target }).name;
        return `Kural ${i + 1} ile ${j + 1} aynı aktüatörü (${act}) aynı öncelikle hedefliyor — hangisinin kazanacağı belirsiz. Önceliklerden birini değiştirin.`;
      }
    }
  }

  return null;
}

/// `rules[3].sensor` → "Kural 4 · Sensör". Cihaz alan adını indeksle
/// döndürüyor; ham hâliyle göstermek kullanıcıya bir şey anlatmaz.
function ruleFieldText(field) {
  const m = /^rules\[(\d+)\]\.(.+)$/.exec(field || '');
  const LEAF = {
    kind: 'kural türü', target: 'hedef aktüatör', sensor: 'sensör',
    threshold: 'eşikler', onThreshold: 'açma eşiği', offThreshold: 'kapatma eşiği',
    startMin: 'başlangıç saati', endMin: 'bitiş saati', window: 'zaman penceresi',
    cycleOnS: 'açık kalma süresi', cyclePeriodS: 'çevrim periyodu',
    cycle: 'çevrim', priority: 'öncelik', minTriggerIntervalS: 'tetikleme aralığı',
    object: 'kural kaydı',
  };
  if (m) return `Kural ${+m[1] + 1} · ${LEAF[m[2]] || m[2]}`;

  const SET = {
    'rules.count': 'Kural sayısı', 'rules.priority': 'Kural çakışması (öncelik)',
    'rules.target': 'Hedef aktüatör', 'rules.sensor': 'Sensör',
    'rules.threshold': 'Eşikler', 'rules.onThreshold': 'Açma eşiği',
    'rules.offThreshold': 'Kapatma eşiği', 'rules.window': 'Zaman penceresi',
    'rules.cycleOnS': 'Açık kalma süresi', 'rules.cyclePeriodS': 'Çevrim periyodu',
    'rules': 'Kural kümesi',
  };
  return SET[field] || field || '';
}

async function saveRules() {
  const m = el('rulesMsg');

  const problem = validateRulesClient(store.rules);
  if (problem) {
    m.className = 'form-feedback err';
    txt(m, problem);
    return;
  }

  m.className = 'form-feedback';
  txt(m, 'Kural kümesi yazılıyor…');

  try {
    await api('/api/config/rules', {
      method: 'PUT',
      body: JSON.stringify({ rules: store.rules }),
    });
    m.className = 'form-feedback msg';
    txt(m, '✔ Kural kümesi cihaza yazıldı. Kurallar yalnızca OTOMATİK modda değerlendirilir.');
  } catch (e) {
    m.className = 'form-feedback err';
    const label = ruleFieldText(e.field);
    txt(m, label ? label + ': ' + e.message : e.message);
  }
}

// ── Parola ve oturum ───────────────────────────────────────────────────────

/// Oturumu yerelde kapatır: token'ı düşürür, WS'i kapatır, giriş ekranına
/// döner. Cihaz tarafında oturum geçersizken arayüzün "bağlı" görünmesi,
/// kullanıcının çalışmayan bir panele bakması demektir.
function endSession(reason) {
  store.token = null;
  clearTimeout(reconnectTimer);
  if (ws) {
    try { ws.close(); } catch (e) { /* zaten kapalı */ }
    ws = null;
  }
  setLinked(false, reason);
  showLogin(reason, 'msg');
}

async function changePassword() {
  const m = el('pwMsg');
  const cur = el('pwCurrent').value;
  const next = el('pwNext').value;
  const again = el('pwNext2').value;

  m.className = 'form-feedback err';
  if (!cur) { txt(m, 'Mevcut parolayı girin.'); return; }
  if (next.length < 8) { txt(m, 'Yeni parola en az 8 karakter olmalıdır.'); return; }
  if (next !== again) { txt(m, 'Yeni parolalar eşleşmiyor.'); return; }
  if (next === cur) { txt(m, 'Yeni parola mevcut paroladan farklı olmalıdır.'); return; }

  m.className = 'form-feedback';
  txt(m, 'Parola değiştiriliyor…');

  try {
    await api('/api/auth/password', {
      method: 'POST',
      body: JSON.stringify({ current: cur, next: next }),
    });
  } catch (e) {
    m.className = 'form-feedback err';
    if (e.code === 0x0901)      txt(m, 'Mevcut parola hatalı.');
    else if (e.code === 0x0204) txt(m, 'Yeni parola cihaz tarafından reddedildi (en az 8 karakter).');
    else                        txt(m, e.message);
    return;
  }

  // Cihaz parola değişiminde TÜM oturumları düşürür. Elimizdeki token artık
  // ölü: WS'in 1008 ile kapanmasını beklemeden oturumu biz sonlandırıyoruz.
  el('pwCurrent').value = '';
  el('pwNext').value = '';
  el('pwNext2').value = '';
  txt(m, '');
  endSession('Parola değiştirildi — yeni parolanızla giriş yapın.');
}

async function doLogout() {
  const t = store.token;
  try {
    if (t) await api('/api/auth/logout?token=' + encodeURIComponent(t), { method: 'POST' });
  } catch (e) {
    // Cihaz yanıt vermese de yerel oturumu düşürürüz: token'ı elde tutmanın
    // faydası yok, TTL dolduğunda zaten geçersiz olacak.
  }
  endSession('Oturum kapatıldı.');
}

// ── Wi-Fi Ağ Tarayıcı ─────────────────────────────────────────────────────

let scanPoll = null;

async function startScan() {
  clearInterval(scanPoll);
  try {
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
  } catch (e) {
    clearInterval(scanPoll);
  }
}

function renderScan(r) {
  const st = {
    idle: 'Hazır',
    running: 'Ağlar Taranıyor…',
    done: 'Tarama Tamamlandı',
    failed: 'Tarama Başarısız'
  }[r.status] || r.status;

  txt(el('scanStatus'), st + (r.truncated ? ' (liste sınırlandı)' : ''));

  const list = el('scanList');
  if (!list) return;

  list.innerHTML = r.networks
    .sort((a, b) => b.rssi - a.rssi)
    .map((n) => `
      <li class="scan-item" data-ssid="${n.ssid}">
        <span class="scan-ssid">
          ${n.open ? '🔓' : '🔒'} <b>${n.ssid}</b>
        </span>
        <span class="scan-meta">${n.rssi} dBm (Kanal ${n.channel})</span>
      </li>
    `).join('');

  list.querySelectorAll('li').forEach((li) => {
    li.onclick = () => {
      el('ssid').value = li.dataset.ssid;
      el('wifiPw').focus();
    };
  });
}

// ── Kimlik Doğrulama & Giriş ───────────────────────────────────────────────

let setupMode = false;

async function checkAuth() {
  try {
    const s = await api('/api/auth/status');
    setupMode = s.setupMode;
  } catch (e) {
    setupMode = false;
  }

  txt(el('loginHint'), setupMode
    ? 'İlk kurulum: Cihaz için bir yönetici parolası belirleyin (en az 8 karakter).'
    : 'Devam etmek için cihaz parolanızı girin.');
  show(el('pw2'), setupMode);
  show(el('pw2Label'), setupMode);
  el('loginBtn').textContent = setupMode ? 'Parolayı Belirle ve Başla' : 'Giriş Yap';
}

/// @param reason giriş ekranında gösterilecek açıklama (parola değişti,
///               oturum kapatıldı, oturum geçersiz…). Kullanıcı neden geri
///               atıldığını görmezse bunu bir arıza sanır.
/// @param tone   'msg' bilgilendirme, 'err' hata
function showLogin(reason, tone) {
  show(el('loginView'), true);
  show(el('appView'), false);
  checkAuth();

  const fb = el('loginErr');
  if (fb) {
    fb.className = 'form-feedback ' + (tone === 'err' ? 'err' : 'msg');
    txt(fb, reason || '');
  }
}

async function doLogin() {
  const e = el('loginErr');
  // Geri bildirim kutusu bilgi mesajı da taşıyabiliyor (parola değişti,
  // oturum kapatıldı); her denemede hata tonuna geri döndürülür.
  e.className = 'form-feedback err';
  txt(e, '');
  const pass = el('pw').value;

  try {
    if (setupMode) {
      if (pass.length < 8) { txt(e, 'Parola en az 8 karakter olmalıdır.'); return; }
      if (pass !== el('pw2').value) { txt(e, 'Girilen parolalar eşleşmiyor.'); return; }
      await api('/api/setup/password', { method: 'POST', body: JSON.stringify({ password: pass }) });
    }
    const r = await api('/api/auth/login', { method: 'POST', body: JSON.stringify({ password: pass }) });

    store.token = r.token;
    el('pw').value = ''; el('pw2').value = '';
    show(el('loginView'), false);
    show(el('appView'), true);
    connect();
  } catch (err) {
    txt(e, 'Giriş başarısız. Parolanızı kontrol edin.');
  }
}

// ── Uygulama Başlatma ─────────────────────────────────────────────────────

function init() {
  document.querySelectorAll('.tab-btn').forEach((t) => {
    t.onclick = () => switchView(t.dataset.view);
  });

  el('loginBtn').onclick = doLogin;
  el('pw').addEventListener('keydown', (e) => { if (e.key === 'Enter') doLogin(); });
  el('pw2').addEventListener('keydown', (e) => { if (e.key === 'Enter') doLogin(); });

  el('estopBtn').onclick = () => sendCmd('system', 'emergencyStop');
  el('eclearBtn').onclick = () => sendCmd('system', 'emergencyClear');

  el('scanBtn').onclick = startScan;
  el('refreshDiag').onclick = loadDiagnostics;

  if (el('chartRefreshBtn')) el('chartRefreshBtn').onclick = loadHistory;

  document.querySelectorAll('#chartSensors .chip-btn').forEach((btn) => {
    btn.onclick = () => {
      document.querySelectorAll('#chartSensors .chip-btn').forEach((b) => b.classList.remove('active'));
      btn.classList.add('active');
      store.activeChartSensor = btn.dataset.sensor;
      drawChart();
    };
  });

  el('saveWifi').onclick = async () => {
    const m = el('netMsg');
    try {
      await api('/api/config/network', {
        method: 'PUT',
        body: JSON.stringify({ ssid: el('ssid').value, password: el('wifiPw').value }),
      });
      el('wifiPw').value = '';
      m.className = 'form-feedback msg';
      txt(m, '✔ Wi-Fi bilgileri kaydedildi, bağlanılıyor…');
    } catch (e) {
      m.className = 'form-feedback err';
      txt(m, e.message);
    }
  };

  el('forgetWifi').onclick = async () => {
    if (!confirm('Kayıtlı ağ silinsin mi? Cihaz AP moduna dönecektir.')) return;
    try {
      await api('/api/network/forget', { method: 'POST' });
    } catch (e) {
      txt(el('netMsg'), e.message);
    }
  };

  el('retryNow').onclick = () => api('/api/network/retry', { method: 'POST' })
    .then(() => txt(el('netMsg'), 'Ağa yeniden bağlanma deneniyor…'))
    .catch((e) => txt(el('netMsg'), e.message));

  el('addRule').onclick = () => {
    if (store.rules.length >= MAX_RULES) return;
    store.rules.push(newRule());
    renderRules();
    el('rulesMsg').className = 'form-feedback';
    txt(el('rulesMsg'), 'Yeni kural eklendi — etkinleştirip KAYDEDENE kadar çalışmaz.');
  };
  el('saveRules').onclick = saveRules;
  el('reloadRules').onclick = loadRules;

  el('savePw').onclick = changePassword;
  el('logoutBtn').onclick = doLogout;
  ['pwCurrent', 'pwNext', 'pwNext2'].forEach((id) => {
    el(id).addEventListener('keydown', (ev) => { if (ev.key === 'Enter') changePassword(); });
  });

  el('factoryBtn').onclick = async () => {
    if (!confirm('DİKKAT: TÜM ayarlar, ağ bilgileri ve yönetici parolası silinecektir. Emin misiniz?')) return;
    try {
      await api('/api/system/factory-reset?confirm=FACTORY_RESET', { method: 'POST' });
      endSession('Cihaz fabrika ayarlarına döndürüldü — kurulum AP’sinden yeniden başlayın.');
    } catch (e) {
      alert(e.message);
    }
  };

  setLinked(false);
  showLogin();
}

document.addEventListener('DOMContentLoaded', init);

