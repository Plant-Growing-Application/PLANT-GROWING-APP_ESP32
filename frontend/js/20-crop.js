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

/// Aynı anda birden fazla `loadCrop()` çağrısı TEK isteğe indirgenir.
///
/// Girişte iki yol birden tetikliyordu: `afterLogin()` ve WS'in bağlanır
/// bağlanmaz gönderdiği ilk `state` paketi. İkisi de `await`'e girdiği için
/// hiçbiri diğerini göremiyor ve `/api/crop` + `/api/crops` İKİŞER kez
/// çekiliyordu — katalog 5 KB, yani her açılışta boşuna 10 KB. AP modunda
/// bu, gözle görülür bir gecikmedir.
let cropInFlight = null;

function loadCrop() {
  if (!cropInFlight) {
    cropInFlight = loadCropOnce().finally(() => { cropInFlight = null; });
  }
  return cropInFlight;
}

async function loadCropOnce() {
  try {
    store.crop = await api('/api/crop');
  } catch (e) {
    store.crop = null;
  }

  // ── KATALOG ÜRÜN SEÇİLİYSE HER ZAMAN GEREKLİ ─────────────────────────────
  //
  // Eskiden yalnızca `CUSTOM` profilin ADINI çözmek için çekiliyordu. Gelişim
  // yolculuğu ise TÜM dönemleri ve sürelerini ister: `/api/crop` yalnızca
  // İÇİNDE BULUNULAN dönemin hedeflerini döner, diğer dönemleri değil.
  //
  // Katalog sabittir ve bir kez alınır (~5 KB); her ürün değişiminde değil.
  if (store.crop && cropSelected() && !store.catalog) {
    try { await loadCatalog(); } catch (e) { /* yolculuk çizilmez, kart çizilir */ }
  }

  renderCropCard();
  renderCropTargets();
  renderCropSettings();
  render();
}

/// Katalog SABİTTİR; bir kez alınır ve saklanır (~5 KB).
///
/// Bekçi `store.catalog`'a değil UÇUŞTAKİ İSTEĞE bakar: değer ancak
/// `await` döndükten SONRA yazılır, o yüzden eşzamanlı iki çağrının ikisi de
/// `null` görüp ikisi de indirirdi. Sabit bir belgeyi iki kez çekmek,
/// ESP32'nin tek çekirdekli web sunucusunda bedava değildir.
let catalogInFlight = null;

