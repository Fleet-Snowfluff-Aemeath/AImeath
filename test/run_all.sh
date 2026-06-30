#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"
pkill -9 -f gameserver 2>/dev/null
sleep 1
./build/output/gameserver > /tmp/srv.log 2>&1 &
sleep 2

echo "=== BURST ==="
node test/chat_burst.js --rounds=5
BURST_EXIT=$?
echo "exit=$BURST_EXIT"

echo "=== CONCURRENT ==="
node test/chat_concurrent.js --connections=5 --rounds=5
CONC_EXIT=$?
echo "exit=$CONC_EXIT"

echo "=== STRESS ==="
node test/chat_stress.js --rounds=5
STRESS_EXIT=$?
echo "exit=$STRESS_EXIT"

echo "=== SUMMARY ==="
echo "burst:   $BURST_EXIT"
echo "conc:    $CONC_EXIT"
echo "stress:  $STRESS_EXIT"

pkill -f gameserver 2>/dev/null
