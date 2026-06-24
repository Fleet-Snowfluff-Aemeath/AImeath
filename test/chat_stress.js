#!/usr/bin/env node
/**
 * chat_stress.js — 高频多轮 WebSocket 压力测试
 *
 * 用法:
 *   node test/chat_stress.js [--rounds N] [--delay-ms D] [--port P]
 *
 * 默认: 20 轮, 50ms 间隔, 端口 3001
 */

const WebSocket = require('ws');

const ROUNDS = parseInt(process.argv.find(a => a.startsWith('--rounds='))?.split('=')[1] || '20');
const DELAY_MS = parseInt(process.argv.find(a => a.startsWith('--delay-ms='))?.split('=')[1] || '50');
const PORT = parseInt(process.argv.find(a => a.startsWith('--port='))?.split('=')[1] || '3001');
const URL = `ws://localhost:${PORT}`;

let recvCount = 0;
let sendCount = 0;
let connected = false;
let done = false;
let startTime;

const ws = new WebSocket(URL);

ws.on('open', () => {
    connected = true;
    startTime = Date.now();
    console.log(`[test] connected to ${URL}`);

    // First message establishes the chat session
    sendMessage('hello from stress test');
});

ws.on('message', (data) => {
    recvCount++;
    try {
        const msg = JSON.parse(data.toString());
        const type = msg.type || 'unknown';
        // Log first few and stream_end events
        if (recvCount <= 5 || type === 'stream_end') {
            const preview = data.toString().substring(0, 80);
            console.log(`[test] recv #${recvCount} type=${type} ${preview}${data.toString().length > 80 ? '...' : ''}`);
        }
        if (type === 'stream_end') {
            sendCount++;
            if (sendCount < ROUNDS) {
                // Send next message after a brief delay
                setTimeout(() => {
                    if (connected) {
                        sendMessage(`round ${sendCount + 1}`);
                    }
                }, DELAY_MS);
            } else if (!done) {
                done = true;
                const elapsed = Date.now() - startTime;
                console.log(`\n[test] === Complete: ${ROUNDS} rounds, ${recvCount} recv, ${elapsed}ms ===`);
                // Wait a bit then close
                setTimeout(() => {
                    console.log('[test] closing connection');
                    ws.close();
                }, 500);
            }
        }
    } catch (e) {
        console.log(`[test] recv #${recvCount} raw: ${data.toString().substring(0, 80)}`);
    }
});

ws.on('close', (code, reason) => {
    connected = false;
    const reasonStr = reason ? reason.toString() : '';
    console.log(`[test] disconnected code=${code} reason="${reasonStr}"`);
    if (!done) {
        console.log(`[test] *** UNEXPECTED disconnect after ${sendCount}/${ROUNDS} rounds ***`);
    }
    process.exit(done ? 0 : 1);
});

ws.on('error', (err) => {
    console.error(`[test] error: ${err.message}`);
    if (!done) process.exit(1);
});

function sendMessage(text) {
    const msg = JSON.stringify({ text });
    console.log(`[test] send #${sendCount + 1}: "${text}"`);
    ws.send(msg);
}

// Initial send on first stream_end
setTimeout(() => {
    if (!connected && !done) {
        console.error('[test] connection timeout');
        process.exit(1);
    }
}, 10000);
