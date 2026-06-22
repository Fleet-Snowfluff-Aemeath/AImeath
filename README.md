# Lab —— 模块化 C++ 游戏实验室

C++17 多模块项目，包含 4 个共享库（core/snake/gomoku/pacman）、一个 C++ WebSocket 游戏服务器和一个 Vue 3 游戏前端。
所有游戏模块通过 `extern "C"` 6 函数 API 暴露，由 `main.cpp` 和游戏服务器经 `GameModule` 加载器动态加载。

## 目录结构

```
├── CMakeLists.txt        根构建文件
├── build.sh              一键构建脚本
├── main.cpp              Demo 入口（链接 libcore，dlopen 加载游戏模块）
├── start.sh              一键启动脚本（游戏服务器 + 前端）
├── backend/
│   └── main.cpp          游戏服务器
├── frontend/             Vue 3 + Vite 游戏前端
├── module/
│   ├── module.md         模块开发规范（新增模块请遵循）
│   ├── game/             游戏基础设施（Game基类、Direction、GameModule加载器）
│   ├── core/             并发 & 网络工具库
│   │   ├── wsutil.hpp/cpp    URL 解析 + JSON 工具函数
│   ├── snake/            终端贪食蛇游戏
│   ├── gomoku/           终端五子棋游戏
│   ├── pacman/           终端吃豆豆游戏
│   └── go/               围棋游戏
└── README.md             本文档
```

## 模块

| 模块 | 库文件 | 说明 |
|------|--------|------|
| `game` | `libgame.so` | Game 抽象基类、Direction 工具、GameModule 加载器（dlopen 封装） |
| `core` | `libcore.so` | ThreadPool, Timer, Logger, EventManager, NetConn（HTTP/WebSocket）, wsutil（URL 解析 + JSON 序列化） |
| `snake` | `libsnake.so` | 贪食蛇，终端渲染，方向键控制 |
| `gomoku` | `libgomoku.so` | 五子棋，15×15 棋盘 |
| `pacman` | `libpacman.so` | 吃豆豆，方向键控制，吃豆得分 |
| `go` | `libgo.so` | 围棋，19×19 棋盘，数子法 3.75 子贴目 |

每个模块遵循统一规范：`include/`、`src/`、`test/`、`benchmark/` 各配中文 README，`extern "C"` 导出 `game_new`、`game_free`、`game_tick`、`game_is_over`、`game_get_score`、`game_get_state`。`getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON。

## 依赖

- C++17 编译器（GCC 11+）
- CMake ≥ 3.12
- Boost（url, json, filesystem）— core 模块直接链接，游戏模块链接 `Boost::json`
- OpenSSL — core 模块链接
- Google Test — 独立构建测试时
- Google Benchmark — 独立构建基准测试时
- Node.js ≥ 18 — 前端（推荐 22+）

## 构建与运行

```bash
# 一键构建 C++
./build.sh
# 或手动：
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# 运行 demo（链接 libcore.so，dlopen 加载游戏模块）
LD_LIBRARY_PATH=build/output/lib ./build/demo

# Demo 输出顺序：NetConn → Pac-Man → Gomoku → Snake
```

### 独立构建单个模块

```bash
cd module/core     # 或 snake / gomoku / pacman
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make tests                    # 编译单元测试
./output/test/tests           # 运行测试
make bench                    # 编译基准测试
./output/bench/bench_timer    # 运行基准测试
```

### 游戏服务器

```bash
# 构建（已包含在 make 中）
cd build && make -j$(nproc)

# 运行（监听 0.0.0.0:3001）
LD_LIBRARY_PATH=output/lib ./output/gameserver
```

服务器链接 `libcore.so` 使用 `wsutil`（URL 解析 + JSON 工具）和 `GameModule`（缓存 `dlopen` 加载器），从 `LD_LIBRARY_PATH` 加载 `libsnake.so`、`libgomoku.so`、`libpacman.so`。

### 前端

```bash
cd frontend
npm install --ignore-scripts   # WSL 下需跳过 esbuild postinstall
npm run dev                    # 启动 Vite 开发服务器
# 访问 http://localhost:5173/ 或 WSL IP:5173
# 需要游戏服务器（端口 3001）同时运行
```

详见 `frontend/README.md`。

## Demo 运行结果

```
=== NetConn (Core) ===
URL parse: host=example.com port=8080 path=/path query=q=1 is_ws=0
NetConn: all utility functions verified OK
=== Pac-Man ===
Final Score: 20
=== Gomoku ===
Game Over! Black wins!
=== Snake ===
Final Score: 0
=== Go ===
Final Score (1=B win, 2=W win): 2
```

## 一键启动

```bash
./start.sh
# 自动构建 C++、启动游戏服务器（:3001）和前端（:5173）
# Ctrl+C 优雅终止所有服务
```

## 设计要点

- **统一加载模式**：所有游戏模块导出相同 `extern "C"` 6 函数签名，`GameModule` 封装 `dlopen` + `boost::dll` 缓存加载
- **代码复用**：`wsutil` 抽取 URL 解析和 JSON 工具函数到 core，`backend/main.cpp` 从 296 行精简至 116 行
- **Core 直接链接**：`main.cpp` 和 `gameserver` 直接链接 `libcore.so`（不通过 dlopen），仅游戏模块动态加载
- **游戏状态输出**：`getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON，无需手工拼接和转义
- **NetConn**：基于 Boost.Asio + ThreadPool 的异步 HTTP/WebSocket 客户端，含自动重连
- **游戏服务器**：C++ 原生 WebSocket 服务器，thread-per-connection 模型
- **前端**：Vue 3 + vue-router 多页面应用，Composition API + `<script setup>`，WebSocket 直连服务器（无 Vite 代理），CSS Grid 渲染游戏网格，游戏配置集中至 `src/config/games.js`，路由自动生成
- **WSL 兼容**：`vite.config.js` 设 `host: '0.0.0.0'` 及 `cacheDir: 'node_modules/.vite'` 规避 UNC 路径问题
