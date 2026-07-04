#!/usr/bin/env node
/**
 * chat_concurrent.js — 多连接并发压力测试
 *
 * 建立 CONNECTIONS 个 WebSocket 连接，每个连接连续不等待发送 ROUNDS 条 "Hello"，
 * 验证每个连接都能收到 ROUNDS 个回复。
 *
 * 用法:
 *   node test/chat_concurrent.js [--connections N] [--rounds M] [--port P]
 *
 * 默认: 5 连接, 5 轮, 端口 3001
 */

const WebSocket = require('ws');

const CONNS = parseInt(process.argv.find(a => a.startsWith('--connections='))?.split('=')[1] || '5');
const ROUNDS = parseInt(process.argv.find(a => a.startsWith('--rounds='))?.split('=')[1] || '5');
const PORT = parseInt(process.argv.find(a => a.startsWith('--port='))?.split('=')[1] || '3001');
const URL = `ws://localhost:${PORT}`;

const results = [];
let completed = 0;
let failed = 0;
let startTime;

function runClient(id) {
    const state = {
        id,
        recvCount: 0,
        streamEndCount: 0,
        sendCount: 0,
        done: false,
        ws: null,
    };

    return new Promise((resolve) => {
        state.ws = new WebSocket(URL);

        state.ws.on('open', () => {
            console.log(`[conn #${id}] connected`);
            // Burst-send all messages immediately
            for (let i = 1; i <= ROUNDS; i++) {
                state.sendCount++;
                state.ws.send(JSON.stringify({ text: 'Hello' }));
            }
            console.log(`[conn #${id}] sent ${ROUNDS} messages`);
        });

        state.ws.on('message', (data) => {
            state.recvCount++;
            try {
                const msg = JSON.parse(data.toString());
                if (msg.type === 'stream_end') {
                    state.streamEndCount++;
                    if (state.streamEndCount >= ROUNDS && !state.done) {
                        console.log(`[conn #${id}] got ${state.streamEndCount}/${ROUNDS}, closing`);
                        state.ws.close();
                    }
                }
            } catch (_) {}
        });

        state.ws.on('close', (code, reason) => {
            const reasonStr = reason ? reason.toString() : '';
            console.log(`[conn #${id}] disconnected code=${code} reason="${reasonStr}"`);
            state.done = true;
            const ok = state.streamEndCount >= ROUNDS;
            if (!ok) {
                console.log(`[conn #${id}] *** FAIL: got ${state.streamEndCount}/${ROUNDS} stream_end ***`);
            }
            resolve({ id, ok, streamEndCount: state.streamEndCount, recvCount: state.recvCount, code });
        });

        state.ws.on('error', (err) => {
            console.error(`[conn #${id}] error: ${err.message}`);
        });
    });
}

async function main() {
    startTime = Date.now();
    console.log(`[test] starting ${CONNS} connections, ${ROUNDS} rounds each, target ${URL}`);

    const promises = [];
    for (let i = 1; i <= CONNS; i++) {
        promises.push(runClient(i));
    }

    const res = await Promise.all(promises);
    const elapsed = Date.now() - startTime;

    let allOk = true;
    let totalRecv = 0;
    let totalEnd = 0;
    for (const r of res) {
        totalRecv += r.recvCount;
        totalEnd += r.streamEndCount;
        if (!r.ok) {
            allOk = false;
            console.log(`[test]   conn #${r.id}: FAIL (${r.streamEndCount}/${ROUNDS})`);
        } else {
            console.log(`[test]   conn #${r.id}: OK (${r.streamEndCount}/${ROUNDS}, ${r.recvCount} events)`);
        }
    }

    console.log(`\n[test] === Complete: ${CONNS} connections x ${ROUNDS} rounds, ` +
                `${totalEnd}/${CONNS * ROUNDS} stream_end, ${totalRecv} events, ${elapsed}ms ===`);

    if (!allOk) {
        console.log(`[test] *** SOME CONNECTIONS FAILED ***`);
    }
    process.exit(allOk ? 0 : 1);
}

main().catch((e) => {
    console.error(`[test] fatal: ${e.message}`);
    process.exit(1);
});
