# test/ —— 单元测试（Google Test）

共 17 个测试用例，覆盖 Board 和 Game 的所有核心逻辑。

## 文件说明

### pacman_test.cpp

**BoardTest（6 个）**
- `Construction`：棋盘大小、初始无豆
- `GenerateBeans`：随机生成指定数量
- `GenerateBeansAvoidsPosition`：避开指定位置
- `RemoveBean` / `RemoveNonExistentBean`：吃豆计数
- `GenerateBeansClampedToMax`：生成数超过容量时截断

**GameTest（11 个）**
- `Construction`：初始状态、有豆
- `TickMovesPlayer`：移动后分数不变或增加
- `WallCollisionUp/Down/Left/Right`：四个方向撞墙结束
- `ReverseDirectionAllowed`：反向移动允许
- `EatBeanIncreasesScore`：吃豆加分
- `TickAfterGameOverIgnored`：结束后 tick 忽略
- `ScoreIncrementsByTen`：每颗豆 +10 分
- `MultipleTicksSequential`：多次移动后撞墙

## 运行

```bash
cd build
./output/test/tests
```
