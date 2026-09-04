/* Uzman ekranları — grafik, teşhis, kural düzenleyici, kısıtlar (TASK-070)
 *
 * Bu dosyadaki her şey BASİT modda gizlidir ama SİLİNMEMİŞTİR: kural
 * düzenleyici, aktüatör süre kısıtları, güvenlik eşikleri ve sistem
 * parametreleri önceki sürümdekiyle aynı yetkinlikte çalışır.
 */

// ── Geçerli aralıklar ──────────────────────────────────────────────────────
//
// TEK DOĞRULUK KAYNAĞI CİHAZDIR (`core/Config.h` → `namespace limits`).
// Buradaki kopya bir KOLAYLIKTIR: kullanıcı sınır dışı değeri göndermeden
// önce görsün diye. Cihaz her gövdeyi yeniden doğrular; bu tablo bayatlarsa
// istek REDDEDİLİR, sessizce kabul edilmez.

const LIMITS = {
  minRunMs:             [0, 600000],        // ACTUATOR_MIN_RUN     0 – 10 dk
  maxRunMs:             [1000, 7200000],    // ACTUATOR_MAX_RUN     1 sn – 2 sa
  cooldownMs:           [0, 3600000],       // ACTUATOR_COOLDOWN    0 – 1 sa
  flowVerifyDelayMs:    [1000, 60000],      // FLOW_VERIFY_DELAY    1 – 60 sn
  flowMinRate:          [0.01, 1000],       // FLOW_MIN_RATE        L/dk
  maxRuntimeGraceMs:    [0, 60000],         // MAX_RUNTIME_GRACE    0 – 60 sn
  maxRuntimeViolations: [1, 20],            // validateSafety       1 – 20 kez
  manualOverrideMs:     [60000, 86400000],  // MANUAL_OVERRIDE      1 dk – 24 sa
  telemetryIntervalMs:  [200, 60000],       // TELEMETRY_INTERVAL   200 ms – 60 sn
};

/// `maxRunMs` üst sınırı AKTÜATÖRE GÖRE değişir (`limits::maxRunLimitFor`).
/// Işık 20 saate kadar yanabilir, dozaj pompası 5 dakikayı geçemez. Tek bir
/// global sınır göstermek, kullanıcıya cihazın kabul edeceğinden farklı bir
/// aralık vaat etmek olurdu.
const MAX_RUN_LIMITS = {
  waterPump:    [1000, 7200000],   // 2 sa
  airPump:      [1000, 7200000],   // 2 sa
  growLight:    [1000, 72000000],  // 20 sa
  heater:       [1000, 21600000],  // 6 sa
  nutrientPump: [1000, 300000],    // 5 dk
};

const maxRunRange = (id) => MAX_RUN_LIMITS[id] || LIMITS.maxRunMs;

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
  'sensors.index': 'Sensör numarası',
  'sensors.validRange': 'Geçerli aralık',
  'sensors.scale': 'Ölçek',
  'sensors.filterStrength': 'Filtre gücü',
  'sensors.maxChangePerSec': 'Azami değişim hızı',
  'sensors.waterLevel.enabled': 'Su seviyesi sensörü (güvenlik kilidi açıkken kapatılamaz)',
  'actuators.index': 'Cihaz numarası',
  'actuators.minRunMs': 'Asgari çalışma süresi',
  'actuators.maxRunMs': 'Azami çalışma süresi',
  'actuators.cooldownMs': 'Bekleme süresi',
  'actuators.relayIndex': 'Röle eşlemesi',
  'automation.mode': 'Çalışma modu',
  'automation.manualOverrideMs': 'Manuel müdahale süresi',
  'crop.crop': 'Ürün',
  'crop.stage': 'Gelişim dönemi',
  'crop.intensity': 'Sulama yoğunluğu',
  'crop.plantedAtEpoch': 'Dikim tarihi',
  'system.timezone': 'Zaman dilimi',
  'system.telemetryIntervalMs': 'Telemetri periyodu',
  'system.logLevel': 'Kayıt seviyesi',
  'index': 'Cihaz numarası',
  'password': 'Parola',
};

const LOG_LEVELS = [
  [0, 'INFO — tüm olaylar'],
  [1, 'WARNING — uyarı ve üstü'],
  [2, 'ERROR — hata ve üstü'],
  [3, 'CRITICAL — yalnızca kritik'],
];

const durHint = (val, range) =>
  fmtDur(val) + '  ·  izin verilen: ' + fmtDur(range[0]) + ' – ' + fmtDur(range[1]);

// ── Form parçaları ─────────────────────────────────────────────────────────

