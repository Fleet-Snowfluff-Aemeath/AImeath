# pacman/ —— 吃豆豆游戏

C++17 实现的终端吃豆豆游戏，编译为共享库 `libpacman.so`，通过 `dlopen` 动态加载。

## 目录结构

```
├── include/          头文件
│   ├── board.hpp     棋盘网格（豆子生成、查询、移除）
│   └── game.hpp      Game 类、Direction 枚举、C API
├── src/              源文件实现
│   ├── board.cpp
│   └── game.cpp
├── test/             单元测试（Google Test）
│   └── pacman_test.cpp
├── benchmark/        性能基准测试（Google Benchmark）
│   └── pacman_bench.cpp
└── CMakeLists.txt    构建配置
```

## 规则

- 玩家在 N×M 网格中移动，方向键控制
- 豆子随机散布在网格上，经过即吃掉，每颗 +10 分
- 撞墙即游戏结束
- 吃完所有豆子即为胜利

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
cd module/pacman
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
./output/bench/bench_pacman
```

## 设计要点

- 棋盘使用 `std::vector<bool>` 标记豆子，O(1) 查询/移除
- 方向变更缓冲（`m_next_dir`）当前 tick 内首次按下后忽略后续输入（复用 snake 模式）
- `getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON 状态
- `extern "C"` API 与 snake/gomoku 模块一致，支持 `dlopen` / `dlsym` 动态加载
- 豆子生成数量自动截断不超过网格容量
- 游戏结束后所有 tick 被忽略，分数保持不变
