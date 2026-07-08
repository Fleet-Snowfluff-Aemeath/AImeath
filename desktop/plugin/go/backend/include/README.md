# include/ — 头文件

围棋模块的公共接口定义。

## 文件说明

### board.hpp
`Board` 类定义，19×19 围棋棋盘：
- 内部使用 21×21 网格（含边界哨兵），简化边界判断
- `place(row, col, stone, capt_out)` — 落子含提子逻辑
- `isLegal(row, col, stone)` — 合法性检查（禁自杀/打劫）
- `countScore(komi)` — 中国规则数子
- `markDead(row, col)` / `removeDeadStones()` / `clearDeadMarks()` — 死子标记
- `hash()` — FNV-1a 哈希用于打劫/超打劫检测
- `renderGrid()` — ASCII 棋盘输出
- `Stone` 枚举：EMPTY=0, BLACK=1, WHITE=2

### game.hpp
`GoGame` 类继承自 `Game` 基类：
- 虚着计数 + 双虚着终局
- 提子统计 (capsB/capsW)
- 贴目 (komi=3.75)
- 死子标记阶段
- 状态序列化 (getState)
- 常量：PASS=-1, RESIGN=-2, CLEAR_DEAD=-3, CONFIRM_DEAD=-4
