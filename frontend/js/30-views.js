/* Görünümler — yönlendirme, panel, bitki, kontrol, ayarlar
 *
 * ══ DÖRT HEDEF + ALT SAYFALAR ═════════════════════════════════════════════
 * Üstte dört sekme (Bahçem · Bitkim · Kontrol · Ayarlar) durur; Ayarlar bir
 * kategori listesidir ve her kategori kendi alt sayfasını açar.
 *
 * Alt sayfalar da birer `.page`: yönlendirici tek bir mekanizma kullanır ve
 * "hangi sekme etkin" sorusu `ROOT_OF` tablosuyla cevaplanır. İkinci bir
 * gizle/göster düzeneği yazmak, iki düzenekten birinin unutulmasıyla biterdi.
 */

let currentView = 'home';

/// Alt sayfa → hangi sekme etkin görünecek.
const ROOT_OF = {
  'set-crop': 'settings', 'set-hardware': 'settings', 'set-automation': 'settings',
  'set-network': 'settings', 'set-appearance': 'settings', 'set-account': 'settings',
  'set-system': 'settings',
};

/// Sekmeye/alt sayfaya girildiğinde çekilecek veri.
///
/// Hepsini açılışta çekmek AP modunda gözle görülür bir gecikme yaratır;
/// LittleFS'ten okuma ve JSON üretimi ücretsiz değildir.
const ON_ENTER = {
  'set-crop':       () => renderCropSettings(),
  'set-hardware':   () => loadConfig(),
  'set-automation': () => { loadConfig(); loadRules(); },
  'set-system':     () => { loadConfig(); loadDiagnostics(); },
  'plant':          () => loadHistory(),
};

function switchView(name) {
  currentView = name;
  const root = ROOT_OF[name] || name;

  document.querySelectorAll('.tab-btn').forEach((t) => {
    const on = t.dataset.view === root;
    t.classList.toggle('active', on);
    t.setAttribute('aria-selected', on ? 'true' : 'false');
  });
  document.querySelectorAll('.page').forEach((p) => {
    p.classList.toggle('hidden', p.id !== 'v-' + name);
  });

  // Alt sayfaya girerken üste dön: yarısından açılan bir sayfa, kullanıcının
  // başlığı görmediği bir sayfadır.
  window.scrollTo(0, 0);

  const fn = ON_ENTER[name];
  if (fn) fn();
}

/// Ayarlar kategorisini açar.
const openSettings = (section) => switchView('set-' + section);

// ── Çizim motoru ───────────────────────────────────────────────────────────

function render() {
  const s = store.state;
  if (!s) return;

  // ── TAVSİYELER TEK KEZ HESAPLANIR ────────────────────────────────────────
  // Hem kahraman kart hem liste aynı kümeye bakar. İkisi ayrı ayrı
  // hesaplasaydı yalnızca iki kat iş yapılmakla kalmaz, aralarındaki
  // "aynı cümleyi iki kez yazma" kuralı da uygulanamazdı.
  store.advice = buildAdvice().filter((a) => a.level !== 'ok');

  renderHeadline(s);
  renderStatus(s);
  renderSafetyBar(s);
  renderVitals(s);
  renderSensors(s);
  renderActuators(s);
  renderRunning(s);
  renderNetwork(s);
  renderInterlocks(s);
  renderAdvice();
  renderQuickMsg();
}

