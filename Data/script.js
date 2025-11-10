const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

ws.onopen = () => {
  console.log("✅ WebSocket bağlı");
  document.getElementById("wifi-mode").textContent = "Bağlantı aktif!";
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
      document.getElementById("ip").textContent = data.ip;
    }
  } catch (err) {
    console.error("WebSocket hatası:", err);
  }
};

ws.onclose = () => {
  document.getElementById("wifi-mode").textContent = "Bağlantı koptu 😕";
};

// Röleye komut gönder
function sendCmd(id) {
  ws.send(JSON.stringify({ id }));
}
function scanNetworks() {
  document.getElementById('status').innerText = 'Tarama yapılıyor... bir kaç saniye bekleyin.';
  fetch('/scan').then(resp => resp.json()).then(data => {
    const sel = document.getElementById('networks');
    sel.innerHTML = '<option value="">-- Ağa seçin --</option>';
    data.forEach(function(item){
      const opt = document.createElement('option');
      opt.value = item.ssid;
      opt.text = item.ssid + ' (' + item.rssi + ' dBm)';
      sel.appendChild(opt);
    });
    document.getElementById('status').innerText = 'Tarama tamamlandı. Listeden seç veya elle gir.';
  }).catch(err => {
    document.getElementById('status').innerText = 'Tarama hatası: ' + err;
  });
}

document.getElementById('networks').addEventListener('change', function(){
  if(this.value) document.getElementById('ssid').value = this.value;
});

