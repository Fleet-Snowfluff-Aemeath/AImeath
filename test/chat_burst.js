#!/usr/bin/env node
/**
 * chat_burst.js — 连续不等待发送 N 条消息，验证每条都有回复
 *
 * 用法:
 *   node test/chat_burst.js [--rounds N] [--port P]
 *
 * 默认: 10 轮, 端口 3001
 */

const WebSocket = require('ws');

const ROUNDS = parseInt(process.argv.find(a => a.startsWith('--rounds='))?.split('=')[1] || '10');
const PORT = parseInt(process.argv.find(a => a.startsWith('--port='))?.split('=')[1] || '3001');
const URL = `ws://localhost:${PORT}`;

let recvCount = 0;
let streamEndCount = 0;
let connected = false;
let done = false;
let startTime;
let lastType;

const ws = new WebSocket(URL);

ws.on('open', () => {
    connected = true;
    startTime = Date.now();
    console.log(`[test] connected to ${URL}`);

    // Burst-send all messages immediately, no waiting
    for (let i = 1; i <= ROUNDS; i++) {
        const msg = JSON.stringify({ text: 'Hello' });
        console.log(`[test] send #${i}: {"text":"Hello"}`);
        ws.send(msg);
    }
});

ws.on('message', (data) => {
    recvCount++;
    try {
        const msg = JSON.parse(data.toString());
        const type = msg.type || 'unknown';
        lastType = type;

        // Log first few of each type and stream_end
        if (recvCount <= 10 || type === 'stream_end') {
            const preview = data.toString().substring(0, 80);
            console.log(`[test] recv #${recvCount} type=${type} ${preview}${data.toString().length > 80 ? '...' : ''}`);
        }

        if (type === 'stream_end') {
            streamEndCount++;
            console.log(`[test] --- stream_end #${streamEndCount}/${ROUNDS} ---`);
            if (streamEndCount >= ROUNDS && !done) {
                done = true;
                const elapsed = Date.now() - startTime;
                console.log(`\n[test] === Complete: ${ROUNDS}/${ROUNDS} replies, ${recvCount} events, ${elapsed}ms ===`);
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
        console.log(`[test] *** UNEXPECTED disconnect after ${streamEndCount}/${ROUNDS} stream_end ***`);
    }
    process.exit(done && streamEndCount >= ROUNDS ? 0 : 1);
});

ws.on('error', (err) => {
    console.error(`[test] error: ${err.message}`);
    if (!done) process.exit(1);
});

setTimeout(() => {
    if (!connected && !done) {
        console.error('[test] connection timeout');
        process.exit(1);
    }
}, 10000);
