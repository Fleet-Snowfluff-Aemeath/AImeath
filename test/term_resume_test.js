var WebSocket = require('ws');
var PORT = process.env.PORT || 3091;
var SERVER = 'ws://127.0.0.1:' + PORT;
var WID = 'term_resume_' + Date.now();

function connectAndCollect(msg, drainSec) {
  return new Promise(function(resolve) {
    var ws = new WebSocket(SERVER);
    var out = [];
    ws.on('message', function(d) {
      try { out.push(JSON.parse(d.toString())); } catch(_) {}
    });
    ws.on('open', function() { ws.send(JSON.stringify(msg)); });
    ws.on('error', function() {});
    setTimeout(function() {
      resolve({ ws: ws, out: out });
    }, (drainSec || 3) * 1000);
  });
}

async function main() {
  console.log('[TERM RESUME TEST] wid=' + WID);

  console.log('[1/5] create terminal');
  var r1 = await connectAndCollect({
    app: 'terminal', action: 'exec',
    cmd: 'bash --norc',
    window_id: WID, display_name: 'test'
  }, 3);
  var outputs1 = r1.out.filter(function(r) { return r.type === 'output'; });
  console.log('  output msgs: ' + outputs1.length);

  console.log('[2/5] echo command');
  r1.ws.send(JSON.stringify({ action: 'stdin', data: 'echo MARKER_A\n' }));
  await new Promise(function(r) { setTimeout(r, 1500); });
  var alive = r1.out.some(function(r) {
    return r.type === 'output' && r.text && r.text.indexOf('MARKER_A') >= 0;
  });
  console.log('  MARKER_A found: ' + alive);

  console.log('[3/5] disconnect');
  r1.ws.close();
  await new Promise(function(r) { setTimeout(r, 1000); });

  console.log('[4/5] reconnect resume');
  var r2 = await connectAndCollect({
    app: 'terminal', action: 'resume', window_id: WID
  }, 3);
  var hasSession = r2.out.some(function(r) { return r.type === 'session'; });
  console.log('  session: ' + hasSession);

  console.log('[5/5] echo on restored PTY');
  r2.ws.send(JSON.stringify({ action: 'stdin', data: 'echo MARKER_B\n' }));
  await new Promise(function(r) { setTimeout(r, 1500); });
  var restored = r2.out.some(function(r) {
    return r.type === 'output' && r.text && r.text.indexOf('MARKER_B') >= 0;
  });
  console.log('  MARKER_B found: ' + restored);

  r2.ws.send(JSON.stringify({ action: 'close_window', window_id: WID }));
  r2.ws.close();

  var pass = outputs1.length > 0 && alive && hasSession && restored;
  console.log('');
  console.log('[TERM RESUME TEST] ' + (pass ? 'PASS' : 'FAIL'));
  process.exit(pass ? 0 : 1);
}

main().catch(function(e) {
  console.error('crashed:', e.message);
  process.exit(1);
});
