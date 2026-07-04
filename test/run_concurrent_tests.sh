#!/bin/bash
set -e

PORT="${PORT:-3091}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo " DDT Concurrent Tests Suite"
echo " Server: ws://127.0.0.1:${PORT}"
echo "=========================================="

PASS=0
FAIL=0

run_test() {
  local name="$1"
  local script="$2"
  echo ""
  echo "--- [DDT] ${name} ---"
  if node "${SCRIPT_DIR}/${script}"; then
    echo "[DDT] ${name}: PASS"
    PASS=$((PASS + 1))
  else
    echo "[DDT] ${name}: FAIL"
    FAIL=$((FAIL + 1))
  fi
}

echo ""
echo "Step 0: Smoke test (verify server is reachable)"
node "${SCRIPT_DIR}/ws_smoke.js" || {
  echo "ERROR: Server not reachable at ws://127.0.0.1:${PORT}"
  echo "Please start the server first: sh start.sh"
  exit 1
}

run_test "Concurrent Connections (500)"  "concurrent_connections.js"
run_test "Stability Under Load"         "stability_under_load.js"
run_test "Overload Protection"          "overload_protection.js"

echo ""
echo "=========================================="
echo " DDT Results: ${PASS} passed, ${FAIL} failed"
echo "=========================================="

if [ "${FAIL}" -gt 0 ]; then
  exit 1
fi
exit 0
