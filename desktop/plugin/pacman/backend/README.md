# pacman/ —�?吃豆豆游�?
C++17 实现的吃豆豆游戏，编译为共享�?`libpacman.so`，通过 `dlopen` 动态加载�?
## 目录结构

```
frontend/plugin/pacman/
├── backend/
�?  ├── include/          头文�?�?  �?  ├── board.hpp     棋盘网格（豆子生成、查询、移除）
�?  �?  └── game.hpp      Game 类、Direction 枚举、C API
�?  ├── src/              源文件实�?�?  �?  ├── board.cpp
�?  �?  └── game.cpp
�?  ├── test/             单元测试（Google Test�?�?  �?  └── pacman_test.cpp
�?  ├── benchmark/        性能基准测试（Google Benchmark�?�?  �?  └── pacman_bench.cpp
�?  ├── test_build.sh     快速构建脚�?�?  └── CMakeLists.txt    构建配置
├── frontend/
�?  ├── index.vue      Vue 页面组件
�?  └── config.js         应用配置
└── README.md             （本文档�?```

## 规则

- 玩家�?N×M 网格中移动，方向键控�?- 豆子随机散布在网格上，经过即吃掉，每�?+10 �?- 撞墙即游戏结�?- 吃完所有豆子即为胜�?
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
cd frontend/plugin/pacman/backend
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
./output/bench/bench_pacman
```

## 设计要点

- 棋盘使用 `std::vector<bool>` 标记豆子，O(1) 查询/移除
- 方向变更缓冲（`m_next_dir`）当�?tick 内首次按下后忽略后续输入（复�?snake 模式�?- `getState()` 使用 `boost::json::object` + `boost::json::serialize()` 生成合法 JSON 状�?- `extern "C"` API �?snake/gomoku 模块一致，支持 `dlopen` / `dlsym` 动态加�?- 豆子生成数量自动截断不超过网格容�?- 游戏结束后所�?tick 被忽略，分数保持不变

## 前端页面

Vue 页面组件位于 `../frontend/index.vue`，通过 WebSocket 连接后端游戏服务器，使用 `useGame('pacman')` 组合式函数管理游戏状态。方向键控制吃豆人移动�?
页面配置�?`../frontend/config.js`�?
## 相关文档

- [include/README.md](include/README.md) �?头文件说�?- [src/README.md](src/README.md) �?源文件实�?- [test/README.md](test/README.md) �?单元测试
- [benchmark/README.md](benchmark/README.md) �?性能基准测试
- [../frontend/README.md](../frontend/README.md) �?前端页面说明
