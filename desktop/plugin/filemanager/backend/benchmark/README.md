# benchmark/ — 性能基准测试（Google Benchmark）

## 文件说明

### filemanager_bench.cpp

- `BM_CreateDestroy` — 创建/销毁实例性能
- `BM_ListRoot` — 根目录列表性能
- `BM_ListHome` — `/home` 目录列表性能
- `BM_WriteFile` — 文件写入性能
- `BM_ReadNonexistent` — 读取不存在文件的错误路径性能
- `BM_MkdirRemove` — 创建+删除目录性能

## 运行

```bash
cd build
./output/bench/bench_filemanager
```
