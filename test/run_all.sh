#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PORT=$(python3 -c "import json; print(json.load(open('$ROOT/config.json')).get('port', 3001))" 2>/dev/null || echo 3001)

cd "$ROOT"

echo "=== Starting server (port $PORT) ==="
pkill -9 AImeath 2>/dev/null || true
sleep 1
bash test/start_server.sh "$PORT" > /tmp/aimeath_test_pid.txt 2>&1
SPID=$(tail -1 /tmp/aimeath_test_pid.txt)
cat /tmp/aimeath_test_pid.txt

cleanup() { kill $SPID 2>/dev/null || true; }
trap cleanup EXIT

PASS=0
FAIL=0

run_test() {
  local name="$1"
  local script="$2"
  echo ""
  echo "=== $name ==="
  PORT=$PORT node "$script" 2>&1
  local rc=$?
  echo "  exit=$rc"
  if [ $rc -eq 0 ]; then
    echo "  $name: PASS"
    PASS=$((PASS + 1))
  else
    echo "  $name: FAIL"
    FAIL=$((FAIL + 1))
  fi
}

run_test "GAME RESUME"        test/resume_test.js
run_test "TERM RESUME"        test/term_resume_test.js
run_test "STABILITY LOAD"     test/stability_under_load.js
run_test "CONCURRENT (500)"   test/concurrent_connections.js
run_test "OVERLOAD"           test/overload_protection.js

echo ""
echo "=========================================="
echo " RESULTS: $PASS passed, $FAIL failed"
echo "=========================================="

exit $FAIL
