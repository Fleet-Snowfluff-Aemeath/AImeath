var WebSocket = require('ws');
var PORT = process.env.PORT || 3091;
var SERVER = 'ws://127.0.0.1:' + PORT;
var WID = 'resume_test_' + Date.now();
var ws = null;

function sendMsg(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg));
}

function waitMsg(timeout) {
  return new Promise(function(resolve) {
    var msgs = [];
    function handler(d) {
      try { msgs.push(JSON.parse(d.toString())); } catch(_) {}
    }
    ws.on('message', handler);
    setTimeout(function() {
      ws.removeListener('message', handler);
      resolve(msgs);
    }, timeout || 2000);
  });
}

async function main() {
  console.log('[RESUME TEST] wid=' + WID);

  ws = new WebSocket(SERVER);
  await new Promise(function(r) {
    ws.on('open', function() {
      console.log('[1/4] send new_game');
      sendMsg({ game: 'snake', action: 'new_game', width: 10, height: 10, window_id: WID, display_name: 'test' });
      r();
    });
  });
  var msgs1 = await waitMsg(2000);
  var hasState = msgs1.some(function(r) { return r.type === 'snake'; });
  var hasSession = msgs1.some(function(r) { return r.type === 'session'; });
  console.log('  session=' + hasSession + ' snake=' + hasState);

  console.log('[2/4] send tick');
  sendMsg({ action: 'tick', value: 3, window_id: WID });
  var msgs2 = await waitMsg(1000);
  var tickOk = msgs2.some(function(r) { return r.type === 'snake'; });
  console.log('  tick response: ' + (tickOk ? 'OK' : 'MISSING'));

  console.log('[3/4] disconnect and reconnect (resume)');
  ws.close();
  await new Promise(function(r) { setTimeout(r, 800); });

  ws = new WebSocket(SERVER);
  await new Promise(function(r) {
    ws.on('open', function() {
      sendMsg({ game: 'snake', action: 'resume', window_id: WID });
      r();
    });
  });
  var msgs3 = await waitMsg(2000);
  var resumedSession = msgs3.some(function(r) { return r.type === 'session'; });
  console.log('  resume: session=' + resumedSession + ' msgs=' + msgs3.length);

  console.log('[4/4] tick after resume');
  sendMsg({ action: 'tick', value: 3, window_id: WID });
  var msgs4 = await waitMsg(1000);
  var liveOk = msgs4.some(function(r) { return r.type === 'snake'; });
  console.log('  tick: ' + (liveOk ? 'OK' : 'MISSING'));

  sendMsg({ action: 'close_window', window_id: WID });
  ws.close();

  var pass = hasState && tickOk && resumedSession && liveOk;
  console.log('');
  console.log('[RESUME TEST] ' + (pass ? 'PASS' : 'FAIL'));
  process.exit(pass ? 0 : 1);
}

main().catch(function(e) {
  console.error('[RESUME TEST] crashed:', e.message);
  process.exit(1);
});
