#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

pkill -9 gameserver 2>/dev/null || true
sleep 2

echo "=== Starting server ==="
LD_LIBRARY_PATH=build/output/lib ./build/output/gameserver &
SPID=$!
echo "Server PID=$SPID"
sleep 3

echo "=== Running test ==="
node test/chat_stress.js --rounds=5 --delay-ms=300
RES=$?

echo "=== Result: $RES ==="
kill $SPID 2>/dev/null
wait $SPID 2>/dev/null
exit $RES
