const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

ws.onopen = () => {
  console.log("✅ WebSocket bağlı");
  const wifiMode = document.getElementById("wifi-mode");
  if (wifiMode) wifiMode.textContent = "Bağlantı aktif!";
};

ws.onmessage = (event) => {
  try {
    const data = JSON.parse(event.data);

    // Röle durumu güncelle
    const btn = document.getElementById(`r${data.id}`);
    if (btn) {
      btn.className = `btn ${data.state === "ON" ? "on" : "off"}`;
    }

    // IP adresini göster
    if (data.ip) {
      const ipSpan = document.getElementById("ip");
      if (ipSpan) ipSpan.textContent = data.ip;
    }
  } catch (err) {
    console.error("WebSocket hatası:", err);
  }
};

ws.onclose = () => {
  const wifiMode = document.getElementById("wifi-mode");
  if (wifiMode) wifiMode.textContent = "Bağlantı koptu 😕";
};

// Röleye komut gönder
function sendCmd(id) {
  ws.send(JSON.stringify({ id }));
}

// WiFi tarama
function scanNetworks() {
  const status = document.getElementById('status');
  if (status) status.innerText = 'Tarama yapılıyor... bir kaç saniye bekleyin.';

  fetch('/scan')
    .then(resp => resp.json())
    .then(data => {
      const sel = document.getElementById('networks');
      sel.innerHTML = '<option value="">-- Ağa seçin --</option>';

      data.forEach(function (item) {
        const opt = document.createElement('option');
        opt.value = item.ssid;
        opt.text = item.ssid + ' (' + item.rssi + ' dBm)';
        sel.appendChild(opt);
      });

      if (status) status.innerText = 'Tarama tamamlandı. Listeden seç veya elle gir.';
    })
    .catch(err => {
      if (status) status.innerText = 'Tarama hatası: ' + err;
    });
}

// SSID otomatik doldurma
const networkSelect = document.getElementById('networks');
if (networkSelect) {
  networkSelect.addEventListener('change', function () {
    if (this.value) {
      const ssidField = document.getElementById('ssid');
      if (ssidField) ssidField.value = this.value;
    }
  });
}

/* -------------------------------------------------------------
   ⭐⭐⭐ SQLite TABLO VERİSİ YÜKLEME FONKSİYONU
--------------------------------------------------------------*/
function loadTable() {
  fetch('/api/get-rows')
    .then(r => r.json())
    .then(rows => {
      let tbody = document.querySelector('#dbTable tbody');
      if (!tbody) return;

      tbody.innerHTML = "";

      rows.forEach(r => {
        tbody.innerHTML += `
          <tr>
            <td>${r.id}</td>
            <td>${r.sensor}</td>
            <td>${r.value}</td>
            <td>${r.time}</td>
          </tr>
        `;
      });
    })
    .catch(err => console.error("SQL tablo yükleme hatası:", err));
}

// Sayfa açılınca tabloyu doldur
if (document.getElementById("dbTable")) {
  loadTable();
}
