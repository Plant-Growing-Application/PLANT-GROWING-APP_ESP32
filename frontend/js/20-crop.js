/* Ürün profili — hedef bantlar, tavsiyeler, kurulum sihirbazı (TASK-070)
 *
 * ══ BU DOSYANIN FİKRİ ═════════════════════════════════════════════════════
 * Kullanıcı "pH 6.1" sayısına bakıp iyi mi kötü mü olduğunu bilmek zorunda
 * DEĞİLDİR. Cihaz hangi ürünü yetiştirdiğimizi biliyor, o ürünün o dönemdeki
 * hedef bandını da biliyor — o hâlde "iyi" veya "besin ekleyin" diyebilir.
 *
 * Hedef bantlar `/api/crop` ile bir kez alınır ve `store.crop.targets`
 * içinde durur. Ürün seçilmemişse band YOKTUR ve arayüz sayıyı yorumsuz
 * gösterir; UYDURMA bir "ideal aralık" göstermek, yetiştirilen bitkiyle
 * ilgisi olmayan bir tavsiye vermek olurdu.
 */

// ── Veri çekme ─────────────────────────────────────────────────────────────

async function loadCrop() {
  try {
    store.crop = await api('/api/crop');
  } catch (e) {
    store.crop = null;
  }

  // `CUSTOM` iken hangi profilden türediğini ADIYLA söylemek istiyoruz
  // ("Özel (Çilek temelli)"). Ad yalnızca katalogda var; henüz çekilmediyse
  // burada çekiyoruz. Aksi hâlde kullanıcıya ham anahtar ("strawberry")
  // gösterilirdi.
  if (store.crop && store.crop.crop === 'custom' && !store.catalog) {
    try { await loadCatalog(); } catch (e) { /* ad yerine anahtar gösterilir */ }
  }

  renderCropCard();
  renderCropSettings();
  render();
}

/// Katalog SABİTTİR; bir kez alınır ve saklanır (~5 KB).
async function loadCatalog() {
  if (store.catalog) return store.catalog;
  store.catalog = await api('/api/crops');
  return store.catalog;
}

const cropTargets = () => (store.crop && store.crop.targets) || null;

const cropSelected = () =>
  !!(store.crop && store.crop.crop && store.crop.crop !== 'none');

/// Aktif ürünün görünen adı. `CUSTOM` iken hangi profilden türediğini de
/// söyler — "Özel" tek başına kullanıcıya hiçbir şey anlatmaz.
function cropDisplayName() {
  if (!store.crop) return '—';
  if (store.crop.crop === 'none') return 'Seçilmedi';
  if (store.crop.crop === 'custom') {
    const from = store.crop.derivedFrom;
    const base = from && from !== 'none' ? catalogName(from) : null;
    return base ? `Özel (${base} temelli)` : 'Özel';
  }
  return store.crop.name || store.crop.crop;
}

function catalogName(key) {
  if (!store.catalog || !store.catalog.crops) return key;
  const c = store.catalog.crops.find((x) => x.key === key);
  return c ? c.name : key;
}

// ── Değer yorumlama ────────────────────────────────────────────────────────

/// Bir ölçümü hedef bandına göre yorumlar.
///
/// @return null = yorumlanamaz (band yok) · aksi hâlde {level, low}
///         level: 'ok' | 'warn' | 'bad'
///         low:   true = hedefin ALTINDA, false = ÜSTÜNDE
function bandVerdict(v, band) {
  if (!band || typeof band.min !== 'number' || typeof band.max !== 'number') return null;
  if (!isFinite(v)) return null;

  if (v >= band.min && v <= band.max) return { level: 'ok', low: false };

  // Bandın dışındayız. "Az dışında" ile "çok dışında" ayrımı, kullanıcıyı
  // her küçük sapmada kırmızı uyarıya boğmamak için: band genişliğinin
  // dörtte birine kadar sapma UYARI, ötesi SORUN.
  const width = Math.max(1e-6, band.max - band.min);
  const low   = v < band.min;
  const dist  = low ? band.min - v : v - band.max;
  return { level: dist <= width * 0.25 ? 'warn' : 'bad', low: low };
}

