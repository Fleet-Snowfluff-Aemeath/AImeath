# 创建构建目录
mkdir -p build && cd build
# CMake生成构建文件
cmake .. -DCMAKE_BUILD_TYPE=Release
# 编译所有目标（含 demo 和 gameserver）
make -j$(nproc)
echo ""
echo "=== 构建完成 ==="
echo "运行 demo : LD_LIBRARY_PATH=output/lib ./output/demo"
echo "运行服务器: LD_LIBRARY_PATH=output/lib ./output/gameserver"
echo "启动前端:  cd frontend && npx vite --host 0.0.0.0"
