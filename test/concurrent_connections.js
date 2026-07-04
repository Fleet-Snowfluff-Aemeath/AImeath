#!/usr/bin/env node
const WebSocket = require('ws');

const PORT = process.env.PORT || 3091;
const CONCURRENT = parseInt(process.env.CONCURRENT || '500', 10);
const SERVER = `ws://127.0.0.1:${PORT}`;

let connected = 0;
let failed = 0;
let errors = [];
const sockets = [];

function connectOne(index) {
  return new Promise((resolve) => {
    const ws = new WebSocket(SERVER);
    const timer = setTimeout(() => {
      failed++;
      errors.push(`conn #${index} timeout`);
      ws.close();
      resolve();
    }, 10000);

    ws.on('open', () => {
      clearTimeout(timer);
      connected++;
      sockets.push(ws);
      resolve();
    });

    ws.on('error', (err) => {
      clearTimeout(timer);
      failed++;
      errors.push(`conn #${index} error: ${err.message}`);
      ws.close();
      resolve();
    });
  });
}

async function main() {
  console.log(`[DDT] Concurrent connections test: target=${CONCURRENT}`);
  const start = Date.now();

  const tasks = [];
  for (let i = 0; i < CONCURRENT; i++) {
    tasks.push(connectOne(i));
  }
  await Promise.all(tasks);

  const elapsed = (Date.now() - start) / 1000;
  console.log(`[DDT] Connected: ${connected} | Failed: ${failed} | Time: ${elapsed.toFixed(1)}s`);

  if (failed > 0) {
    console.log('[DDT] Failures:');
    errors.forEach(e => console.log(`  ${e}`));
  }

  sockets.forEach((ws) => {
    try { ws.close(); } catch (_) {}
  });

  const pass = connected >= CONCURRENT * 0.95;
  console.log(`[DDT] Result: ${pass ? 'PASS' : 'FAIL'}`);
  process.exit(pass ? 0 : 1);
}

main().catch((err) => {
  console.error('[DDT] Test crashed:', err.message);
  process.exit(1);
});
