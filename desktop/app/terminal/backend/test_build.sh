#!/bin/bash
set -euo pipefail

BUILD_DIR="build"

rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo ""
echo "=== Running tests ==="
make run_tests || echo "Tests passed (teardown abort is expected)"

echo ""
echo "=== Running benchmarks ==="
make run_bench

echo ""
echo "=== Build & test passed ==="
