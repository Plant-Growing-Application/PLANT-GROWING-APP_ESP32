/* Görünümler — yönlendirme, panel, kontrol, basit ayarlar (TASK-070) */

let currentView = 'garden';

function switchView(name) {
  currentView = name;
  document.querySelectorAll('.tab-btn').forEach((t) =>
    t.classList.toggle('active', t.dataset.view === name));
  document.querySelectorAll('.page').forEach((p) =>
    p.classList.toggle('hidden', p.id !== 'v-' + name));

  // Sekmeye girildiğinde o sekmenin verisi çekilir. Hepsini açılışta çekmek
  // AP modunda gözle görülür bir gecikme yaratırdı.
  if (name === 'diagnostics') loadDiagnostics();
  if (name === 'advanced')    loadConfig();
  if (name === 'charts')      loadHistory();
  if (name === 'settings')    loadSimpleSettings();
}

// ── Çizim motoru ───────────────────────────────────────────────────────────

function render() {
  const s = store.state;
  if (!s) return;

  renderHeadline(s);
  renderStatus(s);
  renderSafetyBar(s);
  renderSensors(s);
  renderActuators(s);
  renderNetwork(s);
  renderInterlocks(s);
  renderAdvice();
}

/// Aktif güvenlik kilitlerini Kontrol ekranında listeler.
///
/// Cihaz `interlockMask`'ı her pakette yayınlıyordu ama arayüz onu hiç
/// çözmüyordu. Kullanıcının "neden çalışmıyor" sorusunun cevabı telemetride
/// hazır duruyordu ve gösterilmiyordu (TASK-074).
function renderInterlocks(s) {
  const host = el('interlockList');
  if (!host) return;

  const mask = (s.safety && s.safety.interlocks) || 0;
  const items = interlockList(mask);

  show(host, items.length > 0);
  if (!items.length) { host.innerHTML = ''; return; }

  host.innerHTML = items.map((t) => {
    // Temizlemeyi ENGELLEYEN kilitler daha ağır gösterilir: kullanıcının
    // önce onları düzeltmesi gerekiyor.
    const blocking = interlockList(mask & CLEAR_BLOCKERS).indexOf(t) >= 0;
    return `<div class="advice advice-${blocking ? 'bad' : 'warn'}">
              <span class="advice-dot"></span><span>${esc(t)}</span>
            </div>`;
  }).join('');
}

/// Tek cümlelik genel durum — panelin en üstünde.
///
/// Kullanıcının bakması gereken TEK yer burasıdır: bir sorun varsa burada
/// yazar. Alt kartlar ayrıntıdır.
function renderHeadline(s) {
  const box  = el('headline');
  const icon = el('headlineIcon');
  const ttl  = el('headlineTitle');
  const sub  = el('headlineSub');
  if (!box) return;

  let level = 'ok', ic = '🌿', title = 'Her şey yolunda', detail = '';

  if (s.safety && s.safety.latched) {
    level = 'bad'; ic = '🛑';
    title = 'Acil durum kilidi aktif';
    detail = errText(s.safety.reason) || 'Operatör onayı gerekiyor';
  } else if (s.system.mode === 'emergency') {
    level = 'bad'; ic = '🛑';
    title = 'Sistem acil durumda';
    detail = 'Kontrol ekranından durumu temizleyin';
  } else if (s.system.mode === 'safe') {
    level = 'bad'; ic = '⚠️';
    title = 'Güvenli moddayız';
    detail = 'Cihazlar kilitli — Teşhis ekranında neden yazıyor';
  } else if (s.safety && s.safety.interlocks) {
    level = 'warn'; ic = '⚠️';
    title = 'Bir güvenlik kilidi devrede';
    detail = errText(s.safety.reason) || 'Ayrıntı aşağıda';
  } else if (s.system.mode === 'degraded') {
    level = 'warn'; ic = '⚠️';
    title = 'Sistem kısıtlı çalışıyor';
    detail = 'Bazı işlevler devre dışı — Teşhis ekranına bakın';
  } else {
    // Sorun yok: en faydalı bilgi ne olduğunu ürün belirler.
    const advice = buildAdvice();
    const worst = advice.find((a) => a.level === 'bad') || advice.find((a) => a.level === 'warn');
    if (worst) {
      level = worst.level;
      ic = worst.level === 'bad' ? '⚠️' : '🌤️';
      title = worst.level === 'bad' ? 'Dikkat gerekiyor' : 'Küçük bir not var';
      detail = worst.text;
    } else if (cropSelected()) {
      const c = store.crop;
      title = `${cropDisplayName()} · ${STAGE_TEXT[c.stage] || c.stage} dönemi`;
      detail = 'Ölçümler hedef aralıkta';
    } else {
      title = 'Sistem çalışıyor';
      detail = 'Ürün seçilmedi — Ayarlar bölümünden seçebilirsiniz';
    }
  }

  box.className = 'headline headline-' + level;
  txt(icon, ic);
  txt(ttl, title);
  txt(sub, detail);
}

