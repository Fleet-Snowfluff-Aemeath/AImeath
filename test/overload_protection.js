#!/usr/bin/env node
const WebSocket = require('ws');

const PORT = process.env.PORT || 3091;
const TOTAL = parseInt(process.env.TOTAL || '200', 10);
const BATCH = parseInt(process.env.BATCH || '50', 10);
const SERVER = `ws://127.0.0.1:${PORT}`;

let connected = 0;
let failed = 0;
let errors = [];
const sockets = [];

function connectOne(index) {
  return new Promise((resolve) => {
    const ws = new WebSocket(SERVER);
    const timer = setTimeout(() => {
      ws.close();
      failed++;
      errors.push(`conn timeout #${index}`);
      resolve();
    }, 5000);

    ws.on('open', () => {
      clearTimeout(timer);
      connected++;
      sockets.push(ws);
      resolve();
    });

    ws.on('error', (err) => {
      clearTimeout(timer);
      failed++;
      errors.push(`conn #${index}: ${err.message}`);
      ws.close();
      resolve();
    });
  });
}

async function main() {
  console.log(`[DDT] Overload resilience test: total=${TOTAL} batch=${BATCH}`);

  const start = Date.now();
  for (let i = 0; i < TOTAL; i += BATCH) {
    const tasks = [];
    const end = Math.min(i + BATCH, TOTAL);
    for (let j = i; j < end; j++) {
      tasks.push(connectOne(j));
    }
    await Promise.all(tasks);
    await new Promise(r => setTimeout(r, 50));
  }

  const elapsed = (Date.now() - start) / 1000;
  console.log(`[DDT] Connected: ${connected} | Failed: ${failed} | Time: ${elapsed.toFixed(1)}s`);

  if (failed > 0) {
    console.log('[DDT] Failures:');
    errors.slice(0, 10).forEach(e => console.log(`  ${e}`));
    if (errors.length > 10) console.log(`  ... and ${errors.length - 10} more`);
  }

  sockets.forEach((ws) => {
    try { ws.close(); } catch (_) {}
  });

  const pass = connected >= TOTAL * 0.95;
  console.log(`[DDT] Result: ${pass ? 'PASS' : 'FAIL'} (${connected}/${TOTAL} connected)`);
  process.exit(pass ? 0 : 1);
}

main().catch((err) => {
  console.error('[DDT] Test crashed:', err.message);
  process.exit(1);
});
