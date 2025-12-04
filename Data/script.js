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

// Tema değiştir
function toggleTheme() {
  const html = document.documentElement;
  const icon = document.getElementById("themeIcon");

  if (html.getAttribute("data-theme") === "light") {
    html.setAttribute("data-theme", "dark");
    icon.classList.replace("bi-moon-stars", "bi-brightness-high");
  } else {
    html.setAttribute("data-theme", "light");
    icon.classList.replace("bi-brightness-high", "bi-moon-stars");
  }
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



