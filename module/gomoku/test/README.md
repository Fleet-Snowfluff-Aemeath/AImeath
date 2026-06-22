# test/ —— 单元测试（Google Test）

共 15 个测试用例，覆盖 Board 和 Game 的所有核心逻辑。

## 文件说明

### gomoku_test.cpp

**BoardTest（7 个）**
- `Construction`：棋盘大小、非满盘
- `InBounds`：边界检测（四个角 + 越界）
- `PlaceAndAt`：落子后查询
- `PlaceOccupied`：重复落子被拒绝
- `PlaceOutOfBounds`：越界落子被拒绝
- `IsFull` / `IsFullNotFull`：满盘检测

**GameTest（8 个）**
- `Construction`：初始状态、当前玩家
- `TickValid` / `TickOutOfBounds` / `TickOccupied`：合法/非法落子
- `TurnsAlternate`：黑先白后交替
- `HorizontalWin` / `VerticalWin` / `DiagonalWin` / `AntiDiagonalWin`：四种五子连珠胜局
- `WhiteWins`：白棋获胜
- `DrawFullBoard`：棋盘下满无人获胜（和棋）
- `GameOverStopsMoves`：游戏结束后落子被忽略
- `ScorePreservedAfterGameOver`：结束后分数不变
- `InvalidMoveDoesNotChangePlayer`：非法落子不切换玩家
- `BlockingWin`：防守阻挡后另寻路径获胜

## 运行

```bash
cd build
./output/test/tests
```