/// Bir sensörün o anki durumunu düz Türkçe olarak döndürür.
///
/// @return { level, value, unit, text, band }
function sensorStatus(sn) {
  const meta = SENSOR_META[sn.id] || { label: sn.id, unit: '', digits: 2 };
  const out = { id: sn.id, label: meta.label, unit: meta.unit, band: null,
                level: 'unknown', value: '—', text: '' };

  // Değer YOKKEN birim de gösterilmez: "Yok lx" saçmadır ve okuyan kişiye
  // sensörün bir değer ürettiği izlenimi verir.
  if (sn.quality === 'notPresent') {
    out.level = 'off';
    out.value = 'Yok';
    out.unit  = '';
    out.text  = 'Bu sensör takılı değil';
    return out;
  }
  if (sn.quality === 'fault') {
    out.level = 'bad';
    out.value = '—';
    out.unit  = '';
    out.text  = 'Sensör okunamıyor — kabloyu kontrol edin';
    return out;
  }

  // Su seviyesi ayrık bir koddur, sayısal band uygulanmaz.
  if (sn.id === 'level') {
    out.unit = '';
    const idx = Math.max(0, Math.min(2, Math.round(sn.value)));
    out.value = LEVEL_TEXT[idx] || '?';
    out.level = idx === 2 ? 'ok' : (idx === 1 ? 'warn' : 'bad');
    out.text  = idx === 2 ? 'Depoda yeterli su var'
              : (idx === 1 ? 'Su azalıyor — yakında ekleyin'
                           : 'Su bitmek üzere — pompa kilitlenir');
    return out;
  }

  const num = Number(sn.value);
  out.value = num.toFixed(meta.digits);

  const targets = cropTargets();
  const key = CROP_TARGET_KEY[sn.id];
  out.band = (targets && key) ? targets[key] : null;

  const v = bandVerdict(num, out.band);

  if (sn.quality === 'stale') {
    out.level = 'warn';
    out.text  = 'Değer bir süredir değişmiyor';
    return out;
  }
  if (sn.quality === 'outOfRange') {
    out.level = 'bad';
    out.text  = 'Değer beklenen aralığın dışında';
    return out;
  }

  if (!v) {
    // Hedef band yok: ürün seçilmemiş veya bu sensörün ürünle ilgisi yok
    // (akış, ışık). Sayıyı gösterip yorum yapmıyoruz.
    out.level = 'ok';
    out.text  = cropSelected() ? '' : 'Ürün seçilince hedef aralık gösterilir';
    return out;
  }

  out.level = v.level;
  const range = `hedef ${trimNum(out.band.min)}–${trimNum(out.band.max)}${meta.unit ? ' ' + meta.unit : ''}`;
  out.text = v.level === 'ok' ? 'İyi · ' + range
           : (v.low ? 'Hedefin altında · ' + range : 'Hedefin üstünde · ' + range);
  return out;
}

// ── "Bugün ne yapmalıyım?" ─────────────────────────────────────────────────
//
// Ölçümlerden TÜRETİLMİŞ, eyleme dönük cümleler. Panelde ham sayı görmek
// isteyen zaten aşağıdaki kartlara bakabilir; buradaki liste "şimdi ne
// yapayım" sorusunun cevabıdır.

