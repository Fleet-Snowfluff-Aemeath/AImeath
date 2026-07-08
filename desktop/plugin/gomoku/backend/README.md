# gomoku/ —�?五子棋游�?
C++17 实现的五子棋游戏，编译为共享�?`libgomoku.so`，通过 `dlopen` 动态加载�?
## 目录结构

```
frontend/plugin/gomoku/
├── backend/
�?  ├── include/          头文�?�?  �?  ├── board.hpp     棋盘网格（Cell 枚举、落子、查询）
�?  �?  └── game.hpp      Game 类、C API
�?  ├── src/              源文件实�?�?  �?  ├── board.cpp
�?  �?  └── game.cpp
�?  ├── test/             单元测试（Google Test�?�?  �?  └── gomoku_test.cpp
�?  ├── benchmark/        性能基准测试（Google Benchmark�?�?  �?  └── gomoku_bench.cpp
�?  ├── test_build.sh     快速构建脚�?�?  └── CMakeLists.txt    构建配置
├── frontend/
�?  ├── index.vue      Vue 页面组件
�?  └── config.js         应用配置
└── README.md             （本文档�?```

## 规则

- 15×15 标准棋盘（可自定义大小）
- 黑白双方轮流落子，黑棋先�?- 先在横、竖、斜任一方向形成五子连珠者获�?
## 依赖

| 依赖 | 用�?|
|------|------|
| C++17 编译�?| 语言标准 |
| CMake �?3.12 | 构建系统 |
| Boost::json | JSON 状态序列化（`getState()`�?|
| Google Test | 单元测试框架 |
| Google Benchmark | 性能基准测试框架 |

## 构建

```bash
# 独立构建
cd frontend/plugin/gomoku/backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 根项目构�?cd ../../../../
./build.sh
```

## 测试

```bash
cd build
./output/test/tests
```

## 基准测试

```bash
cd build
./output/bench/bench_gomoku
```

## 设计要点

- 棋盘使用 `std::vector<Cell>` 一维数组，`pos = row * size + col`
- 落子后从该位置向 4 个方向（水平、垂直、两对角线）双向扫描计数
- `getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON 状�?- `extern "C"` API �?snake/pacman 模块一致，支持 `dlopen` / `dlsym` 动态加�?- 非法落子（越界、重复）被忽略，不切换玩�?- 游戏结束后所�?tick 被忽略，分数保持不变

## 前端页面

Vue 页面组件位于 `../frontend/index.vue`，通过 WebSocket 连接后端游戏服务器，使用 `useGame('gomoku')` 组合式函数管理游戏状态。点击棋盘落子�?
页面配置�?`../frontend/config.js`�?
## 相关文档

- [include/README.md](include/README.md) �?头文件说�?- [src/README.md](src/README.md) �?源文件实�?- [test/README.md](test/README.md) �?单元测试
- [benchmark/README.md](benchmark/README.md) �?性能基准测试
- [../frontend/README.md](../frontend/README.md) �?前端页面说明
