# benchmark/ — 性能基准测试（Google Benchmark）

## 文件说明

### direction_bench.cpp
- `BM_DirectionIsOpposite` — isOppositeDir 调用开销
- `BM_DirectionPluginlyDir` — applyDir 调用开销

### game_bench.cpp
- `BM_GameVirtualTick` — 虚函数 tick 调用开销

## 运行

```bash
cd build
./output/bench/game_shared_bench
./output/bench/direction_bench
```
