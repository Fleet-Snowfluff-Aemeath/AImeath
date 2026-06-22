# Game —— 游戏基础设施库

C++17 库，提供游戏模块的基类和通用工具，编译为共享库 `libgame.so`。

## 特性

- **Game 抽象基类**（`game_base.hpp`）—— 定义统一接口 `tick()`/`isOver()`/`score()`/`getState()`
- **C API 宏**（`game_api.hpp`）—— `GAME_API_DECL()` / `GAME_API_COMMON()` 统一 `extern "C"` 函数声明和实现
- **方向工具**（`direction.hpp`）—— `Direction` 枚举 + `isOppositeDir()` + `applyDir()` 内联函数
- **模块加载器**（`gamemod.hpp`）—— `GameModule` / `GameModuleCache` 线程安全的 dlopen 缓存加载器

## 目录结构

```
├── include/         头文件
│   ├── game_base.hpp
│   ├── game_api.hpp
│   ├── direction.hpp
│   └── gamemod.hpp
├── src/             源文件实现
│   └── gamemod.cpp
├── test/            单元测试（Google Test）
│   └── game_test.cpp
├── benchmark/       性能测试（Google Benchmark）
│   └── game_bench.cpp
├── test_build.sh    快速构建脚本
├── CMakeLists.txt   构建配置
└── README.md        本文档
```

## 依赖

| 依赖 | 用途 |
|------|------|
| C++17 编译器 | 语言标准 |
| CMake ≥ 3.12 | 构建系统 |
| Boost（filesystem） | `boost::dll::shared_library` 依赖 |
| Google Test | 测试框架 |
| Google Benchmark | 基准测试框架 |

## 构建

```bash
cd game
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

构建产物：`output/lib/libgame.so`

## 测试

```bash
cd build
make tests
./output/test/tests
```

## 基准测试

```bash
cd build
make bench_game
./output/bench/bench_game --benchmark_min_time=0.1
```

## 设计要点

- **与 core 解耦**：game 模块仅依赖 Boost，不依赖 core。游戏模块（snake/gomoku/pacman）仅需链接 game，无需链接 core
- **宏消除样板**：`GAME_API_COMMON()` 一行替代每模块 5 个 `extern "C"` 函数实现
- **方向逻辑共享**：snake 和 pacman 共用 `Direction` / `isOppositeDir()` / `applyDir()`，由 game 模块提供
- **动态加载**：`GameModuleCache` 提供线程安全的 dlopen 加载和缓存，供 demo 和 gameserver 共用
