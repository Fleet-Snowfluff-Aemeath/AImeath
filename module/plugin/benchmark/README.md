# Plugin 模块基准测试

## 基准测试

| 基准测试 | 测量内容 |
|----------|----------|
| `BM_PluginModuleLoad` | `PluginModuleCache::load("mock_plugin")` — dlopen + 符号查找 + 缓存 |
| `BM_PluginCreate` | `PluginModule::create(config)` — plugin_create + PluginPtr 包装 |
| `BM_PluginProcess` | `plugin_process()` 往返调用 + plugin_free_string |
| `BM_PluginCacheHit` | `cache.load("mock_plugin")` 缓存命中（无 dlopen） |

## 运行

```bash
cd plugin/build
make bench_plugin
./output/bench/bench_plugin --benchmark_min_time=0.1
```
