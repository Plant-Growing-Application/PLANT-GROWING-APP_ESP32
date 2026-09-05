/* Oturum ve başlatma (TASK-070) */

// ── Parola ve oturum ───────────────────────────────────────────────────────

/// Oturumu yerelde kapatır. Cihaz tarafında oturum geçersizken arayüzün
/// "bağlı" görünmesi, kullanıcının çalışmayan bir panele bakması demektir.
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
      method: 'POST', body: JSON.stringify({ current: cur, next: next }),
    });
  } catch (e) {
    m.className = 'form-feedback err';
    if (e.code === 0x0901)      txt(m, 'Mevcut parola hatalı.');
    else if (e.code === 0x0204) txt(m, 'Yeni parola cihaz tarafından reddedildi (en az 8 karakter).');
    else                        txt(m, e.message);
    return;
  }

  // Cihaz parola değişiminde TÜM oturumları düşürür; elimizdeki token ölü.
  el('pwCurrent').value = ''; el('pwNext').value = ''; el('pwNext2').value = '';
  txt(m, '');
  endSession('Parola değiştirildi — yeni parolanızla giriş yapın.');
}

async function doLogout() {
  const t = store.token;
  try {
    if (t) await api('/api/auth/logout?token=' + encodeURIComponent(t), { method: 'POST' });
  } catch (e) {
    // Cihaz yanıt vermese de yerel oturumu düşürürüz.
  }
  endSession('Oturum kapatıldı.');
}

// ── Giriş ──────────────────────────────────────────────────────────────────

let setupMode = false;

async function checkAuth() {
  try {
    const s = await api('/api/auth/status');
    setupMode = s.setupMode;
  } catch (e) {
    setupMode = false;
  }

  txt(el('loginHint'), setupMode
    ? 'İlk kurulum: cihaz için bir parola belirleyin (en az 8 karakter).'
    : 'Devam etmek için cihaz parolanızı girin.');
  show(el('pw2'), setupMode);
  show(el('pw2Label'), setupMode);
  el('loginBtn').textContent = setupMode ? 'Parolayı Belirle ve Başla' : 'Giriş Yap';
}

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
    afterLogin();
  } catch (err) {
    txt(e, 'Giriş başarısız. Parolanızı kontrol edin.');
  }
}

/// Girişten sonra bir kez çekilen veriler.
///
/// Ürün ve yapılandırma panelin ilk çiziminde gerekiyor: hedef bantlar
/// olmadan sensör kartları "iyi/kötü" diyemez, bağlı cihaz listesi olmadan
/// kontrol kartları "bu cihaz bağlı değil" uyarısını gösteremez.
async function afterLogin() {
  switchView('home');
  await loadCrop();
  try { store.config = await api('/api/config'); } catch (e) { /* panel yine çizilir */ }
  loadRules();   // panelin "kurallar hazır ama manuel modda" uyarısı için
  render();
}

// ── Başlatma ───────────────────────────────────────────────────────────────

