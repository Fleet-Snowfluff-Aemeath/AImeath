#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${1:-3001}"
CFG="${2:-config.json}"

cd "$ROOT"

# Kill existing server on this port only
PID=$(lsof -ti :"$PORT" 2>/dev/null || true)
if [ -n "$PID" ]; then
  echo "Killing existing server on port $PORT (PID=$PID)"
  kill "$PID" 2>/dev/null || true
  sleep 1
fi

# Start server
echo "Starting server on port $PORT..."
LD_LIBRARY_PATH=build/output/lib ./build/output/AImeath &
SPID=$!

# Wait for server to accept connections
for i in $(seq 1 30); do
  if nc -z localhost "$PORT" 2>/dev/null; then
    echo "Server ready (PID=$SPID)"
    break
  fi
  if ! kill -0 "$SPID" 2>/dev/null; then
    echo "Server failed to start"
    exit 1
  fi
  sleep 1
done

echo "$SPID"
