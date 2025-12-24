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
  const isPump = (id === 1);

  const cardId = isPump ? 'pumpCard' : 'oxyCard';
  const statusId = isPump ? 'pumpStatus' : 'oxyStatus';

  const card = document.getElementById(cardId);
  const status = document.getElementById(statusId);

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


// Parola göster/gizle
function togglePass() {
  const pass = document.getElementById("pass");
  const icon = document.getElementById("passIcon");

  if (pass.type === "password") {
    pass.type = "text";
    icon.classList.replace("bi-eye-slash", "bi-eye");
  } else {
    pass.type = "password";
    icon.classList.replace("bi-eye", "bi-eye-slash");
  }
}

// WiFi tarama
function scanNetworks() {
  fetch("/scan")
    .then(res => res.json())
    .then(list => {
      const select = document.getElementById("networks");
      select.innerHTML = "";

      list.forEach(n => {
        const opt = document.createElement("option");
        opt.value = n.ssid;
        opt.innerText = `${n.ssid} (${n.rssi} dBm)`;
        select.appendChild(opt);
      });

      select.addEventListener("change", () => {
        document.getElementById("ssid").value = select.value;
      });
    })
    .catch(() => alert("Ağ taraması yapılamadı!"));
}
// 🧹 Sensor kayıtlarını temizle
function clearSensors() {
  if (!confirm("Tüm sensör kayıtları silinsin mi?")) return;

  fetch("/api/clear-sensor", {
    method: "POST"
  })
    .then(res => res.json())
    .then(data => {
      if (data.ok) {
        alert("✅ Sensör kayıtları temizlendi");

        // tablo varsa yenile
        if (typeof loadData === "function") {
          loadData();
        }
      } else {
        alert("❌ Kayıtlar silinemedi");
      }
    })
    .catch(err => {
      console.error("Clear hatası:", err);
      alert("❌ Sunucu hatası");
    });
}



