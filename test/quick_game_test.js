const WebSocket = require('ws');
const ws = new WebSocket('ws://127.0.0.1:3091');
ws.on('open', function() {
  console.log('WS OPEN');
  ws.send(JSON.stringify({game:'snake', action:'new_game', width:10, height:10}));
});
ws.on('message', function(d) {
  console.log('MSG:', d.toString().substring(0, 100));
  ws.close();
});
ws.on('close', function() {
  console.log('WS CLOSED');
  process.exit(0);
});
ws.on('error', function(e) {
  console.log('WS ERROR:', e.message);
  process.exit(1);
});
setTimeout(function() { console.log('TIMEOUT'); process.exit(1); }, 5000);
