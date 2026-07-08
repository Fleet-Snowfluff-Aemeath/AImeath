# Go —�?围棋游戏模块

C++17 实现的围棋游戏，编译为共享库 `libgo.so`，通过 `GameModule` 动态加载�?
遵循《中国围棋规则》（2002版）�?-9条、第21条核心规则�?
## 规则实现

| 规则 | 实现 |
|------|------|
| �?�?棋具 | 19x19 棋盘，黑白棋�?|
| �?�?下法 | 黑先白后，交替落子，支持虚着（pass�?|
| �?�?�?| 直线紧邻空点为气，同色连接为整体 |
| �?�?提子 | 无气之自动移�?|
| �?�?禁着�?| 禁止自杀（除非提对方子） |
| �?�?禁止全局同形 | 劫争检测（board hash 比较�?|
| �?�?终局 | 连续两次虚着或认�?|
| �?�?计算胜负 | 数子法（盘上活子+围空），黑贴3�?/4子（komi=3.75�?|
| �?1�?贴子 | 黑贴3.75子，等效黑白�?7.5黑胜 |

## 目录结构

```
frontend/plugin/go/
├── backend/
�?  ├── include/         头文�?�?  �?  ├── board.hpp    棋盘、气、提子、劫
�?  �?  └── game.hpp     GoGame 类、常�?�?  ├── src/             源文�?�?  �?  ├── board.cpp
�?  �?  └── game.cpp
�?  ├── test/            单元测试（Google Test�?2 用例�?�?  ├── benchmark/       性能测试（Google Benchmark�?�?  ├── test_build.sh    快速构建脚�?�?  └── CMakeLists.txt   构建配置
├── frontend/
�?  ├── index.vue     Vue 页面组件
�?  └── config.js        应用配置
└── README.md            本文�?```

## 依赖

| 依赖 | 用�?|
|------|------|
| C++17 编译�?| 语言标准 |
| CMake �?3.12 | 构建系统 |
| Boost::json | getState() JSON 序列�?|
| Game 模块 | Game 基类 + C API �?|

## 构建

```bash
cd frontend/plugin/go/backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 测试

```bash
cd build
make tests
LD_LIBRARY_PATH=output/lib ./output/test/tests
```

## 使用

```cpp
GoGame game;
game.tick(9 * 19 + 9);  // 黑下天元
game.tick(GoGame::PASS); // 白虚着
game.tick(GoGame::PASS); // 黑虚着 �?终局
int winner = game.score(); // 1=黑胜, 2=白胜
```

`tick()` 参数�?- `>= 0`：落子位�?`row * 19 + col`
- `-1`：虚着（PASS�?- `-2`：认输（RESIGN�?
## 设计要点

- **19×19 + 哨兵边框**：使�?SIZE1=21 的内嵌网格，边框细胞不计数为�?- **劫检�?*：基�?FNV-1a hash 的棋盘状态去�?- **数子�?*：flood-fill 计算每方围住的空点，边界接触异色石不计数
- **自杀检�?*：使用棋盘副本模拟，避免 const_cast

## 前端页面

Vue 页面组件位于 `../frontend/index.vue`，通过 WebSocket 连接后端游戏服务器，使用 `useGame('go')` 组合式函数管理游戏状态。点击棋盘落子，支持虚着（Pass）和认输（Resign）�?
页面配置�?`../frontend/config.js`�?
## 相关文档

- [../frontend/README.md](../frontend/README.md) �?前端页面说明