const field = (id, label, val, type = 'number', step = 'any') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <input id="${id}" type="${type}" step="${step}" value="${esc(val)}">
  </div>`;

const numField = (id, label, val, unit, range, step = 'any', note = '') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <div class="field-row">
      <input id="${id}" type="number" step="${step}" min="${range[0]}" max="${range[1]}" value="${esc(val)}">
      <span class="field-unit">${unit}</span>
    </div>
    <span class="field-hint">izin verilen: ${range[0]} – ${range[1]} ${unit}</span>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>`;

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
  </div>`;

const checkbox = (id, label, on) => `
  <div class="form-group" style="margin-top:10px;">
    <label class="check-line">
      <input id="${id}" type="checkbox" ${on ? 'checked' : ''}>
      <span>${label}</span>
    </label>
  </div>`;

const selectField = (id, label, options, current, note = '') => `
  <div class="form-group">
    <label for="${id}">${label}</label>
    <select id="${id}">
      ${options.map(([v, t]) =>
        `<option value="${esc(v)}"${String(v) === String(current) ? ' selected' : ''}>${t}</option>`
      ).join('')}
    </select>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>`;

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

// ── Gelişmiş ekran: yapılandırma ───────────────────────────────────────────

async function loadConfig() {
  try {
    const c = await api('/api/config');
    store.config = c;

    renderSafetyConfig(c);
    renderSensorConfig(c.sensors || []);
    renderActuatorConfig(c.actuators || []);
    renderSystemConfig(c);
    bindDurHints();
    loadRules();
  } catch (e) {
    const msg = `<p class="err">${esc(e.message)}</p>`;
    ['safetyForm', 'sensorConfigForm', 'actuatorConfigForm', 'systemForm'].forEach((id) => {
      if (el(id)) el(id).innerHTML = msg;
    });
  }
}

// ── Sensör kalibrasyonu (ISSUE-035) ────────────────────────────────────────
//
// `offset`/`scale` ve `validRange` cihazda hep vardı ama API'de yoktu:
// ARCHITECTURE §9.3'ün anlattığı 2 nokta pH kalibrasyonunun karşılığı
// ulaşılamazdı.

/// Güvenlik zincirinin girdisi olan sensörler burada KAPATILAMAZ.
const SAFETY_SENSORS = ['level', 'flow'];

function renderSensorConfig(list) {
  const host = el('sensorConfigForm');
  if (!host) return;

  if (!list.length) {
    host.innerHTML =
      '<div class="card"><p class="muted">Cihaz sensör yapılandırması döndürmedi — firmware güncel mi?</p></div>';
    return;
  }

  host.innerHTML = list.map((s, i) => {
    const meta = SENSOR_META[s.id] || { label: s.id, unit: '' };
    const safety = SAFETY_SENSORS.indexOf(s.id) >= 0;
    const u = meta.unit || '';
    return `
      <div class="card form-card cfg-act-card">
        <div class="cfg-act-head">
          <div>
            <h3>${esc(meta.label)}</h3>
            <p class="muted small">${esc(SENSOR_DESC[s.id] || '')}</p>
          </div>
          <span class="badge ${s.enabled ? 'ok' : 'dim'}" id="cfgSensBadge${i}">${s.enabled ? 'TAKILI' : 'TAKILI DEĞİL'}</span>
        </div>

        ${safety
          ? `<p class="field-hint dim">Güvenlik zincirinin girdisi — kapatılamaz. Kapatmak, pompa kilidini sessizce devre dışı bırakırdı.</p>`
          : checkbox('cfgSensEn' + i, 'Bu sensör takılı ve okunsun', s.enabled)}

        ${numField('cfgSensScale' + i, 'Ölçek (scale)', s.scale, '×', [-1000, 1000], 'any',
          'İşlenmiş değer = ham × ölçek + kayma. Sıfır olamaz — sensörü sessizce öldürür.')}
        ${numField('cfgSensOff' + i, 'Kayma (offset)', s.offset, u, [-100000, 100000], 'any',
          'Saha trimi. pH probunda tampon çözelti okumasıyla farkın kapatılması.')}
        ${numField('cfgSensMin' + i, 'Geçerli aralık — alt', s.validRange.min, u,
          [-100000, 100000], 'any')}
        ${numField('cfgSensMax' + i, 'Geçerli aralık — üst', s.validRange.max, u,
          [-100000, 100000], 'any',
          'Fiziksel sınır, HEDEF aralık değil. Dışına çıkan ölçüm kullanılamaz sayılır.')}
        ${numField('cfgSensFilt' + i, 'Filtre gücü', s.filterStrength, 'örnek', [0, 32], '1',
          '0 = filtresiz. Güvenlik sensörleri filtresiz olmalıdır: gecikme kuru çalışma demektir.')}
        ${numField('cfgSensJump' + i, 'Azami değişim hızı', s.maxChangePerSec,
          u ? u + '/sn' : '/sn', [0, 100000], 'any',
          '0 = kapalı. Fiziksel olmayan ani sıçramaları eler.')}

        <button id="saveSens${i}" class="btn btn-primary btn-sm">Kaydet</button>
        <p id="sensMsg${i}" class="form-feedback"></p>
      </div>`;
  }).join('');

  list.forEach((s, i) => {
    el('saveSens' + i).onclick = () => {
      const m = el('sensMsg' + i);
      const lo = +el('cfgSensMin' + i).value;
      const hi = +el('cfgSensMax' + i).value;
      const scale = +el('cfgSensScale' + i).value;

      // Cihaz da reddeder; sebebini ağ turu beklemeden söylemek daha iyidir.
      if (!(lo < hi)) {
        m.className = 'form-feedback err';
        txt(m, 'Geçerli aralığın alt sınırı üst sınırdan küçük olmalıdır.');
        return;
      }
      if (scale === 0) {
        m.className = 'form-feedback err';
        txt(m, 'Ölçek sıfır olamaz — her ölçüm sabitlenir ve sensör sessizce ölür.');
        return;
      }

      const safety = SAFETY_SENSORS.indexOf(s.id) >= 0;
      const enabled = safety ? true : el('cfgSensEn' + i).checked;

      saveSection('/api/config/sensors', 'sensMsg' + i, {
        index: i,
        enabled: enabled,
        scale: scale,
        offset: +el('cfgSensOff' + i).value,
        filterStrength: +el('cfgSensFilt' + i).value,
        maxChangePerSec: +el('cfgSensJump' + i).value,
        validRange: { min: lo, max: hi },
      }, () => {
        const badge = el('cfgSensBadge' + i);
        badge.className = 'badge ' + (enabled ? 'ok' : 'dim');
        txt(badge, enabled ? 'TAKILI' : 'TAKILI DEĞİL');
        if (store.config && store.config.sensors && store.config.sensors[i]) {
          store.config.sensors[i].enabled = enabled;
        }
      });
    };
  });
}

function renderSafetyConfig(c) {
  el('safetyForm').innerHTML = `
    ${durField('cfgFlowDelay', 'Akış Doğrulama Gecikmesi', c.safety.flowVerifyDelayMs,
      LIMITS.flowVerifyDelayMs,
      'Pompa açıldıktan sonra debinin oluşması için tanınan süre. Süre dolduğunda debi eşiğin altındaysa KURU ÇALIŞMA kilidi düşer.')}
    ${numField('cfgFlowMin', 'Asgari Debi Eşiği', c.safety.flowMinRate, 'L/dk',
      LIMITS.flowMinRate, '0.01',
      'Bu debinin altı kuru çalışma sayılır. Akış katsayısı donanımda doğrulanmadan eşiği sıkılaştırmayın (ISSUE-014).')}
    ${durField('cfgGrace', 'Azami Süre Toleransı', c.safety.maxRuntimeGraceMs,
      LIMITS.maxRuntimeGraceMs, 'Azami çalışma süresi aşıldığında tanınan ek pay.')}
    ${numField('cfgViol', 'Acil Duruma Geçiş İhlal Sayısı', c.safety.maxRuntimeViolations, 'kez',
      LIMITS.maxRuntimeViolations, '1',
      'Azami süre bu kadar kez aşılırsa sistem acil duruma geçer ve kilit kalıcı olur.')}
    ${checkbox('cfgReqLevel', 'Su seviyesi sensörü okunamıyorsa pompa kilitli kalsın',
      c.safety.requireLevelSensor)}
    <button id="saveSafety" class="btn btn-primary" style="margin-top:10px;">Güvenlik Ayarlarını Kaydet</button>
    <p id="safetyMsg" class="form-feedback"></p>`;

  el('saveSafety').onclick = () => saveSection('/api/config/safety', 'safetyMsg', {
    flowVerifyDelayMs: +el('cfgFlowDelay').value,
    flowMinRate: +el('cfgFlowMin').value,
    maxRuntimeGraceMs: +el('cfgGrace').value,
    maxRuntimeViolations: +el('cfgViol').value,
    requireLevelSensor: el('cfgReqLevel').checked,
  });
}

function renderActuatorConfig(list) {
  const host = el('actuatorConfigForm');
  if (!list.length) {
    host.innerHTML = '<div class="card"><p class="muted">Cihaz yapılandırması alınamadı.</p></div>';
    return;
  }

  host.innerHTML = list.map((a, i) => {
    const meta = ACT_META[a.id] || { name: a.id, desc: '', icon: '' };
    const mr = maxRunRange(a.id);
    return `
      <div class="card form-card cfg-act-card">
        <div class="cfg-act-head">
          <div>
            <h3>${meta.icon || ''} ${esc(meta.name)}</h3>
            <p class="muted small">${esc(meta.desc)}</p>
          </div>
          <span class="badge ${a.enabled ? 'ok' : 'dim'}" id="cfgActBadge${i}">${a.enabled ? 'BAĞLI' : 'BAĞLI DEĞİL'}</span>
        </div>
        ${checkbox('cfgActEn' + i, 'Bu cihaz kablolu ve kullanılabilir', a.enabled)}
        ${durField('cfgActMin' + i, 'Asgari Çalışma Süresi', a.minRunMs, LIMITS.minRunMs,
          'Açıldıktan sonra en az bu kadar çalışır — röle titremesini önler.')}
        ${durField('cfgActMax' + i, 'Azami Çalışma Süresi', a.maxRunMs, mr,
          'Bu süreyi aşarsa güvenlik zinciri cihazı kapatır. Üst sınır cihazın rolüne göre değişir.')}
        ${durField('cfgActCd' + i, 'Bekleme Süresi', a.cooldownMs, LIMITS.cooldownMs,
          a.id === 'nutrientPump'
            ? 'Dozajdan sonra besinin karışması için beklenir. KISALTMAYIN: aşırı gübreleme koruması budur.'
            : 'Kapandıktan sonra bu süre dolmadan yeniden açılmaz.')}
        <button id="saveAct${i}" class="btn btn-primary btn-sm">Kaydet</button>
        <p id="actMsg${i}" class="form-feedback"></p>
      </div>`;
  }).join('');

  list.forEach((a, i) => {
    el('saveAct' + i).onclick = () => {
      const minMs = +el('cfgActMin' + i).value;
      const maxMs = +el('cfgActMax' + i).value;

      // ALANLAR ARASI KURAL: cihaz da reddeder, ama sebebini ağ turu
      // beklemeden söylemek kullanıcıyı "neden olmadı" sorusuyla bırakmaz.
      if (minMs >= maxMs) {
        const m = el('actMsg' + i);
        m.className = 'form-feedback err';
        txt(m, 'Asgari süre azami süreden küçük olmalıdır — aksi hâlde cihaz ne açılabilir ne kapanabilir.');
        return;
      }

      const enabled = el('cfgActEn' + i).checked;
      saveSection('/api/config/actuators', 'actMsg' + i, {
        index: i, enabled: enabled, minRunMs: minMs, maxRunMs: maxMs,
        cooldownMs: +el('cfgActCd' + i).value,
      }, () => {
        const badge = el('cfgActBadge' + i);
        badge.className = 'badge ' + (enabled ? 'ok' : 'dim');
        txt(badge, enabled ? 'BAĞLI' : 'BAĞLI DEĞİL');
        if (store.config && store.config.actuators && store.config.actuators[i]) {
          store.config.actuators[i].enabled = enabled;
        }
      });
    };
  });
}

function renderSystemConfig(c) {
  el('systemForm').innerHTML = `
    ${field('cfgTz', 'Zaman Dilimi (POSIX TZ)', c.system.timezone, 'text')}
    <span class="field-hint dim">Türkiye için: <code>EET-2EEST,M3.5.0/3,M10.5.0/4</code></span>
    ${durField('cfgTelem', 'Telemetri Periyodu', c.system.telemetryIntervalMs,
      LIMITS.telemetryIntervalMs, 'Cihazın durum yayınlama hız sınırı.')}
    ${selectField('cfgLog', 'Seri Port Kayıt Seviyesi', LOG_LEVELS, c.system.logLevel,
      'Bu seviyenin altındaki olaylar seri porta yazılmaz.')}
    ${durField('cfgOverride', 'Manuel Müdahale Süresi',
      (c.automation && c.automation.manualOverrideMs) || 900000, LIMITS.manualOverrideMs,
      'OTOMATİK modda bir cihaza elle müdahale ederseniz otomasyon O CİHAZ için bu süre susar, sonra kontrolü geri alır.')}
    <button id="saveSystem" class="btn btn-primary" style="margin-top:10px;">Sistem Ayarlarını Kaydet</button>
    <p id="systemMsg" class="form-feedback"></p>`;

  // ── İKİ İSTEK, TEK SONUÇ ─────────────────────────────────────────────────
  // Bu form iki ayrı bölüme yazar (sistem + otomasyon). Her birini
  // `saveSection` ile ayrı ayrı yazdırmak, ikincisinin mesajının birincisinin
  // ÜZERİNE yazmasına yol açıyordu: sistem kaydı başarısız olup otomasyon
  // kaydı başarılı olduğunda kullanıcı "✔ yazıldı" görüyordu (TASK-072).
  //
  // Artık ikisi de burada toplanır ve TEK bir sonuç raporlanır.
  el('saveSystem').onclick = async () => {
    const m = el('systemMsg');
    m.className = 'form-feedback';
    txt(m, 'Kaydediliyor…');

    const jobs = [
      ['Sistem ayarları', '/api/config/system', {
        timezone: el('cfgTz').value,
        telemetryIntervalMs: +el('cfgTelem').value,
        logLevel: +el('cfgLog').value,
      }],
      ['Manuel müdahale süresi', '/api/config/automation', {
        manualOverrideMs: +el('cfgOverride').value,
      }],
    ];

    const failures = [];
    for (const [label, path, body] of jobs) {
      try {
        await api(path, { method: 'PUT', body: JSON.stringify(body) });
      } catch (e) {
        const field = e.field ? (FIELD_TEXT[e.field] || e.field) : '';
        failures.push(`${label}${field ? ' · ' + field : ''}: ${e.message}`);
      }
    }

    if (failures.length === 0) {
      m.className = 'form-feedback msg';
      txt(m, '✔ Değişiklikler cihaza yazıldı.');
    } else {
      m.className = 'form-feedback err';
      // Kısmi başarı da AÇIKÇA söylenir: neyin yazıldığı belirsiz kalmamalı.
      txt(m, (failures.length === jobs.length ? 'Hiçbiri kaydedilemedi — ' : 'Kısmen kaydedildi — ') +
             failures.join(' | '));
    }
  };
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
    // kaydın olup olmadığını göremez.
    if (after) after();
  } catch (e) {
    m.className = 'form-feedback err';
    const label = e.field ? (FIELD_TEXT[e.field] || e.field) : '';
    txt(m, label ? label + ': ' + e.message : e.message);
  }
}

// ── Otomasyon kuralları ────────────────────────────────────────────────────
//
// Kural kümesi cihaza BÜTÜN olarak yazılır: çakışma denetimi yalnızca küme
// bütününde anlamlı. Bu yüzden düzenleme yerel bir model üzerinde yapılır,
// "Kaydet" tek istekte tümünü gönderir.

const MAX_RULES = 8;

const RULE_KINDS = [
  ['threshold', 'Eşik — ölçüme göre'],
  ['window',    'Zaman Penceresi — gün içi saat aralığı'],
  ['cycle',     'Periyodik Çevrim — N sn açık / P sn periyot'],
];

const RULE_KIND_SHORT = { threshold: 'EŞİK', window: 'PENCERE', cycle: 'ÇEVRİM', inactive: 'BOŞ' };

const actOptions    = () => Object.keys(ACT_META).map((k) => [k, ACT_META[k].name]);
const sensorOptions = () => Object.keys(SENSOR_META).map((k) => [k, SENSOR_META[k].label]);

/// Yeni kural GÜVENLİ doğar: `enabled = false`.
const newRule = () => ({
  kind: 'threshold', target: 'waterPump', enabled: false, priority: 10,
  minTriggerIntervalS: 60,
  sensor: 'ec', onThreshold: 1.2, offThreshold: 1.6,
  startMin: 360, endMin: 1080,
  cycleOnS: 120, cyclePeriodS: 1800,
});

function normalizeRule(r) {
  const o = Object.assign(newRule(), r || {});
  o.enabled = !!o.enabled;
  return o;
}

/// Kuralın ne yaptığını TEK CÜMLEDE söyler. Eşik yönü iki eşikten türer;
/// kullanıcının bunu kutulara bakıp çıkarması beklenemez.
function ruleSummary(r) {
  const act = (ACT_META[r.target] || { name: r.target }).name;

  if (r.kind === 'threshold') {
    const meta = SENSOR_META[r.sensor] || { label: r.sensor, unit: '' };
    const u = meta.unit ? ' ' + meta.unit : '';
    const down = (+r.onThreshold < +r.offThreshold);
    return `${esc(meta.label)} ${esc(r.onThreshold)}${u} ${down ? 'ALTINA düşünce' : 'ÜSTÜNE çıkınca'} ` +
           `<b>${esc(act)}</b> açılır, ${esc(r.offThreshold)}${u} değerine ${down ? 'yükselince' : 'düşünce'} kapanır.`;
  }
  if (r.kind === 'window') {
    const wrap = (+r.startMin > +r.endMin)
      ? ' <span class="warn-inline">(gece yarısını aşar)</span>' : '';
    return `Her gün ${minToTime(r.startMin)} – ${minToTime(r.endMin)} arasında <b>${esc(act)}</b> açık.${wrap}`;
  }
  return `Her ${fmtSec(r.cyclePeriodS)} içinde ${fmtSec(r.cycleOnS)} boyunca <b>${esc(act)}</b> açık.`;
}

const ruleSelect = (i, key, label, options, current) => `
  <div class="form-group">
    <label>${label}</label>
    <select data-ri="${i}" data-rk="${key}">
      ${options.map(([v, t]) =>
        `<option value="${esc(v)}"${String(v) === String(current) ? ' selected' : ''}>${t}</option>`
      ).join('')}
    </select>
  </div>`;

const ruleNum = (i, key, label, val, step, unit, note) => `
  <div class="form-group">
    <label>${label}</label>
    <div class="field-row">
      <input type="number" step="${step}" value="${esc(val)}" data-ri="${i}" data-rk="${key}">
      <span class="field-unit">${unit || ''}</span>
    </div>
    ${note ? `<span class="field-hint dim">${note}</span>` : ''}
  </div>`;

const ruleTime = (i, key, label, minutes) => `
  <div class="form-group">
    <label>${label}</label>
    <input type="time" value="${minToTime(minutes)}" data-ri="${i}" data-rk="${key}">
  </div>`;

const checkboxRule = (i, key, label, on) => `
  <div class="form-group" style="margin-top:10px;">
    <label class="check-line">
      <input type="checkbox" ${on ? 'checked' : ''} data-ri="${i}" data-rk="${key}">
      <span>${label}</span>
    </label>
  </div>`;

function ruleKindFields(r, i) {
  if (r.kind === 'threshold') {
    const meta = SENSOR_META[r.sensor] || { label: r.sensor, unit: '', digits: 2 };
    const step = meta.digits === 0 ? '1' : (meta.digits === 1 ? '0.1' : '0.01');
    return `
      ${ruleSelect(i, 'sensor', 'Ölçüm', sensorOptions(), r.sensor)}
      ${ruleNum(i, 'onThreshold', 'Açma Eşiği', r.onThreshold, step, meta.unit)}
      ${ruleNum(i, 'offThreshold', 'Kapatma Eşiği', r.offThreshold, step, meta.unit,
        'Yön İKİ EŞİKTEN türer: açma &lt; kapatma ise değer düşünce açılır, açma &gt; kapatma ise değer yükselince açılır. Eşit olamaz.')}
      ${thresholdRangeWarning(r)}`;
  }
  if (r.kind === 'window') {
    return `
      ${ruleTime(i, 'startMin', 'Başlangıç Saati', r.startMin)}
      ${ruleTime(i, 'endMin', 'Bitiş Saati', r.endMin)}
      <span class="field-hint dim">Gece yarısını aşan pencere geçerlidir (22:00 – 02:00). Çizelgeler cihaz saatinin geçerli olmasını gerektirir.</span>`;
  }
  return `
    ${ruleNum(i, 'cycleOnS', 'Açık Kalma Süresi', r.cycleOnS, '1', 'sn', fmtSec(r.cycleOnS))}
    ${ruleNum(i, 'cyclePeriodS', 'Çevrim Periyodu', r.cyclePeriodS, '1', 'sn',
      fmtSec(r.cyclePeriodS) + ' — açık kalma süresi periyottan KISA olmalı')}
    ${cycleVsMaxRunWarning(r)}`;
}

/// Çevrim süresi hedef cihazın `maxRunMs`'ini aşarsa, cihaz HER çevrimde
/// süre aşımıyla zorla kapatılır ve kullanıcı doğru görünen bir kuralın neden
/// kesildiğini bulamaz.
function cycleVsMaxRunWarning(r) {
  if (!store.config || !store.config.actuators) return '';
  const a = store.config.actuators.find((x) => x.id === r.target);
  if (!a) return '';
  const onMs = (+r.cycleOnS || 0) * 1000;
  if (onMs <= a.maxRunMs) return '';
  return `<span class="field-hint err">Açık kalma süresi (${fmtDur(onMs)}), bu cihazın azami çalışma süresinden (${fmtDur(a.maxRunMs)}) uzun — güvenlik zinciri her çevrimde erken kapatır.</span>`;
}

function thresholdRangeWarning(r) {
  const meta = SENSOR_META[r.sensor];
  if (!meta) return '';
  let html = '';

  const out = [r.onThreshold, r.offThreshold].some((v) => +v < meta.min || +v > meta.max);
  if (out) {
    html += `<span class="field-hint err">Eşiklerden biri ölçümün beklenen aralığı (${meta.min} – ${meta.max}) dışında; cihaz bu kuralı reddedebilir.</span>`;
  }

  // Takılı olmayan bir sensöre bağlı kural DOĞRULAMADAN GEÇER ama hiç
  // tetiklenmez — kullanıcı kuralın neden çalışmadığını anlayamaz.
  const live = store.state && store.state.sensors
    ? store.state.sensors.find((sn) => sn.id === r.sensor) : null;
  if (live && (live.quality === 'notPresent' || live.quality === 'fault')) {
    html += `<span class="field-hint err">${esc(meta.label)} şu an "${qualityText(live.quality)}" — bu kural cihazda hiç tetiklenmez.</span>`;
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
      ${ruleSelect(i, 'target', 'Hedef Cihaz', actOptions(), r.target)}
      ${ruleKindFields(r, i)}
      ${ruleNum(i, 'priority', 'Öncelik', r.priority, '1', '',
        'Aynı cihazı hedefleyen kurallarda BÜYÜK olan kazanır. İki etkin kural aynı hedefe aynı önceliği veremez.')}
      ${ruleNum(i, 'minTriggerIntervalS', 'Asgari Tetikleme Aralığı', r.minTriggerIntervalS, '1', 'sn',
        'Kuralın bu süreden sık tetiklenmesini engeller — histerezisten ayrı, hızlı salınıma karşı ikinci koruma.')}
    </div>`;
}

