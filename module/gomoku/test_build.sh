mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) tests bench_gomoku 2>&1
if ($?) { echo "=== Running tests ==="; ./output/test/tests }
if ($?) { echo "=== Running benchmarks ==="; ./output/bench/bench_gomoku }
