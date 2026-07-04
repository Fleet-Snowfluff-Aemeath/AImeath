#!/bin/bash
set -e
PORT=3091
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0

run_test() {
  local name="$1"
  local script="$2"
  echo ""
  echo "--- [DDT] ${name} ---"
  if PORT=${PORT} node "${SCRIPT_DIR}/${script}"; then
    echo "[DDT] ${name}: PASS"
    PASS=$((PASS + 1))
  else
    echo "[DDT] ${name}: FAIL"
    FAIL=$((FAIL + 1))
  fi
}

echo "=========================================="
echo " DDT Concurrent Tests Suite"
echo " Server: ws://127.0.0.1:${PORT}"
echo "=========================================="

run_test "Concurrent Connections (500)"  "concurrent_connections.js"
run_test "Stability Under Load"         "stability_under_load.js"
run_test "Overload Resilience (200)"    "overload_protection.js"

echo ""
echo "=========================================="
echo " DDT Results: ${PASS} passed, ${FAIL} failed"
echo "=========================================="

if [ "${FAIL}" -gt 0 ]; then
  exit 1
fi
exit 0
