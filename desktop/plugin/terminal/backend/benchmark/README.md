# benchmark/ — 性能基准测试（Google Benchmark）

## 文件说明

### terminal_bench.cpp

- `BM_CreateDestroy` — 创建/销毁实例性能
- `BM_ExecSyncEcho` — `echo hello` 同步执行性能
- `BM_ExecSyncPwd` — `pwd` 同步执行性能
- `BM_ExecSyncListFiles` — `ls /` 列表性能
- `BM_InvalidAction` — 错误路径性能

## 运行

```bash
cd build
./output/bench/bench_terminal
```