function renderRules() {
  const host = el('rulesList');
  if (!host) return;
  const rules = store.rules;

  txt(el('rulesBadge'), rules.length + ' / ' + MAX_RULES + ' kural');

  host.innerHTML = rules.length
    ? rules.map((r, i) => ruleCard(r, i)).join('')
    : '<div class="card"><p class="muted">Tanımlı kural yok — OTOMATİK modda bile hiçbir cihaz kendiliğinden çalışmaz.</p></div>';

  el('addRule').disabled = rules.length >= MAX_RULES;
  bindRuleInputs();
}

function bindRuleInputs() {
  document.querySelectorAll('#rulesList [data-ri]').forEach((inp) => {
    inp.onchange = () => {
      const r = store.rules[+inp.dataset.ri];
      if (!r) return;
      const key = inp.dataset.rk;

      if (inp.type === 'checkbox')    r[key] = inp.checked;
      else if (inp.type === 'time')   r[key] = timeToMin(inp.value);
      else if (inp.type === 'number') r[key] = +inp.value;
      else                            r[key] = inp.value;

      renderRules();
    };
  });

  document.querySelectorAll('#rulesList [data-del]').forEach((b) => {
    b.onclick = () => {
      store.rules.splice(+b.dataset.del, 1);
      renderRules();
      el('rulesMsg').className = 'form-feedback';
      txt(el('rulesMsg'), 'Kural listeden çıkarıldı — değişiklik KAYDEDİLMEDİ.');
    };
  });
}

