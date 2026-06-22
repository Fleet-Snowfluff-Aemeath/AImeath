# src/ —— 源文件实现

## 文件说明

### board.cpp
`Board` 类实现：
- 构造函数：分配 `size × size` 的 `EMPTY` 网格
- `place()`：检查边界和是否已占用，写入棋子
- `isFull()`：遍历网格检查是否全部非空

### game.cpp
`Game` 类实现：
- 构造函数：初始化棋盘，默认黑棋先手
- `tick(pos)`：解码坐标 → `Board::place()` → 胜局检测 → 切换玩家
- `checkWin(row, col)`：从刚落子的位置向 4 个方向双向扫描，计数同色棋子 ≥ 5 则胜
- `countDir()`：沿某个方向连续同色计数
- C API 包装：`game_new` / `game_free` / `game_tick` / `game_is_over` / `game_get_score`