function buildAdvice() {
  const s = store.state;
  if (!s || !s.sensors) return [];

  const items = [];
  const byId = {};
  s.sensors.forEach((sn) => { byId[sn.id] = sn; });

  const push = (level, text) => items.push({ level, text });

  // 1) Güvenlik önce: su seviyesi pompayı kilitler.
  const lvl = byId.level;
  if (lvl && lvl.quality === 'ok') {
    const idx = Math.round(lvl.value);
    if (idx <= 0) push('bad', 'Depo boşalmak üzere — hemen su ekleyin. Pompa güvenlik nedeniyle kilitli.');
    else if (idx === 1) push('warn', 'Su seviyesi düşüyor — bugün depoya su ekleyin.');
  } else if (lvl && (lvl.quality === 'fault' || lvl.quality === 'notPresent')) {
    push('bad', 'Su seviyesi sensörü okunamıyor — pompa güvenlik nedeniyle kilitli olabilir.');
  }

  // 2) Acil durum kilidi.
  if (s.safety && s.safety.latched) {
    push('bad', 'Acil durum kilidi aktif: ' + (errText(s.safety.reason) || 'nedeni bilinmiyor') +
                '. Sorunu giderip Kontrol ekranından temizleyin.');
  }

  // 3) Ürün seçilmemişse en faydalı tavsiye budur.
  if (!cropSelected()) {
    push('warn', 'Henüz ürün seçilmedi. Ayarlar → "Ne yetiştiriyorsunuz?" adımını tamamlayın; sistem hedefleri ve sulama programını sizin için kursun.');
    return items;
  }

  // 4) Besin ve pH — kullanıcının elle müdahale ettiği iki şey.
  const ec = byId.ec;
  if (ec && ec.quality === 'ok') {
    const st = sensorStatus(ec);
    if (st.level !== 'ok' && st.band) {
      push(st.level, Number(ec.value) < st.band.min
        ? `Besin yoğunluğu düşük (EC ${Number(ec.value).toFixed(2)}, hedef ${trimNum(st.band.min)}–${trimNum(st.band.max)}). Gübre ekleyin.`
        : `Besin yoğunluğu yüksek (EC ${Number(ec.value).toFixed(2)}, hedef ${trimNum(st.band.min)}–${trimNum(st.band.max)}). Temiz su ekleyerek seyreltin.`);
    }
  }

  const ph = byId.ph;
  if (ph && ph.quality === 'ok') {
    const st = sensorStatus(ph);
    if (st.level !== 'ok' && st.band) {
      push(st.level, Number(ph.value) < st.band.min
        ? `pH düşük (${Number(ph.value).toFixed(2)}, hedef ${trimNum(st.band.min)}–${trimNum(st.band.max)}). pH yükseltici ekleyin.`
        : `pH yüksek (${Number(ph.value).toFixed(2)}, hedef ${trimNum(st.band.min)}–${trimNum(st.band.max)}). pH düşürücü ekleyin.`);
    }
  }

  // 5) Sıcaklık — ısıtıcı bağlı değilse elle müdahale gerekir.
  const wt = byId.waterTemp;
  if (wt && wt.quality === 'ok') {
    const st = sensorStatus(wt);
    if (st.level === 'bad' && st.band) {
      const heaterOn = actuatorEnabled('heater');
      push('bad', Number(wt.value) < st.band.min
        ? `Su çok soğuk (${Number(wt.value).toFixed(1)} °C, hedef ${trimNum(st.band.min)}–${trimNum(st.band.max)}). ` +
          (heaterOn ? 'Isıtıcı bağlı — otomatik modda devreye girer.' : 'Isıtıcı bağlı değil; ortamı ısıtın.')
        : `Su çok sıcak (${Number(wt.value).toFixed(1)} °C, hedef ${trimNum(st.band.min)}–${trimNum(st.band.max)}). Kök çürümesi riski var, ortamı serinletin.`);
    }
  }

  // 6) Arızalı sensör. Kartında zaten görünüyor ama listede olması gerekir:
  // arızalı bir sensöre bağlı kural sessizce hiç tetiklenmez ve kullanıcı
  // "program neden çalışmıyor" sorusunun cevabını burada bulmalı.
  s.sensors.forEach((sn) => {
    if (sn.quality !== 'fault') return;
    const meta = SENSOR_META[sn.id] || { label: sn.id };
    push('bad', `${meta.label} sensörü okunamıyor — kablosunu kontrol edin. Bu ölçüme bağlı kurallar çalışmaz.`);
  });

  // 7) Dönem ilerlemesi durduysa nedenini söyle.
  if (store.crop && store.crop.autoStage && !store.crop.autoStageActive) {
    push('warn', 'Gelişim dönemi otomatik ilerlemesi duraklatıldı — cihaz saati geçerli değil. Wi-Fi bağlanınca kendiliğinden düzelir.');
  }

  // 8) Kurallar hazır ama motor kapalı — en sık sorulan "neden çalışmıyor".
  if (s.automation && s.automation.mode === 0 && store.rulesCountHint > 0) {
    push('warn', 'Sulama programı hazır ancak sistem MANUEL modda; kendiliğinden çalışmaz. Ayarlar → Otomatik çalışma bölümünden açabilirsiniz.');
  }

  if (!items.length) push('ok', 'Her şey yolunda görünüyor. Yapılacak bir şey yok.');
  return items;
}

function actuatorEnabled(id) {
  if (!store.config || !store.config.actuators) return false;
  const a = store.config.actuators.find((x) => x.id === id);
  return !!(a && a.enabled);
}

// ── Panel kartları ─────────────────────────────────────────────────────────

function renderAdvice() {
  const host = el('adviceList');
  if (!host) return;
  const items = buildAdvice();
  host.innerHTML = items.map((it) => `
    <div class="advice advice-${it.level}">
      <span class="advice-dot"></span>
      <span>${esc(it.text)}</span>
    </div>`).join('');
}

