# Plugin 模块单元测试

## 测试文件

| 测试文件 | 测试内容 |
|----------|----------|
| `plugin_test.cpp` | `PluginModule` 默认构造、`PluginPtr` 生命周期、`PluginModuleCache::load()` mock.so 加载/缓存/驱逐 |
| `mock_plugin.cpp` | 实现 `plugin_create`/`plugin_process`/`plugin_is_done` 的 mock .so，用于集成测试 |

## 运行

```bash
cd plugin/build
make tests
./output/test/tests
```

## 测试策略

- **PluginModule 测试**：通过头文件构造已足够，无需动态加载
- **PluginPtr 测试**：验证 unique_ptr + 自定义 deleter 生命周期
- **PluginModuleCache 测试**：构建 mock_plugin.so(mock_plugin.cpp) → dlopen → 验证 `load()`/`evict()`/`clear()` 流程
- **不存在模块测试**：验证 `load("nonexistent")` 返回 false 而非崩溃
