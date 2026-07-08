# src/ — 源文件实现

围棋后端的完整实现，编译为共享库 `libgo.so`。

## 文件说明

### board.cpp
`Board` 类实现（305 行）：
- 哨兵边界棋盘（21×21 内部网格）
- `place()` — 落子 + 提子（移除无气敌方群组）+ 自杀预防
- `isLegal()` — 在副本上模拟验证合法性
- `hash()` — FNV-1a 哈希用于打劫检测
- `countTerritory()` / `countScore()` — 洪泛填充分类
- `markDead()` / `removeDeadStones()` / `clearDeadMarks()` — 死子标记管理
- `renderGrid()` — ASCII 棋盘输出

### game.cpp
`GoGame` 类实现（125 行）：
- `tick(action)` — 核心动作处理：落子/提子/虚着/认输/死子标记
- `score()` — 中国规则计分（含贴目）
- `getState()` — JSON 状态序列化
- 打劫检测（通过 hash 缓存）
- 通过 `APP_GAME_API_COMMON()` 注册 C ABI
