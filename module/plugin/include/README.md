# include/ —— 头文件声明

头文件（`include/*.hpp`）包含统一 C ABI 声明和插件加载器类声明。

## 文件说明

### plugin_api.hpp

统一 C ABI 接口声明（`extern "C"`），定义所有 plugin 插件必须导出的符号：

- `plugin_create` / `plugin_destroy` —— 生命周期管理
- `plugin_is_done` —— 会话结束判断
- `plugin_on_input` / `plugin_set_output` / `plugin_set_io_context` —— 异步 API（推荐）
- `plugin_process` / `plugin_free_string` —— 同步 API（遗留）
- `plugin_output_fn` —— 输出回调类型

### plugin_mod.hpp

`PluginModule` 结构体和 `PluginModuleCache` 类：

- `PluginModule` —— 封装 `boost::dll::shared_library` 和函数指针，提供 `create()` 工厂方法、`is_async()` 判断
- `PluginModuleCache` —— 线程安全的 dlopen 缓存，`load()` 自动多路径搜索，`evict()`/`clear()` 管理缓存
- `PluginPtr` —— 带自定义 deleter 的 `unique_ptr<void>`，确保 `plugin_destroy` 在析构时被调用
