#!/usr/bin/env node
/**
 * ws_smoke.js — WebSocket 连接冒烟测试
 *
 * 验证: 1. 连接成功  2. 发送消息收到回复  3. 正常关闭
 *
 * 用法:
 *   node test/ws_smoke.js [--port P]
 */

const WebSocket = require('ws');

const PORT = parseInt(process.argv.find(a => a.startsWith('--port='))?.split('=')[1] || '3001');
const URL = `ws://localhost:${PORT}`;

let opened = false;
let received = false;
let closed = false;

const ws = new WebSocket(URL);

ws.on('open', () => {
  opened = true;
  console.log(`[smoke] connected to ${URL}`);
  ws.send(JSON.stringify({ text: '/图片' }));
});

ws.on('message', (data) => {
  received = true;
  try {
    const msg = JSON.parse(data.toString());
    if (msg.type === 'stream_end') {
      console.log('[smoke] stream_end received, closing');
      ws.close();
    }
  } catch (_) {}
});

ws.on('close', (code) => {
  closed = true;
  console.log(`[smoke] closed code=${code}`);
  const ok = opened && received && closed;
  if (ok) {
    console.log('[smoke] PASS');
  } else {
    console.log(`[smoke] FAIL (opened=${opened} received=${received})`);
  }
  process.exit(ok ? 0 : 1);
});

ws.on('error', (err) => {
  console.error(`[smoke] error: ${err.message}`);
  process.exit(1);
});

setTimeout(() => {
  if (!received) {
    console.error('[smoke] timeout - no reply');
    process.exit(1);
  }
}, 10000);
