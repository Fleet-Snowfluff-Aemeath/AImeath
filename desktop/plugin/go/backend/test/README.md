# test/ — 单元测试（Google Test）

共 15 个测试用例，覆盖围棋引擎核心逻辑。

## 文件说明

### go_test.cpp

**基础（2 个）**
- `StoneOpponent` — opponent() 函数正确性
- `BoardEmpty` — 新棋盘全为空

**落子/提子（3 个）**
- `PlaceStone` — 单子落子
- `PlaceOccupied` — 禁止重复落子
- `CaptureSingleStone` — 单子提子（4 口气全填）
- `CaptureGroup` — 群组提子
- `SuicidePrevented` — 禁自杀

**游戏流程（2 个）**
- `PassAndEnd` — 双虚着终局 + 标记阶段
- `Resign` — 认输结束

**状态/操作（2 个）**
- `GetState` — JSON 状态含 "go" 和 "grid"
- `GameTickInvalid` — 非法落子不切换回合

**死子/计分（3 个）**
- `MarkDeadStone` — 死子标记生命周期
- `SekiCountsBothStones` — 共活双方算活
- `ChineseScoringKomi` — 贴目计分

**C API（1 个）**
- `CApi` — 完整 C API 测试

## 运行

```bash
cd build
./output/test/tests
```