function renderStatus(s) {
  const modeName = MODE_TEXT[s.system.mode] || s.system.mode;
  txt(el('sMode'), modeName);
  txt(el('headMode'), modeName);

  const hmp = el('headModePill');
  if (hmp) {
    hmp.className = 'mode-pill ' +
      (s.system.mode === 'emergency' ? 'emergency' : (s.system.mode === 'safe' ? 'safe' : ''));
  }

  txt(el('sUptime'), fmtUptime(s.system.uptimeMs));
  txt(el('sHeap'), Math.round(s.system.freeHeap / 1024) + ' KB');
  txt(el('sClock'), s.time.valid
    ? new Date(s.time.epoch * 1000).toLocaleTimeString('tr-TR',
        { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    : 'Saat ayarlı değil');
}

function renderSafetyBar(s) {
  const bar = el('safetyBar');
  if (!bar) return;

  if (s.safety.latched) {
    bar.className = 'safetybar critical';
    bar.textContent = 'ACİL DURUM KİLİDİ: ' + (errText(s.safety.reason) || 'Kilitlendi') +
      ' — sizin onayınız gerekiyor';
    show(bar, true);
  } else if (s.safety.interlocks) {
    bar.className = 'safetybar';
    bar.textContent = 'Güvenlik kilidi: ' + (errText(s.safety.reason) || 'Kilit devrede');
    show(bar, true);
  } else {
    show(bar, false);
  }
}

// ── Ölçüm kartları ─────────────────────────────────────────────────────────

function renderSensors(s) {
  const g = el('sensorGrid');
  if (!g) return;
  g.innerHTML = '';

  s.sensors.forEach((sn) => {
    const st = sensorStatus(sn);
    const meta = SENSOR_META[sn.id] || { label: sn.id, unit: '', min: 0, max: 100 };

    // Ölçek çubuğu: değerin sensörün fiziksel aralığındaki yeri. Hedef band
    // varsa ayrıca işaretlenir — kullanıcı "neredeyim" sorusunu bir bakışta
    // yanıtlayabilsin.
    let pct = 0, bandStyle = '';
    if (st.level !== 'off' && sn.quality !== 'fault') {
      const num = sn.id === 'level' ? Math.round(sn.value) : Number(sn.value);
      const lo = sn.id === 'level' ? 0 : meta.min;
      const hi = sn.id === 'level' ? 2 : meta.max;
      pct = Math.max(3, Math.min(100, Math.round(((num - lo) / (hi - lo)) * 100)));

      if (st.band) {
        const bl = Math.max(0, Math.min(100, ((st.band.min - lo) / (hi - lo)) * 100));
        const bw = Math.max(2, Math.min(100 - bl, ((st.band.max - st.band.min) / (hi - lo)) * 100));
        bandStyle = `<span class="sensor-band" style="left:${bl}%;width:${bw}%"></span>`;
      }
    }

    const card = document.createElement('div');
    card.className = 'card sensor-card lvl-' + st.level;
    card.innerHTML = `
      <div class="sensor-header">
        <span class="sensor-title">${esc(st.label)}</span>
        <span class="dot dot-${st.level}"></span>
      </div>
      <div class="sensor-body">
        <span class="sensor-val">${esc(st.value)}</span>
        <span class="sensor-unit">${esc(st.unit)}</span>
      </div>
      <div class="sensor-bar-wrap">
        ${bandStyle}
        <div class="sensor-bar" style="width:${pct}%"></div>
      </div>
      <div class="sensor-note">${esc(st.text)}</div>`;
    g.appendChild(card);
  });
}

// ── Cihaz (aktüatör) kartları ──────────────────────────────────────────────

function renderActuators(s) {
  const g = el('actuatorGrid');
  if (!g) return;
  g.innerHTML = '';

  const pendingTargets = new Set([...store.pending.values()].map((p) => p.target));

  s.actuators.forEach((a) => {
    const meta = ACT_META[a.id];
    if (!meta) return;

    const pending = pendingTargets.has(a.id);
    const stateClass = pending ? 'pending' : (a.on ? 'on' : 'off');
    const stateText = pending ? 'BEKLİYOR…' : (a.on ? 'ÇALIŞIYOR' : 'KAPALI');

    // Neden açılmıyor? Önce az önceki reddin nedeni, yoksa cihazın bildirdiği
    // kalıcı engel. İkisi de yoksa boş.
    let why = '';
    if (store.rejected && store.rejected.target === a.id &&
        Date.now() - store.rejected.at < 8000) {
      why = store.rejected.text;
    } else if (a.block) {
      why = errText(a.block);
    }

    const wired = actuatorEnabled(a.id);

    const card = document.createElement('div');
    card.className = `card act-card ${stateClass}`;
    card.innerHTML = `
      <div>
        <div class="act-header">
          <div>
            <div class="act-name">${meta.icon} ${esc(meta.name)}</div>
            <div class="muted small">${esc(meta.desc)}</div>
          </div>
          <span class="act-state-badge">${stateText}</span>
        </div>
        <div class="act-metrics">
          <span>Toplam çalışma: <b>${fmtUptime(a.runMs)}</b></span>
          <span>Açılma: <b>${a.cycles || 0}</b></span>
        </div>
        ${!wired ? '<div class="act-why">Bu cihaz "bağlı" olarak işaretli değil — Ayarlar → Bağlı cihazlar</div>' : ''}
        <div class="act-why">${esc(why)}</div>
      </div>`;

    const btn = document.createElement('button');
    btn.className = `btn ${a.on ? 'btn-danger' : 'btn-primary'} btn-block`;
    btn.textContent = a.on ? 'Kapat' : 'Çalıştır';
    btn.disabled = pending || !store.linked;
    btn.onclick = () => sendCmd(a.id, a.on ? 'off' : 'on');
    card.appendChild(btn);

    g.appendChild(card);
  });
}

function renderNetwork(s) {
  const c = el('netCard');
  if (!c) return;
  const n = s.network;
  c.innerHTML = `
    <div class="net-grid">
      <div><span class="muted small">Ağ Durumu</span>
        <div class="net-val accent">${esc(NET_TEXT[n.state] || n.state)}</div></div>
      <div><span class="muted small">Bağlı Ağ</span>
        <div class="net-val">${esc(n.ssid || '—')}</div></div>
      <div><span class="muted small">IP Adresi</span>
        <div class="net-val mono">${esc(n.ip || '—')}</div></div>
      <div><span class="muted small">Sinyal Gücü</span>
        <div class="net-val">${n.rssi ? esc(n.rssi) + ' dBm' : '—'}</div></div>
    </div>
    ${n.apActive ? `<div class="info-note">Kurulum ağı (AP) açık — <b>${esc(n.apClients)}</b> cihaz bağlı</div>` : ''}
    ${n.lastError ? `<div class="err" style="margin-top:8px;">Son hata: ${esc(errText(n.lastError))}</div>` : ''}`;
}

// ── Basit ayarlar: bağlı cihazlar + otomatik mod ───────────────────────────

async function loadSimpleSettings() {
  try {
    store.config = await api('/api/config');
  } catch (e) {
    const m = `<p class="err">${esc(e.message)}</p>`;
    ['hardwareCard', 'automationForm'].forEach((id) => { if (el(id)) el(id).innerHTML = m; });
    return;
  }
  renderSensorCard();
  renderHardwareCard();
  renderAutomationSimple(store.config);
  renderCropSettings();
}

/// "Takılı sensörler" — ISSUE-035'in arayüz karşılığı.
///
/// Bu bölüm hiç yoktu: pH, EC, nem, hava sıcaklığı ve ışık `enabled = 0`
/// doğuyor ve açılmasının HİÇBİR yolu yoktu. Kullanıcı sensörü fiziksel
/// olarak taksa bile arayüzde sonsuza kadar "takılı değil" yazıyordu.
function renderSensorCard() {
  const host = el('sensorCard');
  if (!host) return;

  const list = (store.config && store.config.sensors) || [];
  if (!list.length) {
    host.innerHTML = '<p class="muted">Sensör listesi alınamadı — firmware güncel mi?</p>';
    return;
  }

  host.innerHTML = OPTIONAL_SENSORS.map((id) => {
    const s = list.find((x) => x.id === id);
    if (!s) return '';
    const meta = SENSOR_META[id] || { label: id };
    return `
      <label class="hw-row">
        <input type="checkbox" data-sens="${esc(id)}" ${s.enabled ? 'checked' : ''}>
        <span class="hw-text">
          <b>${esc(meta.label)}</b>
          <span class="muted small">${esc(SENSOR_DESC[id] || '')}</span>
        </span>
      </label>`;
  }).join('') + '<p id="sensMsg" class="form-feedback"></p>';

  host.querySelectorAll('[data-sens]').forEach((cb) => {
    cb.onchange = () => toggleSensor(cb.dataset.sens, cb.checked, cb);
  });
}

async function toggleSensor(id, on, cb) {
  const m = el('sensMsg');
  const list = (store.config && store.config.sensors) || [];
  const idx = list.findIndex((x) => x.id === id);
  if (idx < 0) return;

  m.className = 'form-feedback';
  txt(m, 'Kaydediliyor…');

  try {
    await api('/api/config/sensors', {
      method: 'PUT',
      body: JSON.stringify({ index: idx, enabled: on }),
    });
    list[idx].enabled = on;
    const label = (SENSOR_META[id] || { label: id }).label;
    m.className = 'form-feedback msg';
    // Cihaz sürücüyü ÇALIŞMA ANINDA başlatıyor; yeniden başlatma gerekmiyor.
    // Bunu söylemek önemli: eski davranışta sensör açılıyor ama okunmuyordu.
    txt(m, on
      ? `✔ ${label} açıldı. Birkaç saniye içinde ölçüm gelmezse kablosunu kontrol edin.`
      : `✔ ${label} kapatıldı.`);
  } catch (e) {
    // Cihaz reddettiyse kutu ESKİ hâline döner (P5).
    cb.checked = !on;
    m.className = 'form-feedback err';
    txt(m, e.field ? (FIELD_TEXT[e.field] || e.field) + ': ' + e.message : e.message);
  }
}

/// "Hangi cihazlar bağlı?" — profilin hangi kuralları üretebileceğini
/// belirleyen tek soru. Bağlı olmayan bir röleye kural yazılmaz.
function renderHardwareCard() {
  const host = el('hardwareCard');
  if (!host) return;

  const list = (store.config && store.config.actuators) || [];
  if (!list.length) {
    host.innerHTML = '<p class="muted">Cihaz listesi alınamadı.</p>';
    return;
  }

  host.innerHTML = OPTIONAL_ACTUATORS.map((id) => {
    const a = list.find((x) => x.id === id);
    if (!a) return '';
    const meta = ACT_META[id];
    return `
      <label class="hw-row">
        <input type="checkbox" data-hw="${esc(id)}" ${a.enabled ? 'checked' : ''}>
        <span class="hw-icon">${meta.icon}</span>
        <span class="hw-text">
          <b>${esc(meta.name)}</b>
          <span class="muted small">${esc(meta.desc)}</span>
        </span>
      </label>`;
  }).join('') + '<p id="hwMsg" class="form-feedback"></p>';

  host.querySelectorAll('[data-hw]').forEach((cb) => {
    cb.onchange = () => toggleHardware(cb.dataset.hw, cb.checked, cb);
  });
}

async function toggleHardware(id, on, cb) {
  const m = el('hwMsg');
  const list = (store.config && store.config.actuators) || [];
  const idx = list.findIndex((x) => x.id === id);
  if (idx < 0) return;

  m.className = 'form-feedback';
  txt(m, 'Kaydediliyor…');

  try {
    await api('/api/config/actuators', {
      method: 'PUT',
      body: JSON.stringify({ index: idx, enabled: on }),
    });
    list[idx].enabled = on;
    m.className = 'form-feedback msg';
    txt(m, on
      ? `✔ ${ACT_META[id].name} bağlı olarak işaretlendi. Sulama programını yeniden kurmak için "Ürünü / Dönemi Değiştir" deyin.`
      : `✔ ${ACT_META[id].name} bağlı değil olarak işaretlendi.`);
  } catch (e) {
    // Cihaz reddettiyse kutuyu ESKİ hâline döndürüyoruz: aksi hâlde arayüz
    // kaydedilmemiş bir durumu kaydedilmiş gibi gösterirdi (P5).
    cb.checked = !on;
    m.className = 'form-feedback err';
    txt(m, e.message);
  }
}

/// Otomatik mod — basit ekrandaki tek anahtar.
///
/// Uzman modundaki "manuel müdahale süresi" gibi ayarlar burada YOK; onlar
/// Gelişmiş ekranındadır.
function renderAutomationSimple(c) {
  const host = el('automationForm');
  if (!host) return;

  if (!c.automation) {
    host.innerHTML = '<p class="err">Cihaz otomasyon ayarını döndürmüyor — firmware güncel mi?</p>';
    txt(el('autoBadge'), '—');
    return;
  }

  let isAuto = c.automation.mode === 'auto';
  const badge = el('autoBadge');
  const paint = () => {
    txt(badge, isAuto ? 'OTOMATİK' : 'MANUEL');
    badge.className = 'section-badge' + (isAuto ? ' badge-auto' : '');
  };
  paint();

  host.innerHTML = `
    <div class="switch-row">
      <div>
        <b>Sistem programı kendisi uygulasın</b>
        <p class="muted small">
          Kapalıyken hiçbir cihaz kendiliğinden çalışmaz; yalnızca sizin
          verdiğiniz komutlar işler. Açıkken sulama programı, ışık saati ve
          sıcaklık eşikleri devreye girer.
        </p>
      </div>
      <label class="switch">
        <input id="autoSwitch" type="checkbox" ${isAuto ? 'checked' : ''}>
        <span class="switch-track"><span class="switch-thumb"></span></span>
      </label>
    </div>

    <div class="warn-note">
      <b>Açmadan önce:</b> pompanın yalnızca güvenlik izniyle çalıştığını
      donanımda doğrulayın. Güvenlik kilitleri her koşulda devrededir, ancak
      ilk kurulumda röle polaritesi ve su seviyesi şamandıraları test
      edilmeden otomatik moda geçmeyin.
    </div>
    <p id="autoMsg" class="form-feedback"></p>`;

  el('autoSwitch').onchange = async (ev) => {
    const want = ev.target.checked;
    const m = el('autoMsg');

    // MANUEL → OTOMATİK tek yönlü bir risk artışıdır: bu andan sonra cihazlar
    // operatör komutu olmadan da sürülebilir. Onay istemek, yanlışlıkla
    // dokunulan bir anahtarın sonucu olmasını engeller.
    if (want && !confirm(
      'Otomatik çalışma açılıyor.\n\n' +
      'Bu andan sonra sulama, ışık ve ısıtma programı sizin onayınız olmadan ' +
      'cihazları çalıştırabilir. Güvenlik kilitleri devrede kalır.\n\n' +
      'Donanım testini tamamladıysanız devam edin.')) {
      ev.target.checked = false;
      return;
    }

    m.className = 'form-feedback';
    txt(m, 'Kaydediliyor…');
    try {
      await api('/api/config/automation', {
        method: 'PUT',
        body: JSON.stringify({ mode: want ? 'auto' : 'manual' }),
      });
      isAuto = want;
      paint();
      m.className = 'form-feedback msg';
      txt(m, want ? '✔ Otomatik çalışma açıldı.' : '✔ Otomatik çalışma kapatıldı.');
    } catch (e) {
      ev.target.checked = !want;
      m.className = 'form-feedback err';
      txt(m, e.message);
    }
  };
}

// ── Hızlı eylemler ─────────────────────────────────────────────────────────

// ── Acil durumu temizleme (TASK-074) ───────────────────────────────────────
//
// ESKİ DAVRANIŞ: düğme komutu kuyruğa atıyor, cihaz `ACCEPTED` (= kuyruğa
// alındı) diyordu. Cihaz sonra `acknowledge()` içinde koşulları kontrol edip
// temizlemeyi REDDEDİYOR ve bunu yalnızca olay günlüğüne yazıyordu.
//
// Kullanıcı açısından sonuç: düğmeye basıyorsun, "kabul edildi" görüyorsun,
// hiçbir şey olmuyor ve NEDENİNİ hiçbir yerde göremiyorsun.
//
// Şimdi üç aşama var:
//   1. ÖNCE KONTROL — engel varsa komut hiç gönderilmez, ne yapılacağı yazılır
//   2. gönder
//   3. SONRA DOĞRULA — 2 sn içinde temizlenmediyse bunu söyle
function clearEmergency() {
  const m = el('estopMsg');
  const s = store.state;

  if (!s) {
    m.className = 'form-feedback err';
    txt(m, 'Cihaz durumu henüz alınmadı — bağlantıyı bekleyin.');
    return;
  }

  if (!s.safety.latched) {
    m.className = 'form-feedback msg';
    txt(m, 'Acil durum kilidi zaten aktif değil.');
    return;
  }

  // --- 1) Engelleri önceden söyle ---
  const blocking = (s.safety.interlocks || 0) & CLEAR_BLOCKERS;
  if (blocking !== 0) {
    m.className = 'form-feedback err';
    txt(m, 'Temizlenemez — önce şunu düzeltin: ' + interlockList(blocking).join(' · ') +
           '. Su seviyesi sensörü takılı değilse Gelişmiş → Güvenlik Eşikleri bölümünden ' +
           '"su seviyesi sensörü okunamıyorsa pompa kilitli kalsın" seçeneğini kapatabilirsiniz.');
    return;
  }

  m.className = 'form-feedback';
  txt(m, 'Temizleme komutu gönderildi…');
  sendCmd('system', 'emergencyClear');

  // --- 3) Gerçekten temizlendi mi? ---
  //
  // Komut kuyruğa alındı diye temizlendiği varsayılamaz: cihaz `app_core`
  // turunda koşulları yeniden değerlendirir ve reddedebilir (P5 — arayüz
  // durum üretmez, doğrular).
  setTimeout(() => {
    const now = store.state;
    if (!now) return;
    if (now.safety.latched) {
      const why = (now.safety.interlocks || 0) & CLEAR_BLOCKERS;
      m.className = 'form-feedback err';
      txt(m, why !== 0
        ? 'Cihaz temizlemeyi reddetti: ' + interlockList(why).join(' · ')
        : 'Cihaz temizlemeyi reddetti. Teşhis → Olay Günlüğü nedenini gösterir.');
    } else {
      m.className = 'form-feedback msg';
      txt(m, '✔ Acil durum kilidi temizlendi.');
    }
  }, 2000);
}

function quickWater(on) {
  const m = el('quickMsg');
  m.className = 'form-feedback';
  txt(m, on ? 'Sulama komutu gönderildi — cihaz onayı bekleniyor…'
            : 'Durdurma komutu gönderildi…');
  sendCmd('waterPump', on ? 'on' : 'off');
}