function init() {
  // Tema İLK: `index.html` içindeki betik `<html data-theme>`'i zaten
  // damgaladı; buradaki çağrı yalnızca kontrolleri (başlık düğmesi, seçici)
  // o değerle eşitler.
  applyTheme();
  applyExpertMode();

  // --- Gezinme ---
  document.querySelectorAll('.tab-btn').forEach((t) => {
    t.onclick = () => switchView(t.dataset.view);
  });

  // Ayarlar kategorileri ve geri düğmeleri. Olay DELEGASYONU değil doğrudan
  // bağlama: bu ögeler statiktir ve yeniden çizilmezler.
  document.querySelectorAll('[data-set]').forEach((b) => {
    b.onclick = () => openSettings(b.dataset.set);
  });
  document.querySelectorAll('[data-back]').forEach((b) => {
    b.onclick = () => switchView('settings');
  });
  // `data-goto` kısayolları (ölçüm kutuları, "Tümü ›" bağlantıları) her
  // çizimde YENİDEN ÜRETİLİR. Tek tek bağlamak, `renderVitals()` sonrası
  // ölü dinleyiciler bırakırdı; bu yüzden delegasyon.
  document.addEventListener('click', (ev) => {
    const t = ev.target.closest && ev.target.closest('[data-goto]');
    if (t) switchView(t.dataset.goto);
  });

  // --- Tema ---
  el('themeBtn').onclick = toggleTheme;
  document.querySelectorAll('[data-theme-pick]').forEach((b) => {
    b.onclick = () => setTheme(b.dataset.themePick);
  });

  el('loginBtn').onclick = doLogin;
  ['pw', 'pw2'].forEach((id) => {
    el(id).addEventListener('keydown', (e) => { if (e.key === 'Enter') doLogin(); });
  });

  // Panel — hızlı eylemler
  el('quickWater').onclick = () => quickWater(true);
  el('quickStop').onclick = () => quickWater(false);

  // Kontrol
  el('estopBtn').onclick = () => {
    const m = el('estopMsg');
    m.className = 'form-feedback';
    txt(m, 'Acil durdurma gönderildi — tüm cihazlar kesiliyor…');
    sendCmd('system', 'emergencyStop');
  };
  // Temizleme artık ÖNCE koşulları kontrol edip SONRA doğruluyor (TASK-074).
  el('eclearBtn').onclick = clearEmergency;

  // Sihirbaz
  el('wizNext').onclick = wizardNext;
  el('wizBack').onclick = wizardBack;
  el('wizClose').onclick = closeWizard;
  el('wizard').addEventListener('click', (ev) => {
    // Karta değil, karartılmış zemine tıklandıysa kapat.
    if (ev.target === el('wizard')) closeWizard();
  });

  // Uzman modu
  el('expertToggle').onchange = (ev) => setExpert(ev.target.checked);

  // Wi-Fi
  el('scanBtn').onclick = startScan;

  el('saveWifi').onclick = async () => {
    const m = el('netMsg');
    try {
      await api('/api/config/network', {
        method: 'PUT',
        body: JSON.stringify({ ssid: el('ssid').value, password: el('wifiPw').value }),
      });
      el('wifiPw').value = '';
      m.className = 'form-feedback msg';

      // ── NE OLACAĞINI ÖNCEDEN SÖYLE ─────────────────────────────────────
      // İlk kurulumda bağlantı kurulur kurulmaz cihaz kendini yeniden
      // başlatır ve kurulum ağı kapanır (§8.4). Bunu önceden söylemezsek
      // kullanıcı, tam da her şey yolundayken kopan bir sayfa görür ve
      // kurulumun başarısız olduğunu sanar.
      const inSetup = store.state && store.state.network && store.state.network.provisioning;
      txt(m, inSetup
        ? '✔ Kaydedildi. Cihaz ağa bağlanıyor; bağlanır bağlanmaz yeniden '
          + 'başlayacak ve kurulum ağı kapanacak. Yeni adres bu ekranda yazacak.'
        : '✔ Wi-Fi bilgileri kaydedildi, bağlanılıyor…');
    } catch (e) {
      m.className = 'form-feedback err';
      txt(m, e.message);
    }
  };

  el('forgetWifi').onclick = async () => {
    if (!confirm('Kayıtlı ağ silinsin mi? Cihaz kurulum (AP) moduna döner.')) return;
    try { await api('/api/network/forget', { method: 'POST' }); }
    catch (e) { txt(el('netMsg'), e.message); }
  };

  el('retryNow').onclick = () => api('/api/network/retry', { method: 'POST' })
    .then(() => txt(el('netMsg'), 'Ağa yeniden bağlanma deneniyor…'))
    .catch((e) => txt(el('netMsg'), e.message));

  // Uzman: grafik
  el('chartRefreshBtn').onclick = loadHistory;
  document.querySelectorAll('#chartSensors .chip-btn').forEach((btn) => {
    btn.onclick = () => {
      document.querySelectorAll('#chartSensors .chip-btn').forEach((b) => b.classList.remove('active'));
      btn.classList.add('active');
      store.activeChartSensor = btn.dataset.sensor;
      drawChart();
    };
  });

  // Uzman: teşhis ve kurallar
  el('refreshDiag').onclick = loadDiagnostics;
  el('addRule').onclick = () => {
    if (store.rules.length >= MAX_RULES) return;
    store.rules.push(newRule());
    renderRules();
    el('rulesMsg').className = 'form-feedback';
    txt(el('rulesMsg'), 'Yeni kural eklendi — etkinleştirip KAYDEDENE kadar çalışmaz.');
  };
  el('saveRules').onclick = saveRules;
  el('reloadRules').onclick = loadRules;

  // Parola
  el('savePw').onclick = changePassword;
  el('logoutBtn').onclick = doLogout;
  ['pwCurrent', 'pwNext', 'pwNext2'].forEach((id) => {
    el(id).addEventListener('keydown', (ev) => { if (ev.key === 'Enter') changePassword(); });
  });

  el('factoryBtn').onclick = async () => {
    if (!confirm('DİKKAT: tüm ayarlar, ürün seçimi, ağ bilgileri ve parola silinecek. Emin misiniz?')) return;
    try {
      await api('/api/system/factory-reset?confirm=FACTORY_RESET', { method: 'POST' });
      endSession('Cihaz fabrika ayarlarına döndürüldü — kurulum ağından yeniden başlayın.');
    } catch (e) {
      alert(e.message);
    }
  };

  setLinked(false);
  showLogin();
}

document.addEventListener('DOMContentLoaded', init);
