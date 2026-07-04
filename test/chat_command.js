#!/usr/bin/env node
/**
 * chat_command.js — 测试聊天命令 (/图片,/音乐,/视频,/游戏)
 *
 * 验证各命令返回正确的 embed 输出
 *
 * 用法:
 *   node test/chat_command.js [--port P]
 *
 * 默认: 端口 3001
 */

const WebSocket = require('ws');

const PORT = parseInt(process.argv.find(a => a.startsWith('--port='))?.split('=')[1] || '3001');
const URL = `ws://localhost:${PORT}`;

const COMMANDS = [
  { text: '/图片', expect: 'embed', name: 'image' },
  { text: '/音乐', expect: 'embed', name: 'audio' },
  { text: '/视频', expect: 'embed', name: 'video' },
  { text: '/游戏 snake', expect: 'game', name: 'game' },
];

let idx = 0;
let recvCount = 0;
let done = false;
let startTime;

const ws = new WebSocket(URL);

ws.on('open', () => {
  startTime = Date.now();
  console.log(`[test] connected to ${URL}`);
  sendNext();
});

ws.on('message', (data) => {
  recvCount++;
  try {
    const msg = JSON.parse(data.toString());
    const expected = COMMANDS[idx - 1];

    if (msg.type === 'stream_end' && expected) {
      console.log(`[test] ${expected.name}: stream_end received`);
      if (idx < COMMANDS.length) {
        sendNext();
      } else if (!done) {
        done = true;
        const elapsed = Date.now() - startTime;
        console.log(`\n[test] === All ${COMMANDS.length} commands tested, ${recvCount} events, ${elapsed}ms ===`);
        setTimeout(() => ws.close(), 500);
      }
    }
  } catch (e) {
    console.log(`[test] recv #${recvCount} raw: ${data.toString().substring(0, 80)}`);
  }
});

ws.on('close', (code) => {
  console.log(`[test] disconnected code=${code}`);
  process.exit(done ? 0 : 1);
});

ws.on('error', (err) => {
  console.error(`[test] error: ${err.message}`);
  if (!done) process.exit(1);
});

function sendNext() {
  const cmd = COMMANDS[idx];
  console.log(`[test] send "${cmd.text}"`);
  ws.send(JSON.stringify({ text: cmd.text }));
  idx++;
}

setTimeout(() => {
  if (!done) {
    console.error('[test] timeout');
    process.exit(1);
  }
}, 15000);
