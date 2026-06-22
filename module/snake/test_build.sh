# 创建构建目录
mkdir -p build && cd build
# CMake生成构建文件
cmake ..
# 编译
make -j$(nproc)