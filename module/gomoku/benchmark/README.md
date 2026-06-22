# benchmark/ —— 性能基准测试（Google Benchmark）

6 个基准测试，覆盖 Board 和 Game 的核心操作。

## 结果参考

| 测试 | 说明 |
|------|------|
| BM_BoardPlace | 棋盘落子操作 |
| BM_BoardAt | 棋盘位置查询 |
| BM_BoardIsFull | 满盘检测 |
| BM_GameTick | 游戏 tick（轮流落子） |
| BM_WinDetectionHorizontal | 水平五子连珠胜局检测 |
| BM_WinDetectionDiagonal | 对角线五子连珠胜局检测 |

## 运行

```bash
cd build
./output/bench/bench_gomoku
```
