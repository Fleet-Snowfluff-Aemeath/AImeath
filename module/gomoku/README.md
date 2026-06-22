# gomoku/ —— 五子棋游戏

C++17 实现的五子棋游戏，编译为共享库 `libgomoku.so`，通过 `dlopen` 动态加载。

## 目录结构

```
├── include/          头文件
│   ├── board.hpp     棋盘网格（Cell 枚举、落子、查询）
│   └── game.hpp      Game 类、C API
├── src/              源文件实现
│   ├── board.cpp
│   └── game.cpp
├── test/             单元测试（Google Test）
│   └── gomoku_test.cpp
├── benchmark/        性能基准测试（Google Benchmark）
│   └── gomoku_bench.cpp
└── CMakeLists.txt    构建配置
```

## 规则

- 15×15 标准棋盘（可自定义大小）
- 黑白双方轮流落子，黑棋先手
- 先在横、竖、斜任一方向形成五子连珠者获胜

## 依赖

| 依赖 | 用途 |
|------|------|
| C++17 编译器 | 语言标准 |
| CMake ≥ 3.12 | 构建系统 |
| Boost::json | JSON 状态序列化（`getState()`） |
| Google Test | 单元测试框架 |
| Google Benchmark | 性能基准测试框架 |

## 构建

```bash
# 独立构建
cd module/gomoku
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 根项目构建（含 demo）
cd lab
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
- `getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON 状态
- `extern "C"` API 与 snake/pacman 模块一致，支持 `dlopen` / `dlsym` 动态加载
- 非法落子（越界、重复）被忽略，不切换玩家
- 游戏结束后所有 tick 被忽略，分数保持不变

## 相关文档

- [include/README.md](include/README.md) — 头文件说明
- [src/README.md](src/README.md) — 源文件实现
- [test/README.md](test/README.md) — 单元测试
- [benchmark/README.md](benchmark/README.md) — 性能基准测试
