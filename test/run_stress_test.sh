#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT=3001
cd "$ROOT"

SPID=$(bash test/start_server.sh "$PORT" | tail -1)
trap "kill $SPID 2>/dev/null || true" EXIT

echo "=== Running stress test (100 rounds, 5ms delay) ==="
node test/chat_stress.js --rounds=100 --delay-ms=5 --port="$PORT"
RES=$?
echo "=== Test result: $RES ==="
exit $RES
