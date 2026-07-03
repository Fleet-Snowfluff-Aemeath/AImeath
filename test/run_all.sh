#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT=3001
cd "$ROOT"

# Start server
SPID=$(bash test/start_server.sh "$PORT" | tail -1)
trap "kill $SPID 2>/dev/null || true" EXIT

echo ""
echo "=== BURST ==="
node test/chat_burst.js --rounds=5 --port="$PORT"
BURST_EXIT=$?
echo "exit=$BURST_EXIT"

echo ""
echo "=== CONCURRENT ==="
node test/chat_concurrent.js --connections=5 --rounds=5 --port="$PORT"
CONC_EXIT=$?
echo "exit=$CONC_EXIT"

echo ""
echo "=== STRESS ==="
node test/chat_stress.js --rounds=5 --port="$PORT"
STRESS_EXIT=$?
echo "exit=$STRESS_EXIT"

echo ""
echo "=== SUMMARY ==="
echo "burst:   $BURST_EXIT"
echo "conc:    $CONC_EXIT"
echo "stress:  $STRESS_EXIT"

exit $(( BURST_EXIT + CONC_EXIT + STRESS_EXIT ))