async function loadRules() {
  try {
    const data = await api('/api/config/rules');
    store.rules = (data.rules || []).map(normalizeRule);

    // Panel "kurallar hazır ama sistem manuel modda" diyebilsin diye.
    store.rulesCountHint = store.rules.filter((r) => r.enabled).length;

    renderRules();
    if (el('rulesMsg')) {
      el('rulesMsg').className = 'form-feedback';
      txt(el('rulesMsg'), '');
    }
  } catch (e) {
    if (el('rulesList')) {
      el('rulesList').innerHTML = `<div class="card"><p class="err">${esc(e.message)}</p></div>`;
    }
  }
}

/// Cihazın `validateRules` denetiminin İSTEMCİ KOPYASI. Cihaz her hâlükârda
/// yeniden doğrular; amaç, hangi kuralın neden reddedileceğini ağ turu
/// beklemeden söylemek.
function validateRulesClient(rules) {
  for (let i = 0; i < rules.length; i++) {
    const r = rules[i], n = i + 1;
    if (r.kind === 'threshold') {
      if (+r.onThreshold === +r.offThreshold) {
        return `Kural ${n}: açma ve kapatma eşiği eşit olamaz — histerezis bandı sıfır olur ve röle gürültüyle çırpınır.`;
      }
    } else if (r.kind === 'window') {
      if (+r.startMin === +r.endMin) return `Kural ${n}: başlangıç ve bitiş saati aynı olamaz.`;
    } else if (r.kind === 'cycle') {
      if (!(+r.cyclePeriodS > 0)) return `Kural ${n}: çevrim periyodu sıfır olamaz.`;
      if (!(+r.cycleOnS > 0) || +r.cycleOnS >= +r.cyclePeriodS) {
        return `Kural ${n}: açık kalma süresi periyottan kısa olmalıdır.`;
      }
    }
  }

  for (let i = 0; i < rules.length; i++) {
    for (let j = i + 1; j < rules.length; j++) {
      const a = rules[i], b = rules[j];
      if (!a.enabled || !b.enabled) continue;
      if (a.target === b.target && +a.priority === +b.priority) {
        const act = (ACT_META[a.target] || { name: a.target }).name;
        return `Kural ${i + 1} ile ${j + 1} aynı cihazı (${act}) aynı öncelikle hedefliyor — hangisinin kazanacağı belirsiz.`;
      }
    }
  }
  return null;
}