function loadCatalog() {
  if (store.catalog) { return Promise.resolve(store.catalog); }
  if (!catalogInFlight) {
    catalogInFlight = api('/api/crops')
      .then((c) => { store.catalog = c; return c; })
      .finally(() => { catalogInFlight = null; });
  }
  return catalogInFlight;
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

/// Aktif ürünün KATALOG profili. `CUSTOM` iken türediği profile düşer —
/// saksıdaki bitki hâlâ o bitkidir, yalnızca kurallar elle değişmiştir.
function activeProfile() {
  if (!store.catalog || !store.catalog.crops || !store.crop) return null;
  const key = store.crop.crop === 'custom'
    ? (store.crop.derivedFrom || '')
    : store.crop.crop;
  return store.catalog.crops.find((c) => c.key === key) || null;
}

// ── Gelişim yolculuğu ──────────────────────────────────────────────────────
//
// ══ UYDURMA YOK ════════════════════════════════════════════════════════════
// Yüzde YALNIZCA gerçek veriden hesaplanır ve veri tutarsızsa GÖSTERİLMEZ.
// Hesap, cihazın kendi algoritmasının aynısıdır (`core::stageForDay`,
// CropProfile.cpp): dönem süreleri kümülatif toplanır.
//
//     dönem başlangıcı = Σ süre[0 .. i-1]
//     dönemdeki gün    = dikimden beri geçen gün − dönem başlangıcı
//
// HANGİ DURUMDA YÜZDE YOK:
//   · süre 0  → son dönem, süresiz ("hasada kadar")
//   · dönemdeki gün negatif veya süreyi aşıyor → kullanıcı dönemi dikim
//     tarihiyle ÇELİŞECEK şekilde elle seçmiş. Böyle bir durumda %140 veya
//     %-30 göstermek yerine hiçbir oran göstermiyoruz: yanlış bir sayı,
//     sayı olmamasından kötüdür.
//   · katalog yok → yolculuk hiç çizilmez
//
// GÖSTERİLEN DÖNEM HER ZAMAN CİHAZINKİDİR (`store.crop.stage`). Günden kendi
// dönemimizi hesaplayıp onu göstermek, cihazın uyguladığı hedeflerden farklı
// bir dönem anlatmak olurdu (P5 — cihaz doğruluk kaynağıdır).

function journeyInfo() {
  const prof = activeProfile();
  const c = store.crop;
  if (!prof || !c || !Array.isArray(prof.stages) || !prof.stages.length) return null;

  const stages = prof.stages;
  const si = Math.max(0, stages.findIndex((s) => s.stage === c.stage));

  let startDay = 0;
  for (let i = 0; i < si; i++) startDay += (+stages[i].durationDays || 0);

  const dur  = +stages[si].durationDays || 0;
  const day  = typeof c.daysSincePlanting === 'number' ? c.daysSincePlanting : null;
  const into = (day === null) ? null : day - startDay;

  const consistent = into !== null && into >= 0 && dur > 0 && into <= dur;

  return {
    stages: stages.map((s, i) => ({
      key: s.stage,
      label: STAGE_TEXT[s.stage] || s.stage,
      state: i < si ? 'done' : (i === si ? 'now' : ''),
    })),
    index: si,
    day: day,
    daysInStage: into,
    stageDays: dur,
    /// null = oran gösterilemez
    pct: consistent ? Math.max(3, Math.min(100, Math.round((into / dur) * 100))) : null,
    finalStage: dur === 0 || si >= stages.length - 1,
    nextLabel: si + 1 < stages.length
      ? (STAGE_TEXT[stages[si + 1].stage] || stages[si + 1].stage)
      : null,
  };
}

/// Yolculuk şeridini çizer. Katalog yoksa boş dizge döner — çağıran yeri
/// boş bırakır, "veri yok" kutusu koymaz.
function renderJourney() {
  const j = journeyInfo();
  if (!j) return '';

  const track = j.stages.map((s) => `
    <div class="journey-stage ${s.state}">
      <span class="journey-node"></span>
      <span class="journey-label">${esc(s.label)}</span>
    </div>`).join('');

  let meter = '';
  if (j.pct !== null) {
    meter = `
      <div class="journey-meter" role="progressbar" aria-valuenow="${j.pct}"
           aria-valuemin="0" aria-valuemax="100"
           aria-label="Bu dönemde ilerleme">
        <div class="journey-fill" style="width:${j.pct}%"></div>
      </div>
      <div class="journey-foot">
        <span>Bu dönemde <b>${j.daysInStage}</b> / ${j.stageDays} gün</span>
        ${j.nextLabel ? `<span>Sonraki: <b>${esc(j.nextLabel)}</b></span>` : ''}
      </div>`;
  } else if (j.finalStage) {
    meter = `<div class="journey-foot"><span>Son dönem — <b>hasada kadar sürer</b></span>
             ${j.day !== null && j.day > 0 ? `<span>${j.day}. gün</span>` : ''}</div>`;
  } else {
    // Tutarsız veri: dönemi ve günü söyle, oran söyleme.
    meter = `<div class="journey-foot">
        ${j.day !== null && j.day > 0 ? `<span>Dikimden beri <b>${j.day}</b> gün</span>` : ''}
        ${j.nextLabel ? `<span>Sonraki: <b>${esc(j.nextLabel)}</b></span>` : ''}
      </div>`;
  }

  const chips = [];
  if (store.crop.crop === 'custom') {
    chips.push('<span class="chip">✎ Kurallar elle düzenlendi</span>');
  }
  if (store.crop.autoStage && !store.crop.autoStageActive) {
    chips.push('<span class="chip">⏸ Dönem ilerlemesi duraklatıldı</span>');
  }
  if (!store.crop.autoStage) {
    chips.push('<span class="chip">⤳ Dönemi siz seçiyorsunuz</span>');
  }

  // Ürünün kimliği KAHRAMAN KARTIN üst satırında zaten yazıyor ("Çilek ·
  // Çiçeklenme dönemi"); burada tekrar etmiyoruz. Eksik olan şey EYLEMDİ:
  // ana sayfadan ürünü değiştirmenin ya da bırakmanın hiçbir yolu yoktu,
  // kullanıcı Ayarlar → Bahçe ekranını bulmak zorundaydı.
  return `<div class="journey">
      <div class="journey-track">${track}</div>
      ${meter}
      ${chips.length ? `<div class="journey-chips">${chips.join('')}</div>` : ''}
      <div class="journey-actions">
        <button id="cropChangeBtn" class="btn btn-sm btn-outline">Ürünü değiştir</button>
        <button id="cropRemoveBtn" class="btn btn-sm btn-ghost-danger">Kaldır</button>
      </div>
      <p id="cropHomeMsg" class="form-feedback"></p>
    </div>`;
}

/// Ürünü bırakma — hasat bitti, saksı boş.
///
/// ── NEDEN ÖNCE ÖNİZLEME ─────────────────────────────────────────────────────
/// Ürünü kaldırmak sulama programını da siler. Kaç kuralın silineceğini
/// SÖYLEMEDEN onay istemek, kullanıcıya ne kaybedeceğini bilmediği bir
/// karar verdirmek olurdu. Sihirbazın "şunlar değişecek" ekranıyla aynı
/// ilke; cihaz sayıyı zaten hesaplıyor, sormak yeterli.
async function removeCrop() {
  const m = el0('cropHomeMsg');
  const btn = el0('cropRemoveBtn');
  const name = cropDisplayName();

  const say = (cls, s) => { if (m) { m.className = 'form-feedback' + (cls ? ' ' + cls : ''); txt(m, s); } };

  say('', 'Ne silineceği hesaplanıyor…');

  let willDelete = 0;
  try {
    const plan = await api('/api/crop/preview', {
      method: 'POST', body: JSON.stringify({ crop: 'none' }),
    });
    willDelete = plan.replacedCount || 0;
  } catch (e) {
    say('err', 'Silinecekler öğrenilemedi: ' + e.message);
    return;
  }

  const ok = confirm(
    `"${name}" kaldırılacak.\n\n` +
    (willDelete > 0
      ? `· ${willDelete} sulama/ışık kuralı silinecek\n`
      : '· Silinecek bir kural yok\n') +
    '· Hedef pH/besin aralıkları kaldırılacak; ölçümler yorumsuz gösterilir\n' +
    '· Gün sayacı sıfırlanır\n\n' +
    'Ayarlarınız, Wi-Fi ve parolanız etkilenmez. Sonra yeni bir ürün seçebilirsiniz.');

  if (!ok) { say('', ''); return; }

  if (btn) { btn.disabled = true; }
  say('', 'Kaldırılıyor…');

  try {
    // `plantedAt: 0` gün sayacını sıfırlar, `autoStage: false` dönem
    // ilerlemesini durdurur — ürün yokken ikisi de anlamsız.
    await api('/api/crop', {
      method: 'PUT',
      body: JSON.stringify({ crop: 'none', plantedAt: 0, autoStage: false }),
    });
  } catch (e) {
    if (btn) { btn.disabled = false; }
    say('err', 'Kaldırılamadı: ' + e.message);
    return;
  }

  await loadCrop();
  if (typeof loadRules === 'function') { loadRules(); }
  // `loadCrop()` kartı yeniden çizdi; mesaj kutusu artık yok. Geri bildirimi
  // yerinde duran kahraman karta bırakıyoruz — o zaten "Başlayalım" diyecek.
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
/// İki metin döner ve ikisi de gerekir:
///   `word` — TEK SÖZCÜK, panelin kompakt kutusu için ("Biraz yüksek")
///   `text` — TAM CÜMLE + hedef aralık, Bitkim ekranındaki kart için
/// Kompakt kutuya uzun cümleyi sığdırmaya çalışmak, ya kesik metin ya da
/// okunmayacak kadar küçük punto demekti.
///
/// @return { id, label, level, word, value, unit, text, band }
function sensorStatus(sn) {
  const meta = SENSOR_META[sn.id] || { label: sn.id, unit: '', digits: 2 };
  const out = { id: sn.id, label: meta.label, unit: meta.unit, band: null,
                level: 'unknown', word: '—', value: '—', text: '' };

  // Değer YOKKEN birim de gösterilmez: "Yok lx" saçmadır ve okuyan kişiye
  // sensörün bir değer ürettiği izlenimi verir.
  if (sn.quality === 'notPresent') {
    out.level = 'off';
    out.word  = 'Takılı değil';
    out.value = 'Yok';
    out.unit  = '';
    out.text  = 'Bu sensör takılı değil';
    return out;
  }
  if (sn.quality === 'fault') {
    out.level = 'bad';
    out.word  = 'Arızalı';
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
    out.word  = idx === 2 ? 'Yeterli' : (idx === 1 ? 'Azalıyor' : 'Kritik');
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
    out.word  = 'Değişmiyor';
    out.text  = 'Değer bir süredir değişmiyor';
    return out;
  }
  if (sn.quality === 'outOfRange') {
    out.level = 'bad';
    out.word  = 'Aralık dışı';
    out.text  = 'Değer beklenen aralığın dışında';
    return out;
  }

  if (!v) {
    // Hedef band yok: ürün seçilmemiş veya bu sensörün ürünle ilgisi yok
    // (akış, ışık). Sayıyı gösterip yorum yapmıyoruz.
    out.level = 'ok';
    out.word  = 'Ölçülüyor';
    out.text  = cropSelected() ? '' : 'Ürün seçilince hedef aralık gösterilir';
    return out;
  }

  out.level = v.level;
  out.word = v.level === 'ok' ? 'İdeal'
           : (v.level === 'warn' ? (v.low ? 'Biraz düşük' : 'Biraz yüksek')
                                 : (v.low ? 'Çok düşük'   : 'Çok yüksek'));
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

  // `key` yalnızca kahraman kartın bir maddeyi TANIYABİLMESİ için var:
  // "ürün seçilmedi" maddesinin altında zaten bir "Ürün Seç" düğmesi
  // duruyor, o yüzden orada talimat yerine davet gösteriyoruz.
  const push = (level, text, key) => items.push({ level, text, key: key || '' });

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
    push('warn', 'Henüz ürün seçilmedi. Ayarlar → Bahçe bölümünden ne yetiştirdiğinizi ' +
                 'seçin; cihaz hedefleri ve sulama programını sizin için kursun.', 'noCrop');
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

  // 6b) Cihaz bir aktüatörü ÇALIŞTIRAMIYOR.
  //
  // `block` her telemetri paketinde geliyordu ama yalnızca Kontrol
  // ekranındaki kartın üstünde görünüyordu. "Pompa neden çalışmadı"
  // sorusunun cevabı, kullanıcının baktığı ilk yerde — yapılacaklar
  // listesinde — olmalı; başka bir sekmeye gidip aramak zorunda kalmamalı.
  //
  // `a.block` tek bir telemetri paketinde parlayıp söndüğü için doğrudan
  // ona bakmak, listede bir saniye görünüp kaybolan bir madde üretirdi —
  // okunamayacak kadar kısa ve dikkat dağıtıcı. Son 20 saniyede bildirilen
  // engeli kullanıyoruz: madde yeterince duruyor ki okunabilsin, ama
  // düzeldikten sonra da kalmıyor.
  const BLOCK_WINDOW_MS = 20000;
  s.actuators.forEach((a) => {
    const meta = ACT_META[a.id];
    if (!meta) { return; }
    const lb = store.lastBlock[a.id];
    const code = a.block || (lb && Date.now() - lb.at < BLOCK_WINDOW_MS ? lb.code : 0);
    if (!code) { return; }
    push('bad', `${meta.name} çalıştırılamıyor — ${errText(code)}`, 'block:' + a.id);
  });

  // 7) Dönem ilerlemesi durduysa nedenini söyle.
  if (store.crop && store.crop.autoStage && !store.crop.autoStageActive) {
    push('warn', 'Gelişim dönemi otomatik ilerlemesi duraklatıldı — cihaz saati geçerli değil. Wi-Fi bağlanınca kendiliğinden düzelir.');
  }

  // 8) Kurallar hazır ama motor kapalı — en sık sorulan "neden çalışmıyor".
  if (s.automation && s.automation.mode === 0 && store.rulesCountHint > 0) {
    push('warn', 'Sulama programı hazır ancak sistem MANUEL modda; kendiliğinden çalışmaz. Ayarlar → Otomatik çalışma bölümünden açabilirsiniz.');
  }

  // 9) Cihazın kendi bildirdiği etkin hatalar. Sayı telemetride hazır
  // duruyor; teşhis ekranına girmeden en azından VARLIĞINDAN haberdar
  // olunmalı — kullanıcı listeyi temiz görüp sorun yok sanmasın.
  if (s.system && s.system.faults > 0) {
    push('warn', `Cihaz ${s.system.faults} etkin hata bildiriyor. ` +
                 'Ayarlar → Sistem ekranındaki teşhis bölümünde listeleniyor.', 'faults');
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

/// "Yapmanız gerekenler" bölümü.
///
/// TEK MADDEYİ ÇİZMEZ: onu kahraman kart tam metniyle zaten söylüyor
/// (bkz. `renderHeadline`). Liste yalnızca iki ve üzeri madde varsa görünür;
/// o zaman kahraman kart sayıya döner ve ayrıntı buraya düşer.
/// "Bugün yapılacaklar" — ana sayfanın görev listesi.
///
/// ── NEDEN HER ZAMAN GÖRÜNÜR ─────────────────────────────────────────────────
/// Önceki sürümde bölüm yalnızca 2+ madde varken çiziliyordu. Sonuç: liste
/// kullanıcının GÜVENEBİLECEĞİ bir yer değildi — bazen vardı, bazen yoktu ve
/// tek maddelik bir sorun hiç listelenmiyordu. "Bugün ne yapmalıyım?"
/// sorusunun cevabı HER ZAMAN aynı yerde olmalı; boşsa da orada olmalı ki
/// boş olduğu görülebilsin.
///
/// Kahraman kart artık madde metnini TEKRARLAMAZ, yalnızca sayar — tekrar
/// bu yüzden oluşmuyor.
function renderAdvice() {
  const host = el0('adviceList');
  const count = el0('adviceCount');
  if (!host) return;

  const items = store.advice || buildAdvice().filter((it) => it.level !== 'ok');

  if (count) {
    txt(count, items.length ? String(items.length) : '✓');
    count.className = 'section-badge' + (items.length
      ? (items.some((i) => i.level === 'bad') ? ' badge-bad' : ' badge-warn')
      : ' badge-ok');
  }

  if (!items.length) {
    host.innerHTML = `
      <div class="advice advice-done">
        <span class="advice-mark" aria-hidden="true">✓</span>
        <span>Bugün yapmanız gereken bir şey yok. Cihaz bahçenizi kendisi
          izliyor; bir şey değişirse burada görürsünüz.</span>
      </div>`;
    return;
  }

  // Önce ACİL olanlar. İki madde varsa ve biri "hemen su ekleyin" ise,
  // kullanıcının onu listenin altında araması saçma olurdu.
  const order = { bad: 0, warn: 1, ok: 2 };
  const sorted = items.slice().sort((a, b) => (order[a.level] || 9) - (order[b.level] || 9));

  host.innerHTML = sorted.map((it) => `
    <div class="advice advice-${it.level}">
      <span class="advice-mark" aria-hidden="true">${it.level === 'bad' ? '!' : '•'}</span>
      <span>${esc(it.text)}</span>
    </div>`).join('');
}

/// Kahraman kartın alt yarısı: gelişim yolculuğu — ürün yoksa kurulum çağrısı.
///
/// Ürünün ADI ve durumu artık kartın ÜST yarısında (`renderHeadline`) yazıyor;
/// burada tekrarlanmıyor. Eski sürümde aynı bilgi üç ayrı yerde vardı:
/// başlıktaki rozet, kahraman metni ve ürün kartının başlığı.
function renderCropCard() {
  const host = el('cropCard');
  if (!host) return;

  if (!store.crop) {
    host.innerHTML = '';
    return;
  }

  const tag = el0('headCropTag');

  if (!cropSelected()) {
    if (tag) txt(tag, 'KURULUM');
    // Açıklama kahraman kartın üst yarısında zaten var; burada YALNIZCA
    // eylem duruyor. Aynı gerekçeyi iki kez yazmak, ekranı doldurmaktan
    // başka bir işe yaramaz.
    host.innerHTML = `
      <div class="journey">
        <button id="cropStartBtn" class="btn btn-primary btn-block btn-lg">🌱 Ürün Seç</button>
      </div>`;
    const b = el0('cropStartBtn');
    if (b) b.onclick = openWizard;
    return;
  }

  if (tag) txt(tag, (store.crop.name || cropDisplayName()).toUpperCase());
  host.innerHTML = renderJourney();

  const chg = el0('cropChangeBtn');
  if (chg) { chg.onclick = openWizard; }
  const rm = el0('cropRemoveBtn');
  if (rm) { rm.onclick = removeCrop; }
}

/// Hedef bant çipleri — Bitkim ekranının başında. Panelde DEĞİL: kullanıcı
/// panelde "iyi mi kötü mü" sorusunun cevabını ister, aralığın kendisini
/// değil. Aralık, sayıya bakmak isteyenin ekranında durur.
function renderCropTargets() {
  const host = el0('cropTargets');
  if (!host) return;

  const t = cropTargets();
  if (!t) { host.innerHTML = ''; show(host, false); return; }

  show(host, true);
  host.innerHTML = `
    <span class="chip">pH ${trimNum(t.ph.min)}–${trimNum(t.ph.max)}</span>
    <span class="chip">EC ${trimNum(t.ec.min)}–${trimNum(t.ec.max)} mS/cm</span>
    <span class="chip">Su ${trimNum(t.waterTemp.min)}–${trimNum(t.waterTemp.max)} °C</span>
    <span class="chip">Hava ${trimNum(t.airTemp.min)}–${trimNum(t.airTemp.max)} °C</span>
    <span class="chip">Nem %${trimNum(t.humidity.min)}–${trimNum(t.humidity.max)}</span>
    <span class="chip">Işık ${Math.round(t.lightMinutes / 60)} sa/gün</span>`;
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
