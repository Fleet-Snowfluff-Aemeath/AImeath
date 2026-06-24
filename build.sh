#!/bin/bash
set -e
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo ""
echo "=== 构建完成 ==="
echo "运行服务器: LD_LIBRARY_PATH=output/lib ./output/gameserver"