function ruleFieldText(field) {
  const m = /^rules\[(\d+)\]\.(.+)$/.exec(field || '');
  const LEAF = {
    kind: 'kural türü', target: 'hedef cihaz', sensor: 'ölçüm',
    threshold: 'eşikler', onThreshold: 'açma eşiği', offThreshold: 'kapatma eşiği',
    startMin: 'başlangıç saati', endMin: 'bitiş saati', window: 'zaman penceresi',
    cycleOnS: 'açık kalma süresi', cyclePeriodS: 'çevrim periyodu',
    cycle: 'çevrim', priority: 'öncelik', minTriggerIntervalS: 'tetikleme aralığı',
    object: 'kural kaydı',
  };
  if (m) return `Kural ${+m[1] + 1} · ${LEAF[m[2]] || m[2]}`;

  const SET = {
    'rules.count': 'Kural sayısı', 'rules.priority': 'Kural çakışması (öncelik)',
    'rules.target': 'Hedef cihaz', 'rules.sensor': 'Ölçüm',
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
      method: 'PUT', body: JSON.stringify({ rules: store.rules }),
    });
    m.className = 'form-feedback msg';
    txt(m, '✔ Kural kümesi yazıldı. Kurallar yalnızca OTOMATİK modda değerlendirilir. ' +
           'Elle düzenleme yaptığınız için ürün profili ÖZEL\'e düştü.');

    // Cihaz profili CUSTOM'a düşürdü; arayüz aksini göstermemeli.
    loadCrop();
  } catch (e) {
    m.className = 'form-feedback err';
    const label = ruleFieldText(e.field);
    txt(m, label ? label + ': ' + e.message : e.message);
  }
}

