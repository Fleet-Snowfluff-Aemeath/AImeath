# snake/ —�?贪食蛇游�?
C++17 实现的贪食蛇游戏，编译为共享�?`libsnake.so`，通过 `dlopen` 动态加载�?
## 目录结构

```
frontend/plugin/snake/
├── backend/
�?  ├── include/          头文�?�?  �?  ├── snake.hpp     Snake 实体（运动、碰撞）
�?  �?  ├── board.hpp     游戏面板（网格、食物生成）
�?  �?  └── game.hpp      游戏主循环、extern "C" 入口
�?  ├── src/              源文件实�?�?  �?  ├── snake.cpp
�?  �?  ├── board.cpp
�?  �?  └── game.cpp
�?  ├── test/             单元测试（Google Test�?�?  �?  └── snake_test.cpp
�?  ├── benchmark/        性能基准测试（Google Benchmark�?�?  �?  └── snake_bench.cpp
�?  ├── build/            构建输出目录
�?  �?  ├── output/lib/   共享库文�?�?  �?  ├── output/test/  测试可执行文�?�?  �?  └── output/bench/ 基准测试可执行文�?�?  ├── test_build.sh     快速构建脚�?�?  └── CMakeLists.txt    构建配置
├── frontend/
�?  ├── index.vue      Vue 页面组件
�?  └── config.js         应用配置
└── README.md             （本文档�?```

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
cd frontend/plugin/snake/backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 快速构建（使用脚本�?cd frontendpluginp/snake/backend
./test_build.sh
```

构建产物�?- `output/lib/libsnake.so` �?共享�?- `output/test/tests` �?测试可执行文�?- `output/bench/bench_snake` �?基准测试可执行文�?
## 测试

```bash
cd build
./output/test/tests
```

测试覆盖�?- **Snake**：初始状态、四个方向移动、尾部收缩、转向禁止（反向/缓冲）、自身碰撞检测、复杂路�?- **Board**：食物边界范围、随机生成不重复、食物避开蛇身
- **Game**：构造与销毁、游戏结束条件（撞墙）、Game Over �?Tick 不崩溃、分数持久化

## 基准测试

```bash
cd build
./output/bench/bench_snake
```

基准覆盖�?- `BM_SnakeAdvance` �?蛇前进性能
- `BM_SnakeCollisionCheck` �?自身碰撞检测性能
- `BM_SnakeHasBodyAt` �?位置查询性能
- `BM_BoardGenerateFood` �?食物生成性能（大/小棋盘）

## 游戏操作

| 按键 | 功能 |
|------|------|
| W / �?/ K | 向上移动 |
| S / �?/ J | 向下移动 |
| A / �?/ H | 向左移动 |
| D / �?/ L | 向右移动 |
| Q | 退出游�?|

## 设计要点

- `Snake` 使用 `std::deque<Position>` 管理身体，头部在 front，尾部在 back
- 方向变更缓冲（`m_next_dir`），防止�?tick 内多次转向导致自身碰�?- 输入采用 `termios` 原始模式 + `select()` 非阻塞轮�?- 渲染基于 ANSI 转义序列（光标定位、清屏），无需 curses 依赖
- `getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON 状�?- `extern "C"` API �?gomoku/pacman 模块一致，支持 `dlopen` / `dlsym` 动态加�?- `atexit()` 注册终端恢复，确保异常退出时回显和光标恢复正�?
## 前端页面

Vue 页面组件位于 `../frontend/index.vue`，通过 WebSocket 连接后端游戏服务器，使用 `useGame('snake')` 组合式函数管理游戏状态。方向键/WASD 操控蛇移动�?
页面配置�?`../frontend/config.js`�?
## 相关文档

- [include/README.md](include/README.md) �?头文件说�?- [src/README.md](src/README.md) �?源文件实�?- [test/README.md](test/README.md) �?单元测试
- [benchmark/README.md](benchmark/README.md) �?性能基准测试
- [../frontend/README.md](../frontend/README.md) �?前端页面说明
