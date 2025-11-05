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

    // IP adresini göster (isteğe bağlı olarak ESP JSON’unda döndürülebilir)
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