// ── Geçmiş grafiği ─────────────────────────────────────────────────────────

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

  const w = rect.width, h = 260;
  const padL = 44, padR = 16, padT = 20, padB = 30;
  const plotW = w - padL - padR, plotH = h - padT - padB;
  ctx.clearRect(0, 0, w, h);

  const fieldIdx = data.fields.indexOf(store.activeChartSensor);
  if (fieldIdx < 0) return;

  const rows = data.rows;
  const values = rows.map((r) => r.v[fieldIdx]);
  let minVal = Math.min(...values), maxVal = Math.max(...values);
  if (minVal === maxVal) { minVal -= 1; maxVal += 1; }
  const padVal = (maxVal - minVal) * 0.15;
  minVal -= padVal; maxVal += padVal;

  ctx.strokeStyle = 'rgba(255,255,255,0.07)';
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

  const points = rows.map((r, i) => {
    const x = padL + (i / (rows.length - 1 || 1)) * plotW;
    const val = r.v[fieldIdx];
    const y = padT + plotH - ((val - minVal) / (maxVal - minVal)) * plotH;
    return { x, y };
  });

  const grad = ctx.createLinearGradient(0, padT, 0, padT + plotH);
  grad.addColorStop(0, 'rgba(16,185,129,0.35)');
  grad.addColorStop(1, 'rgba(16,185,129,0)');

  ctx.beginPath();
  ctx.moveTo(points[0].x, padT + plotH);
  points.forEach((p) => ctx.lineTo(p.x, p.y));
  ctx.lineTo(points[points.length - 1].x, padT + plotH);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  ctx.beginPath();
  points.forEach((p, idx) => (idx === 0 ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y)));
  ctx.strokeStyle = '#10b981';
  ctx.lineWidth = 2.5;
  ctx.stroke();

  ctx.fillStyle = '#34d399';
  points.forEach((p) => {
    ctx.beginPath();
    ctx.arc(p.x, p.y, 2.5, 0, Math.PI * 2);
    ctx.fill();
  });
}

