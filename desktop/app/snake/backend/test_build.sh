#!/bin/bash
set -e
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) tests bench_snake
echo ""
echo "=== 构建完成 ==="
echo "运行测试: ./output/test/tests"
./output/test/tests
echo ""
echo "运行基准: ./output/bench/bench_snake"
./output/bench/bench_snake
