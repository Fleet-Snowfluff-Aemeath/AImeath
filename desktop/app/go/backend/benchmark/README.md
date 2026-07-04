# benchmark/ — 性能基准测试（Google Benchmark）

## 文件说明

### go_bench.cpp

- `BM_BoardPlace` — 棋盘落子性能（每次重建棋盘，20 手）
- `BM_BoardHash` — 棋盘 FNV-1a 哈希计算
- `BM_GameTick` — GoGame 落子流程性能（每次重建游戏，20 手）
- `BM_CaptureStones` — 提子操作性能
- `BM_Score` — 中国规则计分性能

## 运行

```bash
cd build
./output/bench/bench_go
```