// ── Teşhis ─────────────────────────────────────────────────────────────────

async function loadDiagnostics() {
  try {
    const d = await api('/api/diagnostics');

    const fc = el('faultCard');
    if (fc) {
      fc.innerHTML = d.active.length
        ? d.active.map((c) => `<div class="err" style="padding:4px 0;">⚠ ${esc(errText(c))}</div>`).join('')
        : '<p class="muted" style="color:var(--accent-bright)">✔ Sistemde aktif hata yok.</p>';
    }

    const tc = el('tasksCard');
    if (tc && d.tasks) {
      tc.innerHTML = d.tasks.map((t) => `
        <div class="card task-card">
          <div class="task-name">${esc(t.name)}</div>
          <div class="task-metric">Döngü süresi: <b>${esc(t.maxLoopUs)} µs</b></div>
          <div class="task-metric">Boş stack: <b>${esc(t.minStack)} B</b></div>
          <div class="task-metric">Taşma: <b>${esc(t.overruns)}</b></div>
        </div>`).join('');
    }

    const lb = el('logBody');
    if (lb && d.events) {
      lb.innerHTML = d.events.slice().reverse().map((e) => `
        <tr class="lvl-${e.level}">
          <td>${fmtUptime(e.t)}</td>
          <td><b>${esc(errText(e.code) || '0x' + e.code.toString(16))}</b></td>
          <td class="muted">${esc(e.d || '—')}</td>
        </tr>`).join('');
    }
  } catch (e) {
    if (el('faultCard')) el('faultCard').innerHTML = `<p class="err">${esc(e.message)}</p>`;
  }
}

// ── Wi-Fi tarama ───────────────────────────────────────────────────────────

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
  const st = { idle: 'Hazır', running: 'Ağlar taranıyor…', done: 'Tarama tamamlandı',
               failed: 'Tarama başarısız' }[r.status] || r.status;
  txt(el('scanStatus'), st + (r.truncated ? ' (liste sınırlandı)' : ''));

  const list = el('scanList');
  if (!list) return;

  list.innerHTML = r.networks.slice().sort((a, b) => b.rssi - a.rssi).map((n) => `
    <li class="scan-item" data-ssid="${esc(n.ssid)}">
      <span class="scan-ssid">${n.open ? '🔓' : '🔒'} <b>${esc(n.ssid)}</b></span>
      <span class="scan-meta">${esc(n.rssi)} dBm · kanal ${esc(n.channel)}</span>
    </li>`).join('');

  list.querySelectorAll('li').forEach((li) => {
    li.onclick = () => { el('ssid').value = li.dataset.ssid; el('wifiPw').focus(); };
  });
}
