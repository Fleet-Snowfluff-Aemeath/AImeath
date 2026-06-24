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

echo ""
echo "=== Starting Game Server (port 3001) ==="
LD_LIBRARY_PATH=build/output/lib ./build/output/gameserver &
SERVER_PID=$!
echo "GameServer PID: $SERVER_PID"
sleep 1

echo ""
echo "=== Starting Frontend (port 5173) ==="
cd desktop
npm run dev &
VITE_PID=$!
echo "Vite PID: $VITE_PID"

echo ""
echo "=== Ready! ==="
echo "GameServer:  http://localhost:3001  (WebSocket)"
echo "Frontend:    http://localhost:5173/ (browser)"
WSL_IP=$(ip addr show eth0 2>/dev/null | grep -oP 'inet \K[\d.]+' | head -1)
if [ -n "$WSL_IP" ]; then
  echo "WSL IP:     http://${WSL_IP}:5173/ (from Windows browser)"
fi

# Trap Ctrl+C to kill both
trap "echo ''; echo 'Shutting down...'; kill $SERVER_PID $VITE_PID 2>/dev/null; exit 0" INT TERM

# Wait for any child to exit
wait
