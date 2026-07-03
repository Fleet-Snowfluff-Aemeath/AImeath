#!/usr/bin/env node
/**
 * ws_smoke.js — WebSocket 连接冒烟测试
 *
 * 验证: 1. 连接成功  2. 发送本地命令（无需API key）  3. 收到回复  4. 正常关闭
 *
 * 用法:
 *   node test/ws_smoke.js [--port P]
 */

const WebSocket = require('ws');

const PORT = parseInt(process.argv.find(a => a.startsWith('--port='))?.split('=')[1] || '3001');
const URL = `ws://localhost:${PORT}`;
const TIMEOUT = 30000;

let opened = false;
let received = false;
let closed = false;

const ws = new WebSocket(URL);

ws.on('open', () => {
  opened = true;
  console.log(`[smoke] connected to ${URL}`);
  // Send slash command — handled locally, no API key needed
  ws.send(JSON.stringify({ text: '/图片' }));
});

ws.on('message', (data) => {
  try {
    const msg = JSON.parse(data.toString());
    if (msg.type === 'stream_end') {
      received = true;
      console.log('[smoke] stream_end received, closing');
      ws.close();
    }
  } catch (_) {}
});

ws.on('close', (code) => {
  closed = true;
  console.log(`[smoke] closed code=${code}`);
  const ok = opened && received && closed;
  console.log(ok ? '[smoke] PASS' : `[smoke] FAIL (opened=${opened} received=${received})`);
  process.exit(ok ? 0 : 1);
});

ws.on('error', (err) => {
  console.error(`[smoke] error: ${err.message}`);
  process.exit(1);
});

setTimeout(() => {
  if (!received) {
    console.error(`[smoke] timeout after ${TIMEOUT}ms`);
    process.exit(1);
  }
}, TIMEOUT);
