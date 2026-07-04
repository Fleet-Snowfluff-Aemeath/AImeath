#!/bin/bash
# 一键启动：游戏服务器 + 前端开发服务器
# 用法: ./start.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Building C++ ==="
cd "$SCRIPT_DIR"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

BACKEND_PORT=$(python3 -c "import json; print(json.load(open('config.json')).get('port', 3001))" 2>/dev/null || echo 3001)
echo "Backend port from config.json: ${BACKEND_PORT}"

echo ""
echo "=== Starting Game Server (port ${BACKEND_PORT}) ==="
LD_LIBRARY_PATH=build/output/lib ./build/output/AImeath &
SERVER_PID=$!
echo "AImeath PID: $SERVER_PID"
sleep 1

echo ""
echo "=== Starting Frontend (port 5173) ==="
cd desktop
VITE_BACKEND_PORT=${BACKEND_PORT} npm run dev &
VITE_PID=$!
echo "Vite PID: $VITE_PID"

echo ""
echo "=== Ready! ==="
echo "Backend:  ws://localhost:${BACKEND_PORT}  (WebSocket)"
echo "Frontend: http://localhost:5173/ (browser)"
WSL_IP=$(ip addr show eth0 2>/dev/null | grep -oP 'inet \K[\d.]+' | head -1)
if [ -n "$WSL_IP" ]; then
  echo "WSL IP:   http://${WSL_IP}:5173/ (from Windows browser)"
fi

trap "echo ''; echo 'Shutting down...'; kill $SERVER_PID $VITE_PID 2>/dev/null; exit 0" INT TERM

wait
