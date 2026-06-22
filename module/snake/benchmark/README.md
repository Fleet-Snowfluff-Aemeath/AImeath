# benchmark/ —— 性能基准测试（Google Benchmark）

6 个基准测试，覆盖 Snake 和 Board 的核心操作。

## 结果（24 核 AMD @ 3.07 GHz，GCC 11.4）

| 测试 | 耗时 | 说明 |
|------|------|------|
| BM_SnakeAdvance | 2.06 ns | 单次前进步（push_front + dir） |
| BM_SnakeCollisionCheck | 1.38 ns | 50 节蛇身自碰撞检测（线性扫描） |
| BM_SnakeHasBodyAt | 0.98 ns | 蛇身位置查询（线性扫描） |
| BM_BoardGenerateFood | 22.7 ns | 50x50 网格随机食物生成 |
| BM_BoardGenerateFoodSmall | 23.3 ns | 5x5 小网格随机食物生成 |
| BM_SnakeSetDirection | 7.54 ns | 四次方向设置 |

## 运行

```bash
cd snake/build
make bench_snake
./output/bench/bench_snake
```
