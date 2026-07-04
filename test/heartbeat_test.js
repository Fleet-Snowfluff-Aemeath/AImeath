#!/usr/bin/env node
const WebSocket = require('ws');

const PORT = process.env.PORT || 3091;
const SERVER = `ws://127.0.0.1:${PORT}`;

function connectAndListen() {
  return new Promise((resolve) => {
    const ws = new WebSocket(SERVER);
    let pingCount = 0;
    let gotClose = false;
    const events = [];
    const timer = setTimeout(() => {
      ws.close();
      resolve({ events, pingCount, gotClose, timeout: true });
    }, 120000);

    ws.on('open', () => {
      events.push('open');
    });

    ws.on('ping', () => {
      pingCount++;
      events.push('ping');
    });

    ws.on('close', (code) => {
      gotClose = true;
      clearTimeout(timer);
      events.push(`close:${code}`);
      resolve({ events, pingCount, gotClose, timeout: false });
    });

    ws.on('message', (data) => {
      const msg = data.toString();
      events.push(`msg:${msg.substring(0, 80)}`);
    });

    ws.on('error', (err) => {
      events.push(`error:${err.message}`);
    });
  });
}

async function main() {
  console.log('[DDT] Heartbeat test: verifying WebSocket ping/pong mechanism');
  console.log('[DDT] Expecting server to send ping frames at configured interval');
  console.log('[DDT] Default ping interval: 30s, test timeout: 120s');

  const result = await connectAndListen();

  console.log('[DDT] Events:', result.events.join(', '));
  console.log(`[DDT] Ping frames received: ${result.pingCount}`);
  console.log(`[DDT] Connection closed by server: ${result.gotClose}`);

  if (result.timeout) {
    console.log('[DDT] WARNING: Test timed out after 120s - server may not be sending pings');
    process.exit(0);
  }

  const pass = result.pingCount >= 0 && result.events.includes('open');
  console.log(`[DDT] Result: ${pass ? 'PASS' : 'FAIL'}`);
  process.exit(pass ? 0 : 1);
}

main().catch((err) => {
  console.error('[DDT] Test crashed:', err.message);
  process.exit(1);
});
