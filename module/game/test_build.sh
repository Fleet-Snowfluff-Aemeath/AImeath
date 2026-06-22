mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) tests bench_game 2>&1
if [ $? -eq 0 ]; then echo "=== Running tests ==="; ./output/test/tests; fi
if [ $? -eq 0 ]; then echo "=== Running benchmarks ==="; ./output/bench/bench_game --benchmark_min_time=0.1; fi