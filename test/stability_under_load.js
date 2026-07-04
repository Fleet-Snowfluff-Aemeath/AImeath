#!/usr/bin/env node
const WebSocket = require('ws');

const PORT = process.env.PORT || 3091;
const BASE_CONN = parseInt(process.env.BASE_CONN || '50', 10);
const NEW_CONN = parseInt(process.env.NEW_CONN || '100', 10);
const SERVER = `ws://127.0.0.1:${PORT}`;

let baseDisconnects = 0;
const baseSockets = [];

function makeMsg() {
  return JSON.stringify({ game: 'snake', action: 'new_game', width: 10, height: 10 });
}

function connectBase(index) {
  return new Promise((resolve) => {
    const ws = new WebSocket(SERVER);
    const timer = setTimeout(() => {
      ws.close();
      resolve(false);
    }, 5000);

    ws.on('open', () => {
      clearTimeout(timer);
      baseSockets.push(ws);
      ws.send(makeMsg(index));
      resolve(true);
    });

    ws.on('close', () => {
      baseDisconnects++;
    });

    ws.on('error', () => {
      clearTimeout(timer);
      resolve(false);
    });
  });
}

function connectNew(index) {
  return new Promise((resolve) => {
    const ws = new WebSocket(SERVER);
    const timer = setTimeout(() => {
      ws.close();
      resolve();
    }, 5000);

    ws.on('open', () => {
      clearTimeout(timer);
      ws.close();
      resolve();
    });

    ws.on('error', () => {
      clearTimeout(timer);
      resolve();
    });
  });
}

async function main() {
  console.log(`[DDT] Stability under load: base=${BASE_CONN} new=${NEW_CONN}`);

  const tasks = [];
  for (let i = 0; i < BASE_CONN; i++) {
    tasks.push(connectBase(i));
  }
  const baseResults = await Promise.all(tasks);
  const baseOk = baseResults.filter(Boolean).length;
  console.log(`[DDT] Base connections established: ${baseOk}/${BASE_CONN}`);

  await new Promise(r => setTimeout(r, 1000));

  if (baseDisconnects > 0) {
    console.log(`[DDT] WARNING: ${baseDisconnects} base connections dropped before load`);
  }

  const newTasks = [];
  for (let i = 0; i < NEW_CONN; i++) {
    newTasks.push(connectNew(i));
  }
  await Promise.all(newTasks);

  await new Promise(r => setTimeout(r, 2000));

  const newDisconnects = baseDisconnects;
  console.log(`[DDT] After adding ${NEW_CONN} new connections: base disconnects = ${newDisconnects}`);

  baseSockets.forEach((ws) => {
    try { ws.close(); } catch (_) {}
  });

  const pass = newDisconnects === 0;
  console.log(`[DDT] Result: ${pass ? 'PASS' : 'FAIL'}`);
  process.exit(pass ? 0 : 1);
}

main().catch((err) => {
  console.error('[DDT] Test crashed:', err.message);
  process.exit(1);
});
