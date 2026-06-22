# include/ —— 头文件

五子棋模块的公共接口定义，编译为共享库后通过 `dlopen` 动态加载。

## 文件说明

### board.hpp
`Board` 类定义：
- 使用 `std::vector<Cell>` 一维数组模拟 N×N 棋盘
- `Cell` 枚举：`EMPTY` / `BLACK` / `WHITE`
- 核心操作：`place()` 落子、`at()` 查询、`isFull()` 满盘检测

### game.hpp
`Game` 类定义 + `extern "C"` C API：
- 轮流落子，黑先白后
- `tick(pos)`：解码 `pos = row * size + col`，落子并触发胜局检测
- 四方向五子连珠检测（水平 / 垂直 / 对角线 \ / 反对角线 /）
- C API：`game_new` / `game_free` / `game_tick` / `game_is_over` / `game_get_score`