function renderCropCard() {
  const host = el('cropCard');
  if (!host) return;

  if (!store.crop) {
    host.innerHTML = '<p class="muted">Ürün bilgisi alınamadı.</p>';
    return;
  }

  const tag = el('headCropTag');

  if (!cropSelected()) {
    if (tag) txt(tag, 'KURULUM');
    host.innerHTML = `
      <div class="crop-empty">
        <div class="crop-empty-icon">🌱</div>
        <div>
          <h3>Henüz bir ürün seçmediniz</h3>
          <p class="muted small">
            Ne yetiştirdiğinizi söyleyin; sistem o bitkinin hedef pH/besin
            değerlerini, sulama programını ve ışık süresini sizin için kursun.
          </p>
        </div>
        <button id="cropStartBtn" class="btn btn-primary btn-lg">Ürün Seç</button>
      </div>`;
    const b = el('cropStartBtn');
    if (b) b.onclick = openWizard;
    return;
  }

  const c = store.crop;
  if (tag) txt(tag, (c.name || cropDisplayName()).toUpperCase());

  const stageName = STAGE_TEXT[c.stage] || c.stage;
  const custom = c.crop === 'custom';

  // Sonraki dönem ne zaman? Kullanıcı "ne bekliyorum" sorusunun cevabını
  // görmeli; yalnızca "34. gün" tek başına bir şey anlatmaz.
  let progress = '';
  if (c.targets && c.targets.durationDays > 0 && typeof c.daysSincePlanting === 'number') {
    const total = c.targets.durationDays;
    const done  = Math.min(total, c.daysSincePlanting);
    const pct   = Math.max(2, Math.min(100, Math.round((done / total) * 100)));
    progress = `
      <div class="crop-progress">
        <div class="crop-progress-bar" style="width:${pct}%"></div>
      </div>
      <span class="muted small">Bu dönem yaklaşık ${total} gün sürer</span>`;
  } else if (c.targets) {
    progress = '<span class="muted small">Son dönem — hasada kadar sürer</span>';
  }

  const t = c.targets;
  const chips = t ? `
    <div class="crop-chips">
      <span class="chip">pH ${trimNum(t.ph.min)}–${trimNum(t.ph.max)}</span>
      <span class="chip">EC ${trimNum(t.ec.min)}–${trimNum(t.ec.max)}</span>
      <span class="chip">Su ${trimNum(t.waterTemp.min)}–${trimNum(t.waterTemp.max)} °C</span>
      <span class="chip">Işık ${Math.round(t.lightMinutes / 60)} sa/gün</span>
    </div>` : '';

  host.innerHTML = `
    <div class="crop-head">
      <div>
        <h3>${esc(cropDisplayName())}</h3>
        <p class="crop-sub">
          <b>${esc(stageName)}</b> dönemi
          ${typeof c.daysSincePlanting === 'number' && c.daysSincePlanting > 0
            ? ` · ${c.daysSincePlanting}. gün` : ''}
          · Sulama: ${esc(INTENSITY_TEXT[c.intensity] || c.intensity)}
        </p>
      </div>
      <button id="cropChangeBtn" class="btn btn-secondary btn-sm">Değiştir</button>
    </div>
    ${custom ? `<div class="warn-note small">Kurallar elle değiştirildiği için profil <b>ÖZEL</b>'e düştü. Dönem otomatik ilerlemesi durdu; yeniden profil uygulamak isterseniz "Değiştir" deyin.</div>` : ''}
    ${chips}
    ${progress}`;

  const b = el('cropChangeBtn');
  if (b) b.onclick = openWizard;
}

// ── Ayarlar: ürün özeti ────────────────────────────────────────────────────

function renderCropSettings() {
  const host = el('cropSettingsCard');
  if (!host) return;

  if (!store.crop) {
    host.innerHTML = '<p class="muted">Ürün bilgisi alınamadı.</p>';
    return;
  }

  const c = store.crop;

  // ── ÖZEL PROFİLDE YOĞUNLUK DEĞİŞTİRİLEMEZ ────────────────────────────────
  // Yoğunluk değişimi kural kümesini profilden YENİDEN ÜRETİR. Kullanıcı
  // kuralları elle düzenlemişse bu, o düzenlemeleri uyarısız silmek olurdu.
  // Ayrıca cihaz zaten reddeder: `CUSTOM` bir profile karşılık gelmediği için
  // `PUT /api/crop` "Ürün" alanıyla hata döner. Düğmeyi etkin bırakmak,
  // her tıklamada hata veren bir kontrol demekti (TASK-072).
  const isCustom = c.crop === 'custom';
  const intensityLocked = !cropSelected() || isCustom;
  const intensityHint = isCustom
    ? 'Kurallar elle düzenlendiği için yoğunluk buradan değiştirilemez. ' +
      'Değiştirmek, düzenlemelerinizin üzerine yeni bir program yazmak demektir — ' +
      'bunu yapmak isterseniz "Ürünü / Dönemi Değiştir" deyin; önce ne değişeceğini gösterir.'
    : 'Sistem tipi ve bitki sayısı sulama ihtiyacını değiştirir ama cihaz bunu ' +
      'ölçemez. Bitkiler soluyorsa "Bol", kökler ıslak kalıyorsa "Az" seçin.';

  const rows = cropSelected() ? `
    <div class="kv"><span>Ürün</span><b>${esc(cropDisplayName())}</b></div>
    <div class="kv"><span>Dönem</span><b>${esc(STAGE_TEXT[c.stage] || c.stage)}</b></div>
    <div class="kv"><span>Dikimden beri</span><b>${
      typeof c.daysSincePlanting === 'number' && c.daysSincePlanting > 0
        ? c.daysSincePlanting + ' gün' : 'bilinmiyor'}</b></div>
    <div class="kv"><span>Dönem otomatik ilerlesin</span><b>${
      c.autoStage ? (c.autoStageActive ? 'Evet' : 'Evet (saat geçersiz — duraklatıldı)') : 'Hayır'}</b></div>
  ` : '<p class="muted">Henüz ürün seçilmedi.</p>';

  host.innerHTML = `
    ${rows}
    <div class="intensity-row">
      <label>Sulama yoğunluğu</label>
      <div class="seg" id="intensitySeg">
        ${['sparse', 'normal', 'abundant'].map((k) => `
          <button class="seg-btn${c.intensity === k ? ' active' : ''}" data-int="${k}"
                  ${intensityLocked ? 'disabled' : ''}>${INTENSITY_TEXT[k]}</button>`).join('')}
      </div>
      <span class="field-hint dim">${intensityHint}</span>
    </div>
    <div class="btn-group">
      <button id="cropWizardBtn" class="btn btn-primary">
        ${cropSelected() ? 'Ürünü / Dönemi Değiştir' : 'Ürün Seç'}
      </button>
    </div>
    <p id="cropMsg" class="form-feedback"></p>`;

  el('cropWizardBtn').onclick = openWizard;

  host.querySelectorAll('[data-int]').forEach((b) => {
    b.onclick = () => applyIntensity(b.dataset.int);
  });
}

/// Yoğunluk değişimi kural kümesini YENİDEN ÜRETİR (çevrim süreleri değişir).
/// Bu yüzden sessizce yapılmaz: kaç kuralın değiştiği kullanıcıya söylenir.
async function applyIntensity(key) {
  const m = el('cropMsg');
  if (!m || !store.crop) return;

  m.className = 'form-feedback';
  txt(m, 'Uygulanıyor…');

  try {
    const r = await api('/api/crop', {
      method: 'PUT',
      body: JSON.stringify({ intensity: key }),
    });
    m.className = 'form-feedback msg';
    txt(m, `✔ Sulama yoğunluğu "${INTENSITY_TEXT[key]}" olarak ayarlandı — ${r.ruleCount || 0} kural yeniden yazıldı.`);
    await loadCrop();
    if (typeof loadRules === 'function') loadRules();
  } catch (e) {
    m.className = 'form-feedback err';
    txt(m, e.message);
  }
}

// ── Kurulum sihirbazı ──────────────────────────────────────────────────────

const WIZ_STEPS = ['Ürün', 'Dönem', 'Sulama', 'Onay'];

const wiz = {
  step: 0,
  crop: null,
  stage: 'seedling',
  plantedAt: 0,
  autoStage: true,
  intensity: 'normal',
  plan: null,
};

function todayEpoch() {
  const d = new Date();
  d.setHours(0, 0, 0, 0);
  return Math.floor(d.getTime() / 1000);
}

function epochToInputDate(sec) {
  if (!sec) return '';
  const d = new Date(sec * 1000);
  return `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())}`;
}

async function openWizard() {
  wiz.step = 0;
  wiz.plan = null;

  // Mevcut seçimi başlangıç değeri yap: "değiştir" diyen kullanıcı her şeyi
  // baştan doldurmak zorunda kalmasın.
  if (store.crop && cropSelected() && store.crop.crop !== 'custom') {
    wiz.crop = store.crop.crop;
    wiz.stage = store.crop.stage;
    wiz.intensity = store.crop.intensity;
    wiz.autoStage = !!store.crop.autoStage;
    wiz.plantedAt = store.crop.plantedAt || todayEpoch();
  } else {
    wiz.crop = null;
    wiz.stage = 'seedling';
    wiz.intensity = 'normal';
    wiz.autoStage = true;
    wiz.plantedAt = todayEpoch();
  }

  show(el('wizard'), true);
  txt(el('wizMsg'), '');

  try {
    await loadCatalog();
  } catch (e) {
    // Katalog yoksa sihirbaz İLERLEYEMEZ. Eskiden yalnızca gövdeye hata
    // yazılıp çıkılıyordu; "Devam" düğmesi hâlâ etkin kalıyor ve tıklanınca
    // `prof.stages` üzerinde TypeError atıp kip donuyordu (TASK-072).
    el('wizBody').innerHTML =
      `<p class="err">Ürün listesi alınamadı: ${esc(e.message)}</p>` +
      '<p class="muted small">Bağlantıyı kontrol edip yeniden deneyin.</p>';
    el('wizNext').disabled = true;
    el('wizBack').disabled = true;
    el('wizSteps').innerHTML = '';
    return;
  }
  renderWizard();
}

function closeWizard() {
  show(el('wizard'), false);
}

function wizCropProfile() {
  if (!store.catalog || !wiz.crop) return null;
  return store.catalog.crops.find((c) => c.key === wiz.crop) || null;
}

function renderWizard() {
  // KATALOG OLMADAN HİÇBİR ADIM ÇİZİLEMEZ. Adım 0 doğrudan
  // `store.catalog.crops` üzerinde döner; koruma adım 1'e konsaydı çökme
  // zaten adım 0'da olurdu (TASK-072).
  if (!store.catalog || !Array.isArray(store.catalog.crops) || !store.catalog.crops.length) {
    el('wizSteps').innerHTML = '';
    el('wizBody').innerHTML =
      '<p class="err">Ürün listesi alınamadı.</p>' +
      '<p class="muted small">Bağlantıyı kontrol edip pencereyi kapatın ve yeniden deneyin.</p>';
    el('wizNext').disabled = true;
    el('wizBack').disabled = true;
    return;
  }

  const stepsHost = el('wizSteps');
  stepsHost.innerHTML = WIZ_STEPS.map((s, i) => `
    <div class="wiz-step${i === wiz.step ? ' active' : ''}${i < wiz.step ? ' done' : ''}">
      <span class="wiz-num">${i + 1}</span><span>${s}</span>
    </div>`).join('');

  const body = el('wizBody');
  const next = el('wizNext');
  const back = el('wizBack');

  back.disabled = wiz.step === 0;
  txt(el('wizMsg'), '');

  if (wiz.step === 0) {
    txt(el('wizTitle'), 'Ne yetiştireceksiniz?');
    next.textContent = 'Devam';
    next.disabled = !wiz.crop;

    body.innerHTML = `
      <div class="crop-grid">
        ${store.catalog.crops.map((c) => `
          <button class="crop-pick${wiz.crop === c.key ? ' selected' : ''}" data-crop="${esc(c.key)}">
            <span class="crop-pick-name">${esc(c.name)}</span>
            <span class="badge diff-${c.difficulty}">${DIFFICULTY_TEXT[c.difficulty] || ''}</span>
            <span class="muted small">
              pH ${trimNum(c.stages[0].ph.min)}–${trimNum(c.stages[0].ph.max)} ·
              ${c.stageCount} dönem
            </span>
          </button>`).join('')}
      </div>
      <p class="field-hint dim">
        Kolay ürünler geniş bir pH aralığını tolere eder ve hızlı hasat verir.
        Zor ürünler daha çok ışık, daha yoğun besin ve daha kararlı koşullar ister.
      </p>`;

    body.querySelectorAll('[data-crop]').forEach((b) => {
      b.onclick = () => {
        wiz.crop = b.dataset.crop;
        // Ürün değişince dönem sıfırlanır: marulda "meyve" dönemi yoktur ve
        // önceki üründen taşınan bir dönem cihaz tarafından reddedilirdi.
        wiz.stage = 'seedling';
        renderWizard();
      };
    });
    return;
  }

  const prof = wizCropProfile();

  // Adım 0 dışındaki her adım profile bağımlıdır. Profil bulunamıyorsa
  // (katalog bayat, ürün kaldırılmış) ilerlemek yerine BAŞA dönülür —
  // `prof.stages` üzerinde çökmek yerine.
  if (!prof) {
    wiz.step = 0;
    wiz.crop = null;
    renderWizard();                      // adım 0 profile bağımlı değil, döngü yok
    // Mesaj SONRA yazılır: `renderWizard()` her çağrıda geri bildirimi temizler.
    el('wizMsg').className = 'form-feedback err';
    txt(el('wizMsg'), 'Ürün bilgisi okunamadı — lütfen yeniden seçin.');
    return;
  }

  if (wiz.step === 1) {
    txt(el('wizTitle'), 'Hangi aşamadasınız?');
    next.textContent = 'Devam';
    next.disabled = false;

    body.innerHTML = `
      <div class="form-group">
        <label>Bitkinin şu anki dönemi</label>
        <div class="seg seg-wrap" id="wizStageSeg">
          ${prof.stages.map((s) => `
            <button class="seg-btn${wiz.stage === s.stage ? ' active' : ''}" data-stage="${esc(s.stage)}">
              ${STAGE_TEXT[s.stage] || s.stage}
            </button>`).join('')}
        </div>
        <span class="field-hint dim">
          Emin değilseniz <b>Fide</b> seçin. Dönem, dikim tarihinden itibaren
          gün sayılarak kendiliğinden ilerleyebilir.
        </span>
      </div>

      <div class="form-group">
        <label for="wizDate">Dikim tarihi</label>
        <input id="wizDate" type="date" value="${epochToInputDate(wiz.plantedAt)}">
        <span class="field-hint dim">Bilmiyorsanız bugünü bırakın — yalnızca gün sayımı için kullanılır.</span>
      </div>

      <div class="form-group">
        <label class="check-line">
          <input id="wizAuto" type="checkbox" ${wiz.autoStage ? 'checked' : ''}>
          <span>Dönem gün sayısına göre kendiliğinden ilerlesin</span>
        </label>
        <span class="field-hint dim">
          Cihazın saati geçerli değilken ilerleme durur (donanımsal saat yok);
          Wi-Fi bağlanınca kaldığı yerden devam eder.
        </span>
      </div>`;

    body.querySelectorAll('[data-stage]').forEach((b) => {
      b.onclick = () => { wiz.stage = b.dataset.stage; renderWizard(); };
    });
    el('wizDate').onchange = (ev) => {
      const d = new Date(ev.target.value + 'T00:00:00');
      wiz.plantedAt = isNaN(d.getTime()) ? todayEpoch() : Math.floor(d.getTime() / 1000);
    };
    el('wizAuto').onchange = (ev) => { wiz.autoStage = ev.target.checked; };
    return;
  }

  if (wiz.step === 2) {
    txt(el('wizTitle'), 'Ne kadar sulansın?');
    next.textContent = 'Önizle';
    next.disabled = false;

    body.innerHTML = `
      <div class="seg seg-wrap" id="wizIntSeg">
        ${['sparse', 'normal', 'abundant'].map((k) => `
          <button class="seg-btn${wiz.intensity === k ? ' active' : ''}" data-int="${k}">
            ${INTENSITY_TEXT[k]}
          </button>`).join('')}
      </div>
      <p class="field-hint dim">
        Cihaz sistem tipinizi (damlama, NFT, derin su) ve bitki sayınızı bilemez.
        <b>Normal</b> ile başlayın; bitkiler soluyorsa <b>Bol</b>'a,
        kökler sürekli ıslak kalıyorsa <b>Az</b>'a alın.
      </p>`;

    body.querySelectorAll('[data-int]').forEach((b) => {
      b.onclick = () => { wiz.intensity = b.dataset.int; renderWizard(); };
    });
    return;
  }

  // --- Adım 3: ÖNİZLEME ---
  txt(el('wizTitle'), 'Şunlar değişecek');
  next.textContent = 'Uygula';
  next.disabled = !wiz.plan;

  if (!wiz.plan) {
    body.innerHTML = '<p class="muted">Plan hesaplanıyor…</p>';
    return;
  }

  const p = wiz.plan;
  const stageName = STAGE_TEXT[p.stage] || p.stage;

  body.innerHTML = `
    <div class="preview-summary">
      <div class="kv"><span>Ürün</span><b>${esc(prof ? prof.name : wiz.crop)}</b></div>
      <div class="kv"><span>Dönem</span><b>${esc(stageName)}</b></div>
      <div class="kv"><span>Sulama</span><b>${esc(INTENSITY_TEXT[wiz.intensity])}</b></div>
      <div class="kv"><span>Yazılacak kural</span><b>${p.ruleCount}</b></div>
      <div class="kv"><span>Üzerine yazılacak mevcut kural</span><b>${p.replacedCount}</b></div>
    </div>

    ${p.replacedCount > 0 ? `
      <div class="warn-note">
        Şu an tanımlı <b>${p.replacedCount}</b> kural var ve bunların
        <b>tamamı silinip</b> yerine aşağıdaki program yazılacak.
        Elle yaptığınız kural düzenlemeleri kaybolur.
      </div>` : ''}

    <h4 class="preview-title">Kurulacak program</h4>
    <ul class="preview-rules">
      ${p.rules.length
        ? p.rules.map((r) => `<li>${describeGeneratedRule(r)}</li>`).join('')
        : '<li class="muted">Hiç kural üretilmedi — Ayarlar → "Bağlı cihazlar" bölümünde en az bir cihaz işaretli olmalı.</li>'}
    </ul>

    ${p.automationMode === 'manual' ? `
      <div class="warn-note">
        <b>Program kurulacak ama hemen çalışmayacak.</b> Sistem şu an
        <b>MANUEL</b> modda; kurallar yalnızca <b>OTOMATİK</b> modda
        değerlendirilir. Donanımınızı test ettikten sonra Ayarlar →
        "Otomatik çalışma" bölümünden açabilirsiniz.
      </div>` : ''}`;
}

/// Üretilmiş bir kuralı tek cümlede anlatır. Kullanıcı "threshold /
/// onThreshold / cycleOnS" gibi alanları okumak zorunda kalmamalı.
function describeGeneratedRule(r) {
  const act = (ACT_META[r.target] || { name: r.target }).name;

  if (r.kind === 'cycle') {
    return `Her ${fmtSec(r.cyclePeriodS)} içinde <b>${fmtSec(r.cycleOnS)}</b> boyunca ${esc(act)} çalışır.`;
  }
  if (r.kind === 'window') {
    return `Her gün ${minToTime(r.startMin)} – ${minToTime(r.endMin)} arasında ${esc(act)} açık kalır.`;
  }
  if (r.kind === 'threshold') {
    const meta = SENSOR_META[r.sensor] || { label: r.sensor, unit: '' };
    const u = meta.unit ? ' ' + meta.unit : '';
    const down = Number(r.onThreshold) < Number(r.offThreshold);
    return down
      ? `${esc(meta.label)} ${trimNum(r.onThreshold)}${u} altına düşünce ${esc(act)} açılır, ${trimNum(r.offThreshold)}${u} olunca kapanır.`
      : `${esc(meta.label)} ${trimNum(r.onThreshold)}${u} üstüne çıkınca ${esc(act)} açılır, ${trimNum(r.offThreshold)}${u} olunca kapanır.`;
  }
  return esc(act);
}

async function wizardNext() {
  const msg = el('wizMsg');
  msg.className = 'form-feedback';

  if (wiz.step === 0) { if (!wiz.crop) return; wiz.step = 1; renderWizard(); return; }
  if (wiz.step === 1) { wiz.step = 2; renderWizard(); return; }

  if (wiz.step === 2) {
    // ÖNİZLEME: cihaz hesaplar, biz göstereceğiz. Hiçbir şey yazılmaz.
    wiz.step = 3;
    wiz.plan = null;
    renderWizard();
    try {
      wiz.plan = await api('/api/crop/preview', {
        method: 'POST',
        body: JSON.stringify({
          crop: wiz.crop, stage: wiz.stage,
          intensity: wiz.intensity, plantedAt: wiz.plantedAt,
          autoStage: wiz.autoStage,
        }),
      });
      renderWizard();
    } catch (e) {
      msg.className = 'form-feedback err';
      txt(msg, e.message);
      el('wizBody').innerHTML = `<p class="err">${esc(e.message)}</p>`;
    }
    return;
  }

  // --- Adım 3: UYGULA ---
  const next = el('wizNext');
  next.disabled = true;
  txt(msg, 'Uygulanıyor…');

  try {
    const r = await api('/api/crop', {
      method: 'PUT',
      body: JSON.stringify({
        crop: wiz.crop, stage: wiz.stage,
        intensity: wiz.intensity, plantedAt: wiz.plantedAt,
        autoStage: wiz.autoStage,
      }),
    });

    closeWizard();
    await loadCrop();
    if (typeof loadRules === 'function') loadRules();

    const cm = el('cropMsg');
    if (cm) {
      cm.className = 'form-feedback msg';
      txt(cm, `✔ ${cropDisplayName()} profili uygulandı — ${r.ruleCount || 0} kural yazıldı.`);
    }
  } catch (e) {
    next.disabled = false;
    msg.className = 'form-feedback err';
    txt(msg, e.message);
  }
}

function wizardBack() {
  if (wiz.step === 0) return;
  wiz.step -= 1;
  wiz.plan = null;
  renderWizard();
}