/// Aktif güvenlik kilitlerini Kontrol ekranında listeler.
///
/// Cihaz `interlockMask`'ı her pakette yayınlıyordu ama arayüz onu hiç
/// çözmüyordu. Kullanıcının "neden çalışmıyor" sorusunun cevabı telemetride
/// hazır duruyordu ve gösterilmiyordu (TASK-074).
function renderInterlocks(s) {
  const host = el0('interlockList');
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

// ── Kahraman kart ──────────────────────────────────────────────────────────
//
// Kullanıcının bakması gereken TEK yer. Halka genel sağlığı, başlık ne
// olduğunu, alt satır ne yapılacağını söyler.
//
// ══ HALKADAKİ ORAN NE ANLATIR ══════════════════════════════════════════════
// Gelişim yolculuğundaki dönem ilerlemesi — uydurma bir "sağlık puanı"
// DEĞİL. Oran hesaplanamıyorsa (son dönem, tutarsız veri, ürün yok) halka
// yalnızca durum rengiyle dolar ve içinde sayı görünmez. Bir sistemin
// bilmediği şeyi yüzdeyle söylemesi, güven kaybının en hızlı yoludur.

/// Bitki adının altındaki canlılık rozeti.
///
/// ── HAREKET BİR İDDİADIR ────────────────────────────────────────────────────
/// Animasyon "sistem şu anda yaşıyor ve bitkin büyüyor" der. Bunu bayat
/// veriyle söylemek, kullanıcının donmuş bir ekrana bakıp her şeyin yolunda
/// olduğunu sanmasına yol açar — projenin kaçındığı tam olarak bu.
///
/// Bu yüzden rozet YALNIZCA şu üçü birden doğruyken hareket eder:
///   · bağlantı canlı (telemetri akıyor)
///   · sistem duraklatılmamış (acil durum / güvenli mod yok)
///   · bir ürün seçili
/// Aksi hâlde donar ve NEDENİNİ yazar.
function renderLife(s, level) {
  const box = el0('heroLife');
  const label = el0('heroLifeText');
  if (!box || !label) { return; }

  if (!cropSelected()) { box.hidden = true; return; }
  box.hidden = false;

  // Dönem adı HEMEN ÜSTTEKİ satırda ("Çilek · Çiçeklenme dönemi") zaten
  // yazıyor; rozet onu tekrarlamaz. Rozetin tek işi CANLILIK: sistem şu an
  // yürüyor mu, yürümüyorsa neden.
  let mood = 'growing';
  let text = 'Büyüyor';

  if (!store.linked) {
    mood = 'stale';
    text = 'Bağlantı yok';
  } else if (s.safety && s.safety.latched) {
    mood = 'paused';
    text = 'Acil durum — durduruldu';
  } else if (s.system.mode === 'safe' || s.system.mode === 'emergency') {
    mood = 'paused';
    text = 'Güvenlik nedeniyle durduruldu';
  } else if (s.automation && s.automation.mode === 0) {
    // Manuel modda hiçbir program çalışmaz. "Büyüyor" demek, cihazın
    // yapmadığı bir işi yaptığını söylemek olurdu.
    mood = 'manual';
    text = 'Elle yönetiliyor';
  } else if (level === 'bad' || level === 'warn') {
    mood = 'attention';
    text = 'İlginizi bekliyor';
  }

  box.className = 'hero-life life-' + mood;
  txt(label, text);
}

function renderHeadline(s) {
  const box  = el0('headline');
  const icon = el0('headlineIcon');
  const ttl  = el0('headlineTitle');
  const sub  = el0('headlineSub');
  const crop = el0('heroCrop');
  const ring = el0('heroRing');
  const pctEl = el0('heroPct');
  const capEl = el0('heroCap');
  if (!box) return;

  let level = 'ok', ic = '🌿', title = 'Her şey yolunda', detail = '';

  if (s.safety && s.safety.latched) {
    level = 'bad'; ic = '🛑';
    title = 'Acil durum kilidi aktif';
    detail = errText(s.safety.reason) || 'Sizin onayınız gerekiyor';
  } else if (s.system.mode === 'emergency') {
    level = 'bad'; ic = '🛑';
    title = 'Sistem acil durumda';
    detail = 'Kontrol ekranından durumu temizleyin';
  } else if (s.system.mode === 'safe') {
    level = 'bad'; ic = '⚠️';
    title = 'Güvenli moddayız';
    detail = 'Cihazlar kilitli — Ayarlar → Sistem ekranında neden yazıyor';
  } else if (s.safety && s.safety.interlocks) {
    level = 'warn'; ic = '⚠️';
    title = 'Bir güvenlik kilidi devrede';
    detail = errText(s.safety.reason) || 'Ayrıntı Kontrol ekranında';
  } else if (s.system.mode === 'degraded') {
    level = 'warn'; ic = '⚠️';
    title = 'Sistem kısıtlı çalışıyor';
    detail = 'Bazı işlevler devre dışı — Ayarlar → Sistem ekranına bakın';
  } else {
    // ── AYNI CÜMLE İKİ KEZ YAZILMAZ ────────────────────────────────────────
    //
    // "Bugün yapılacaklar" listesi artık HER ZAMAN ekranda ve maddelerin
    // tam metnini o taşıyor. Kahraman kartın işi bu yüzden ÖZETLEMEK:
    // durum + kaç iş var. Madde metnini buraya da basmak, otuz piksel
    // arayla aynı cümleyi iki kez yazmak olurdu.
    //
    //   0 madde → "sağlıklı"
    //   N madde → durum + sayı; ayrıntı hemen aşağıdaki listede
    const advice = store.advice || buildAdvice().filter((a) => a.level !== 'ok');
    const worst = advice.find((a) => a.level === 'bad') || advice[0];

    if (advice.length === 1 && advice[0].key === 'noCrop') {
      // Tek eksik ürün seçimi ve düğmesi HEMEN ALTINDA. "Ayarlar → Bahçe
      // bölümüne gidin" demek, kullanıcıyı yanındaki düğmeden uzaklaştıran
      // bir talimat olurdu.
      ic = '🌱';
      title = 'Başlayalım';
      detail = 'Ne yetiştirdiğinizi seçin; cihaz hedefleri ve sulama programını kursun.';
    } else if (advice.length) {
      level = worst.level;
      ic = worst.level === 'bad' ? '⚠️' : '🌤️';
      title = worst.level === 'bad' ? 'Dikkat gerekiyor' : 'Birkaç küçük not var';
      detail = advice.length === 1
        ? 'Bugün için bir iş var — hemen aşağıda.'
        : `Bugün için ${advice.length} iş var — hemen aşağıda.`;
    } else if (cropSelected()) {
      title = 'Bitkiniz sağlıklı gelişiyor';
      detail = 'Bütün ölçümler hedef aralıkta. Yapmanız gereken bir şey yok.';
    } else {
      ic = '🌱';
      title = 'Başlayalım';
      detail = 'Ne yetiştirdiğinizi seçin; cihaz hedefleri ve sulama programını kursun.';
    }
  }

  box.className = 'hero headline-' + level;
  txt(icon, ic);
  txt(ttl, title);
  txt(sub, detail);

  // Üst satır: ne yetiştiriyoruz, hangi dönemdeyiz.
  if (crop) {
    txt(crop, cropSelected()
      ? `${cropDisplayName()} · ${STAGE_TEXT[store.crop.stage] || store.crop.stage} dönemi`
      : 'Bahçem');
  }

  renderLife(s, level);

  // Halka: renk durumdan, oran yolculuktan.
  if (ring) {
    const j = (typeof journeyInfo === 'function') ? journeyInfo() : null;
    const pct = j && j.pct !== null ? j.pct : null;
    const col = level === 'bad' ? 'var(--danger)' : (level === 'warn' ? 'var(--warn)' : 'var(--accent)');
    ring.style.setProperty('--ring-color', col);
    ring.style.setProperty('--ring-pct', (pct === null ? 100 : pct) + '%');
    ring.classList.toggle('no-pct', pct === null);
    txt(pctEl, pct === null ? '' : pct + '%');
    txt(capEl, pct === null ? '' : 'dönem');
  }
}

function renderStatus(s) {
  const modeName = MODE_TEXT[s.system.mode] || s.system.mode;
  txt(el0('sMode'), modeName);
  txt(el0('headMode'), modeName);

  // ── MOD ROZETİ YALNIZCA ANORMALKEN ───────────────────────────────────────
  // "Çalışıyor" yazan kalıcı bir rozet hiçbir soruyu cevaplamaz; yalnızca
  // dar başlıkta yer kaplar ve gerçekten bir şey söylediği anda (KISITLI,
  // GÜVENLİ, ACİL) fark edilmesini zorlaştırır. Normalken gizliyoruz —
  // görünmesi, bir şeyin değiştiğinin işareti olsun.
  const hmp = el0('headModePill');
  if (hmp) {
    const normal = s.system.mode === 'running';
    hmp.className = 'mode-pill ' +
      (s.system.mode === 'emergency' ? 'emergency' : (s.system.mode === 'safe' ? 'safe' : ''));
    show(hmp, !normal);
  }

  txt(el0('sUptime'), fmtUptime(s.system.uptimeMs));
  txt(el0('sHeap'), Math.round(s.system.freeHeap / 1024) + ' KB');
  txt(el0('sClock'), s.time.valid
    ? new Date(s.time.epoch * 1000).toLocaleTimeString('tr-TR',
        { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    : 'Saat ayarlı değil');
}

function renderSafetyBar(s) {
  const bar = el0('safetyBar');
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

// ── Panel: "Bugün" kompakt ölçümler ────────────────────────────────────────
//
// Panelde SEKİZ kart bir özet değil, bir duvardır — hele dördü "Yok" diyorsa.
// Burada takılı olanlardan en fazla dördü, öncelik sırasına göre gösterilir;
// tam kartlar Bitkim ekranındadır.

function renderVitals(s) {
  const host = el0('vitals');
  if (!host) return;

  const byId = {};
  s.sensors.forEach((sn) => { byId[sn.id] = sn; });

  const picked = [];
  VITAL_ORDER.forEach((id) => {
    if (picked.length >= VITAL_COUNT) return;
    const sn = byId[id];
    if (!sn || sn.quality === 'notPresent') return;   // takılı değilse yer kaplamaz
    picked.push(sn);
  });

  if (!picked.length) {
    host.innerHTML = `<div class="card empty" style="grid-column:1/-1;">
        <span class="empty-icon">📡</span>
        <div>Henüz ölçüm alınamadı.<br>
        <span class="small">Ayarlar → Donanım bölümünden takılı sensörleri işaretleyin.</span></div>
      </div>`;
    return;
  }

  host.innerHTML = picked.map((sn) => {
    const st = sensorStatus(sn);
    const meta = SENSOR_META[sn.id] || { short: sn.id, icon: '•' };
    return `
      <button class="vital lvl-${st.level}${sn.id === 'level' ? ' is-word' : ''}" data-goto="plant" type="button">
        <span class="vital-top"><span class="vital-icon">${meta.icon || ''}</span>${esc(meta.short || st.label)}</span>
        <span class="vital-val">${esc(st.value)}${st.unit ? `<span class="u">${esc(st.unit)}</span>` : ''}</span>
        <span class="vital-state"><span class="dot dot-${st.level}"></span>${esc(st.word)}</span>
      </button>`;
  }).join('');
}

// ── Panel: "Şu anda" çalışanlar ────────────────────────────────────────────

function renderRunning(s) {
  const host = el0('runningCard');
  if (!host) return;

  const on = s.actuators.filter((a) => a.on && ACT_META[a.id]);
  const total = s.actuators.filter((a) => ACT_META[a.id]).length;

  if (!on.length) {
    host.innerHTML = `<div class="empty" style="padding:14px 8px;">
        <span class="empty-icon">😴</span>
        <div>Şu anda hiçbir cihaz çalışmıyor.</div>
      </div>`;
    return;
  }

  host.innerHTML = on.map((a) => {
    const meta = ACT_META[a.id];
    const src = SRC_SHORT[a.source] || '';
    return `<div class="running-row">
        <span class="running-ico">${meta.icon}</span>
        <span class="running-name">${esc(meta.name)}</span>
        ${src ? `<span class="badge">${esc(src)}</span>` : ''}
        <span class="badge ok">Açık</span>
      </div>`;
  }).join('') +
  (total > on.length
    ? `<p class="muted small" style="margin-top:10px;">Diğer ${total - on.length} cihaz kapalı.</p>`
    : '');
}

// ── Bitkim: ayrıntılı ölçüm kartları ───────────────────────────────────────

function renderSensors(s) {
  const g = el0('sensorGrid');
  if (!g) return;

  // Takılı olmayanlar EN SONA. Silmiyoruz — kullanıcı "ışık sensörü nerede"
  // diye sorabilmeli ve cevabı "takılı değil" olmalı — ama canlı ölçümlerin
  // arasına serpiştirilmiş hâlde değil.
  const list = s.sensors.slice().sort((a, b) =>
    (a.quality === 'notPresent' ? 1 : 0) - (b.quality === 'notPresent' ? 1 : 0));

  g.innerHTML = list.map((sn) => {
    const st = sensorStatus(sn);
    const meta = SENSOR_META[sn.id] || { label: sn.id, unit: '', min: 0, max: 100, icon: '' };

    // Ölçek çubuğu: değerin sensörün fiziksel aralığındaki yeri. Hedef band
    // varsa arkasında gölgelenir — "neredeyim" bir bakışta görülsün.
    let scale = '';
    if (st.level !== 'off' && sn.quality !== 'fault') {
      const num = sn.id === 'level' ? Math.round(sn.value) : Number(sn.value);
      const lo = sn.id === 'level' ? 0 : meta.min;
      const hi = sn.id === 'level' ? 2 : meta.max;
      const pct = Math.max(2, Math.min(100, Math.round(((num - lo) / (hi - lo)) * 100)));

      let band = '';
      if (st.band) {
        const bl = Math.max(0, Math.min(100, ((st.band.min - lo) / (hi - lo)) * 100));
        const bw = Math.max(2, Math.min(100 - bl, ((st.band.max - st.band.min) / (hi - lo)) * 100));
        band = `<span class="sensor-band" style="left:${bl}%;width:${bw}%"></span>`;
      }
      scale = `<div class="sensor-scale">${band}
                 <span class="sensor-fill" style="width:${pct}%"></span>
                 <span class="sensor-mark" style="left:calc(${pct}% - 1.5px)"></span>
               </div>`;
    }

    return `
      <div class="sensor-card lvl-${st.level}${sn.id === 'level' ? ' is-word' : ''}">
        <div class="sensor-head">
          <span class="sensor-title">${meta.icon || ''} ${esc(st.label)}</span>
          <span class="badge ${st.level === 'off' ? 'dim' : st.level}">${esc(st.word)}</span>
        </div>
        <div class="sensor-body">
          <span class="sensor-val">${esc(st.value)}</span>
          <span class="sensor-unit">${esc(st.unit)}</span>
        </div>
        ${scale}
        <div class="sensor-note">${esc(st.text)}</div>
      </div>`;
  }).join('');
}

// ── Kontrol: cihaz satırları ───────────────────────────────────────────────
//
// ══ İYİMSER GÜNCELLEME YOK ═════════════════════════════════════════════════
// Düğme durum DEĞİŞTİRMEZ; komut gönderir. Kart yalnızca cihazdan gelen
// `state` paketi röleyi açık bildirdiğinde açık görünür.
//
// ══ "KİM AÇTI" ═════════════════════════════════════════════════════════════
// `a.source` her pakette geliyordu ve hiç okunmuyordu. Kullanıcı elle açtığı
// bir cihazla programın açtığını ayırt edemiyordu — override yaptığını
// anlamasının hiçbir yolu yoktu.

function renderActuators(s) {
  const g = el0('actuatorGrid');
  if (!g) return;

  const pendingTargets = new Set([...store.pending.values()].map((p) => p.target));
  g.innerHTML = '';

  s.actuators.forEach((a) => {
    const meta = ACT_META[a.id];
    if (!meta) return;

    const pending = pendingTargets.has(a.id);
    // "Bekliyor…" kimi beklediğini söylemiyordu ve kullanıcı bunu kendisinin
    // onaylaması gereken bir şey sanıyordu. Beklenen şey CİHAZIN röleyi
    // gerçekten sürmesi; metin de bunu söylüyor.
    const stateText = pending ? 'Cihaz uyguluyor…' : (a.on ? 'Çalışıyor' : 'Kapalı');
    const src = a.on ? (SRC_TEXT[a.source] || '') : '';

    // Neden açılmıyor? Önce az önceki reddin nedeni, yoksa cihazın bildirdiği
    // kalıcı engel.
    let why = '', whyBad = false;
    if (store.rejected && store.rejected.target === a.id &&
        Date.now() - store.rejected.at < 8000) {
      why = store.rejected.text; whyBad = true;
    } else if (a.block) {
      why = errText(a.block);
    }

    const wired = actuatorEnabled(a.id);

    const card = document.createElement('div');
    card.className = 'device' + (pending ? ' pending' : (a.on ? ' on' : ''));
    card.innerHTML = `
      <span class="device-icon">${meta.icon}</span>
      <div class="device-body">
        <div class="device-name">${esc(meta.name)}</div>
        <div class="device-state">
          <span class="dot ${pending ? 'dot-warn' : (a.on ? 'dot-ok' : 'dot-off')}"></span>
          <span>${stateText}</span>
          ${src ? `<span class="device-src">· ${esc(src)}</span>` : ''}
        </div>
      </div>`;

    const btn = document.createElement('button');
    btn.className = 'power' + (pending ? ' is-pending' : (a.on ? ' is-on' : ''));
    btn.type = 'button';
    btn.setAttribute('aria-label', `${meta.name} — ${a.on ? 'kapat' : 'çalıştır'}`);
    btn.innerHTML = pending
      ? '<svg viewBox="0 0 24 24" width="21" height="21" fill="none" stroke-width="2.4"><path d="M21 12a9 9 0 1 1-6.2-8.6"></path></svg>'
      : '<svg viewBox="0 0 24 24" width="21" height="21" fill="none" stroke-width="2.4" stroke-linecap="round"><path d="M18.4 6.6a9 9 0 1 1-12.8 0"></path><line x1="12" y1="2.5" x2="12" y2="12"></line></svg>';
    btn.disabled = pending || !store.linked;
    btn.onclick = () => sendCmd(a.id, a.on ? 'off' : 'on');
    card.appendChild(btn);

    // Açıklama satırları kartın ALTINA, tam genişlikte: bir cümleyi 42 px'lik
    // sütuna sıkıştırmak onu okunmaz yapardı.
    if (!wired) {
      const n = document.createElement('div');
      n.className = 'device-why';
      n.textContent = 'Bu cihaz "bağlı" olarak işaretli değil — Ayarlar → Donanım';
      card.appendChild(n);
    }
    if (why) {
      const n = document.createElement('div');
      n.className = 'device-why' + (whyBad ? ' bad' : '');
      n.textContent = why;
      card.appendChild(n);
    }

    // Bakım sayaçları yalnızca uzman modunda: birincil kontrol yüzeyinde
    // "Toplam çalışma 6g 0sn" hiç kimsenin sorduğu soru değil.
    if (isExpert()) {
      const m = document.createElement('div');
      m.className = 'device-meta';
      m.innerHTML = `<span>Toplam: ${fmtUptime(a.runMs)}</span><span>Açılma: ${a.cycles || 0}</span>`;
      card.appendChild(m);
    }

    g.appendChild(card);
  });
}

function renderNetwork(s) {
  const c = el0('netCard');
  if (!c) return;
  const n = s.network;
  c.innerHTML = `
    <div class="net-grid">
      <div><span class="muted small">Durum</span>
        <div class="net-val accent">${esc(NET_TEXT[n.state] || n.state)}</div></div>
      <div><span class="muted small">Bağlı Ağ</span>
        <div class="net-val" title="${esc(n.ssid || '')}">${esc(clipText(n.ssid) || '—')}</div></div>
      <div><span class="muted small">IP Adresi</span>
        <div class="net-val mono">${esc(n.ip || '—')}</div></div>
      <div><span class="muted small">Sinyal</span>
        <div class="net-val">${n.rssi ? sigBars(n.rssi) + ' ' + esc(n.rssi) + ' dBm' : '—'}</div></div>
    </div>
    ${netSetupNote(n)}
    ${netRetryNote(n)}
    ${n.apActive && !n.setupReboot ? `<div class="info-note">Kurulum ağı (AP) açık — <b>${esc(n.apClients)}</b> cihaz bağlı</div>` : ''}
    ${n.lastError && !n.setupReboot ? `<div class="warn-note small">Son hata: ${esc(errText(n.lastError))}</div>` : ''}`;
}

/// Bekleyen yeniden bağlanma denemesinin geri sayımı.
///
/// Kurulum oturumunda GÖSTERİLMEZ: orada `netSetupNote()` zaten daha
/// açıklayıcı bir şey söylüyor ve iki kutu üst üste gürültü olur.
function netRetryNote(n) {
  if (n.provisioning || n.setupReboot) { return ''; }
  if (n.state !== 'backoff' && n.state !== 'apFallback') { return ''; }
  if (!n.retryIn) { return ''; }
  return `<div class="info-note">Yeniden bağlanma denemesi <b>${esc(n.retryIn)} sn</b> içinde` +
         ` — beklemek istemiyorsanız <b>Şimdi Dene</b>.</div>`;
}

/// Kurulum oturumunun ekrandaki karşılığı (§8.4).
///
/// "Bağlandı" ile "kurulum bitti, cihaz yeniden başlıyor" farklı şeylerdir:
/// ikincisinde kurulum ağı kapanacak ve kullanıcının kendi ağına dönmesi
/// GEREKİYOR. Bunu söylemeyen bir ekran, kullanıcıyı erişilemez bir cihazla
/// baş başa bırakır.
function netSetupNote(n) {
  if (n.setupReboot) {
    const addr = n.ip && n.ip !== '0.0.0.0' ? `<b class="mono">http://${esc(n.ip)}</b>` : 'cihaz ekranındaki adres';
    return `<div class="info-note">✔ Kurulum tamamlandı — cihaz yeniden başlıyor` +
           `${n.rebootIn ? ` (${esc(n.rebootIn)} sn)` : ''}. ` +
           `Telefonunuzu kendi ağınıza alıp ${addr} adresine gidin.</div>`;
  }
  if (n.provisioning && n.state === 'connecting') {
    return '<div class="info-note">Kurulum: ağa bağlanılıyor… Bağlantı kurulunca cihaz yeniden başlayacak.</div>';
  }
  if (n.provisioning && (n.state === 'backoff' || n.state === 'apFallback')) {
    return '<div class="warn-note small">Kurulum: bağlanılamadı, yeniden deneniyor — ağ adı ve parolayı kontrol edin.</div>';
  }
  return '';
}

/// Sinyal gücünü dört çubukla anlatır. "-67 dBm" kimseye bir şey söylemez;
/// üç dolu çubuk söyler.
function sigBars(rssi) {
  const n = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : 1;
  let s = '<span class="sig">';
  for (let i = 1; i <= 4; i++) s += `<i class="${i <= n ? 'lit' : ''}"></i>`;
  return s + '</span>';
}

// ── Ayarlar: bağlı donanım + otomatik mod ──────────────────────────────────

/// "Takılı sensörler" — ISSUE-035'in arayüz karşılığı.
///
/// Bu bölüm hiç yoktu: pH, EC, nem, hava sıcaklığı ve ışık `enabled = 0`
/// doğuyor ve açılmasının HİÇBİR yolu yoktu. Kullanıcı sensörü fiziksel
/// olarak taksa bile arayüzde sonsuza kadar "takılı değil" yazıyordu.
function renderSensorCard() {
  const host = el0('sensorCard');
  if (!host) return;

  const list = (store.config && store.config.sensors) || [];
  if (!list.length) {
    host.innerHTML = '<p class="muted">Sensör listesi alınamadı — firmware güncel mi?</p>';
    return;
  }

  host.innerHTML = OPTIONAL_SENSORS.map((id) => {
    const s = list.find((x) => x.id === id);
    if (!s) return '';
    const meta = SENSOR_META[id] || { label: id, icon: '' };
    return `
      <label class="hw-row">
        <input type="checkbox" data-sens="${esc(id)}" ${s.enabled ? 'checked' : ''}>
        <span class="hw-icon">${meta.icon || ''}</span>
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
  const m = el0('sensMsg');
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
  const host = el0('hardwareCard');
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
  const m = el0('hwMsg');
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
      ? `✔ ${ACT_META[id].name} bağlı olarak işaretlendi. Sulama programını yeniden kurmak için Ayarlar → Bahçe bölümünden "Ürünü / Dönemi Değiştir" deyin.`
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
function renderAutomationSimple(c) {
  const host = el0('automationForm');
  if (!host) return;

  if (!c.automation) {
    host.innerHTML = '<p class="err">Cihaz otomasyon ayarını döndürmüyor — firmware güncel mi?</p>';
    txt(el0('autoBadge'), '—');
    return;
  }

  let isAuto = c.automation.mode === 'auto';
  const badge = el0('autoBadge');
  const paint = () => {
    txt(badge, isAuto ? 'OTOMATİK' : 'MANUEL');
    if (badge) badge.className = 'section-badge' + (isAuto ? '' : ' badge-auto');
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
        <input id="autoSwitch" type="checkbox" ${isAuto ? 'checked' : ''} aria-label="Otomatik çalışma">
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

  el0('autoSwitch').onchange = async (ev) => {
    const want = ev.target.checked;
    const m = el0('autoMsg');

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

// ── Acil durumu temizleme (TASK-074) ───────────────────────────────────────
//
// ESKİ DAVRANIŞ: düğme komutu kuyruğa atıyor, cihaz `ACCEPTED` (= kuyruğa
// alındı) diyordu. Cihaz sonra `acknowledge()` içinde koşulları kontrol edip
// temizlemeyi REDDEDİYOR ve bunu yalnızca olay günlüğüne yazıyordu.
//
// Şimdi üç aşama var:
//   1. ÖNCE KONTROL — engel varsa komut hiç gönderilmez, ne yapılacağı yazılır
//   2. gönder
//   3. SONRA DOĞRULA — 2 sn içinde temizlenmediyse bunu söyle
function clearEmergency() {
  const m = el0('estopMsg');
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
           '. Su seviyesi sensörü takılı değilse Ayarlar → Otomasyon → Güvenlik Eşikleri ' +
           'bölümünden "su seviyesi sensörü okunamıyorsa pompa kilitli kalsın" ' +
           'seçeneğini kapatabilirsiniz.');
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
        : 'Cihaz temizlemeyi reddetti. Ayarlar → Sistem → Olay Günlüğü nedenini gösterir.');
    } else {
      m.className = 'form-feedback msg';
      txt(m, '✔ Acil durum kilidi temizlendi.');
    }
  }, 2000);
}

/// ── "ONAY" KİMDEN? CİHAZDAN. ────────────────────────────────────────────────
///
/// Eski metin "cihaz onayı bekleniyor…" diyordu ve orada kalıyordu.
/// Kullanıcının okuduğu şey "birinin bunu onaylaması gerekiyor, kimin?"
/// oluyordu — sanki bir yerde bekleyen bir izin varmış gibi.
///
/// Gerçekte olan: komut cihaza gidiyor, cihaz güvenlik kilitlerinden
/// geçiriyor ve RÖLEYİ sürüyor; arayüz rölenin gerçekten kapandığını
/// telemetride görene kadar "çalışıyor" demiyor (P5 — iyimser güncelleme
/// yasak). Yani beklenen şey bir onay değil, GERÇEKLEŞME.
///
/// `store.quick` bu yüzden var: mesaj artık asılı kalmıyor, sonucu söylüyor.
function quickWater(on) {
  store.quick = { want: on, at: Date.now() };
  sendCmd('waterPump', on ? 'on' : 'off');
  renderQuickMsg();
}

/// Hızlı sulama düğmelerinin durum satırı. Her telemetri paketinde yeniden
/// değerlendirilir; "gönderildi" mesajı sonsuza kadar ekranda kalmaz.
function renderQuickMsg() {
  const m = el0('quickMsg');
  if (!m) { return; }

  const q = store.quick;
  if (!q) { m.className = 'form-feedback'; txt(m, ''); return; }

  const say = (cls, s) => { m.className = 'form-feedback' + (cls ? ' ' + cls : ''); txt(m, s); };

  // Hâlâ cihazda mı? Bekleyen komut ZAMAN AŞIMINA uğramadıkça sonuç
  // kesinleşmez, o yüzden süre dolumu beklemeyi kesmez.
  const waiting = [...store.pending.values()].some((p) => p.target === 'waterPump');

  // Sonuç bildirildikten sonra satır sonsuza kadar durmaz: 12 sn sonra
  // temizlenir. Kalıcı gerçek zaten cihaz kartında ve ölçümlerde duruyor;
  // burada asılı kalan eski bir "✔ Pompa çalışıyor", pompa çoktan durmuşken
  // yanlış bilgi hâline gelirdi.
  if (!waiting && Date.now() - q.at > 12000) {
    store.quick = null;
    m.className = 'form-feedback';
    txt(m, '');
    return;
  }

  // 1) Cihaz reddetti mi?
  if (store.rejected && store.rejected.target === 'waterPump' &&
      store.rejected.at >= q.at) {
    say('err', 'Cihaz komutu uygulamadı — ' + store.rejected.text);
    return;
  }

  // 2) Hâlâ cihazda mı?
  if (waiting) {
    say('', q.want
      ? 'Komut cihaza iletildi. Cihaz güvenlik kilitlerini kontrol ediyor; pompa gerçekten çalışınca burada yazacak.'
      : 'Durdurma komutu cihaza iletildi…');
    return;
  }

  // 3) Gerçek röle durumu ne diyor? Tek doğruluk kaynağı bu.
  const pump = store.state && store.state.actuators.find((a) => a.id === 'waterPump');
  if (!pump) { say('', ''); return; }

  if (pump.on === q.want) {
    say('msg', q.want ? '✔ Pompa çalışıyor.' : '✔ Pompa durdu.');
    return;
  }

  // Komut kabul edildi ama röle beklenen durumda değil. Cihaz bir nedenle
  // sürmedi (asgari çalışma süresi, bekleme, güvenlik).
  //
  // Neden `lastBlock`: `pump.block` bir sonraki değerlendirmede sıfırlanır ve
  // kullanıcı ekrana baktığında çoktan silinmiş olur. Bu komuttan SONRA
  // gelmiş bir engel varsa gerçek nedeni odur.
  const lb = store.lastBlock.waterPump;
  const code = pump.block || (lb && lb.at >= q.at ? lb.code : 0);
  if (code) {
    say('err', 'Cihaz uygulamadı — ' + errText(code));
    return;
  }

  // ── EN SIK YAŞANAN DURUM: OTOMASYON DEVRALDI ───────────────────────────
  //
  // Komut kabul edildi, röle bir an kapandı ve otomasyon motoru bir sonraki
  // turda kendi programını yeniden dayattı. Kullanıcı açısından görünen
  // şey "bastım, hiçbir şey olmadı" — oysa cihaz komutu uyguladı, sonra
  // program geri aldı.
  //
  // `source` bunu tam olarak söylüyor ve telemetride zaten var: son süren
  // OTOMASYON ise suçlu programdır, gizemli bir arıza değil.
  if (pump.source === 1) {
    say('err', 'Sulama programı pompayı yönetiyor ve komutunuzu geri aldı. ' +
               'Elle sulamak için Ayarlar → Otomatik çalışma bölümünden programı ' +
               'geçici olarak kapatın.');
    return;
  }

  say('err', 'Cihaz komutu aldı ancak pompa istenen duruma geçmedi. ' +
             'Kontrol ekranı nedenini gösterir.');
}
