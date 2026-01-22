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
    if (data.id !== undefined) {
      const btn = document.getElementById(`r${data.id}`);
      if (btn) {
        btn.className = `btn ${data.state === "ON" ? "on" : "off"}`;
      }
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
  const isPump = (id === 1);

  const cardId = isPump ? 'pumpCard' : 'oxyCard';
  const statusId = isPump ? 'pumpStatus' : 'oxyStatus';

  const card = document.getElementById(cardId);
  const status = document.getElementById(statusId);

  if (!card || !status) return;

  const isOff = card.classList.contains('off');

  if (isOff) {
    card.classList.replace('off', 'on');
    status.innerText = "ÇALIŞIYOR";
    console.log((isPump ? "PUMP" : "OXY") + " Komutu: ON");
  } else {
    card.classList.replace('on', 'off');
    status.innerText = "KAPALI";
    console.log((isPump ? "PUMP" : "OXY") + " Komutu: OFF");
  }

  ws.send(JSON.stringify({ id }));
}

// Şifre göster/gizle
function togglePass() {
  const pass = document.getElementById("pass");

  if (pass.type === "password") {
    pass.type = "text";
  } else {
    pass.type = "password";
  }
}

// WiFi tarama
function scanNetworks() {
  fetch("/scan")
    .then(res => res.json())
    .then(list => {
      const select = document.getElementById("networks");
      if (!select) return;

      select.innerHTML = "";

      list.forEach(n => {
        const opt = document.createElement("option");
        opt.value = n.ssid;
        opt.innerText = `${n.ssid} (${n.rssi} dBm)`;
        select.appendChild(opt);
      });

      // Burada da yazıyoruz (ilk yöntem)
      select.onchange = () => {
        const ssidInput = document.getElementById("ssid");
        if (ssidInput) ssidInput.value = select.value;
      };
    })
    .catch(() => alert("Ağ taraması yapılamadı!"));
}

// 🧹 Sensor kayıtlarını temizle
function clearSensors() {
  if (!confirm("Tüm sensör kayıtları silinsin mi?")) return;

  fetch("/api/clear-sensor", { method: "POST" })
    .then(res => res.json())
    .then(data => {
      if (data.ok) alert("✅ Sensör kayıtları temizlendi");
      else alert("❌ Kayıtlar silinemedi");
    })
    .catch(err => {
      console.error("Clear hatası:", err);
      alert("❌ Sunucu hatası");
    });
}

function readFlow() {
  fetch("/api/sensors")
    .then(res => res.json())
    .then(data => {
      const flowVal = document.getElementById("flow-val");
      const tempVal = document.getElementById("temp-val");

      if (flowVal && data.waterFlow !== undefined) {
        flowVal.firstChild.nodeValue = data.waterFlow; // INTEGER
      }

      if (tempVal && data.temperature !== undefined) {
        tempVal.firstChild.nodeValue = data.temperature; // INTEGER
      }
    })
    .catch(err => {
      console.error("❌ Sensorler okunamadı:", err);
    });
}


// ⏱️ 600 ms
setInterval(readFlow, 600);

function setSSID(val) {
  const ssidInput = document.getElementById("ssid");
  if (ssidInput) {
    ssidInput.value = val;
    console.log("SSID yazıldı:", val);
  }
}