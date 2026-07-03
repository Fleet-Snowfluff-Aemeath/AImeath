#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT=3001
cd "$ROOT"

SPID=$(bash test/start_server.sh "$PORT" | tail -1)
trap "kill $SPID 2>/dev/null || true" EXIT

echo "=== Running quick stress ==="
node test/chat_stress.js --rounds=5 --delay-ms=300 --port="$PORT"
RES=$?
echo "=== Result: $RES ==="
exit $RES
