#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "Root: $ROOT"

# Kill any existing server
pkill -f gameserver 2>/dev/null || true
sleep 1

# Start server from root so it can find config.json
cd "$ROOT"
LD_LIBRARY_PATH=build/output/lib ./build/output/gameserver &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 2

# Run stress test
cd "$ROOT"
echo "=== Running stress test (100 rounds, 5ms delay) ==="
node test/chat_stress.js --rounds=100 --delay-ms=5
RESULT=$?

echo "=== Test result: $RESULT ==="

# Stop server
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

exit $RESULT
