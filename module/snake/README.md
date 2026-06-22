# snake/ —— 贪食蛇游戏

C++17 实现的终端贪食蛇游戏，编译为共享库 `libsnake.so`，通过 `dlopen` 动态加载。

## 目录结构

```
├── include/          头文件
│   ├── snake.hpp     Snake 实体（运动、碰撞）
│   ├── board.hpp     游戏面板（网格、食物生成）
│   └── game.hpp      游戏主循环、extern "C" 入口
├── src/              源文件实现
│   ├── snake.cpp
│   ├── board.cpp
│   └── game.cpp
├── test/             单元测试（Google Test）
│   ├── snake_test.cpp
│   └── README.md
├── benchmark/        性能基准测试（Google Benchmark）
│   ├── snake_bench.cpp
│   └── README.md
├── build/            构建输出目录
│   ├── output/lib/   共享库文件
│   ├── output/test/  测试可执行文件
│   └── output/bench/ 基准测试可执行文件
├── test_build.sh     快速构建脚本
└── CMakeLists.txt    构建配置
```

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
cd snake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 快速构建（使用脚本）
cd snake
./test_build.sh
```

构建产物：
- `output/lib/libsnake.so` — 共享库
- `output/test/tests` — 测试可执行文件
- `output/bench/bench_snake` — 基准测试可执行文件

## 测试

```bash
cd build
./output/test/tests
```

测试覆盖：
- **Snake**：初始状态、四个方向移动、尾部收缩、转向禁止（反向/缓冲）、自身碰撞检测、复杂路径
- **Board**：食物边界范围、随机生成不重复、食物避开蛇身
- **Game**：构造与销毁、游戏结束条件（撞墙）、Game Over 后 Tick 不崩溃、分数持久化

## 基准测试

```bash
cd build
./output/bench/bench_snake
```

基准覆盖：
- `BM_SnakeAdvance` — 蛇前进性能
- `BM_SnakeCollisionCheck` — 自身碰撞检测性能
- `BM_SnakeHasBodyAt` — 位置查询性能
- `BM_BoardGenerateFood` — 食物生成性能（大/小棋盘）

## 游戏操作

| 按键 | 功能 |
|------|------|
| W / ↑ / K | 向上移动 |
| S / ↓ / J | 向下移动 |
| A / ← / H | 向左移动 |
| D / → / L | 向右移动 |
| Q | 退出游戏 |

## 设计要点

- `Snake` 使用 `std::deque<Position>` 管理身体，头部在 front，尾部在 back
- 方向变更缓冲（`m_next_dir`），防止单 tick 内多次转向导致自身碰撞
- 输入采用 `termios` 原始模式 + `select()` 非阻塞轮询
- 渲染基于 ANSI 转义序列（光标定位、清屏），无需 curses 依赖
- `getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON 状态
- `extern "C"` API 与 gomoku/pacman 模块一致，支持 `dlopen` / `dlsym` 动态加载
- `atexit()` 注册终端恢复，确保异常退出时回显和光标恢复正常
