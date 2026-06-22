# benchmark/ —— 性能基准测试（Google Benchmark）

6 个基准测试，覆盖 Board 和 Game 的核心操作。

## 结果参考

| 测试 | 说明 |
|------|------|
| BM_BoardGenerateBeans | 20×20 棋盘随机生成 30 颗豆 |
| BM_BoardHasBean | 豆子位置查询 |
| BM_BoardRemoveBean | 移除豆子操作 |
| BM_GameTick | 游戏 tick（方向切换） |
| BM_GameTickUntilGameOver | 完整游戏生命周期（直到撞墙） |
| BM_BoardIsBeanConsistency | 100×100 棋盘遍历查询 |

## 运行

```bash
cd build
./output/bench/bench_pacman
```
