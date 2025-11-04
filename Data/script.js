var ws = new WebSocket(`ws://${window.location.hostname}/ws`);

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  document.getElementById('r'+data.id).className = 'btn ' + (data.state == "ON" ? 'on' : 'off');
};

function sendCmd(id) {
  ws.send(JSON.stringify({cmd:"toggle", id:id}));
}
