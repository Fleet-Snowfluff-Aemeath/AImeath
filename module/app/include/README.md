# include/ —— 头文件声明

头文件（`include/*.hpp`）包含统一 C ABI 声明和插件加载器类声明。

## 文件说明

### app_api.hpp

统一 C ABI 接口声明（`extern "C"`），定义所有 app 插件必须导出的符号：

- `app_create` / `app_destroy` —— 生命周期管理
- `app_is_done` —— 会话结束判断
- `app_on_input` / `app_set_output` / `app_set_io_context` —— 异步 API（推荐）
- `app_process` / `app_free_string` —— 同步 API（遗留）
- `app_output_fn` —— 输出回调类型

### app_mod.hpp

`AppModule` 结构体和 `AppModuleCache` 类：

- `AppModule` —— 封装 `boost::dll::shared_library` 和函数指针，提供 `create()` 工厂方法、`is_async()` 判断
- `AppModuleCache` —— 线程安全的 dlopen 缓存，`load()` 自动多路径搜索，`evict()`/`clear()` 管理缓存
- `AppPtr` —— 带自定义 deleter 的 `unique_ptr<void>`，确保 `app_destroy` 在析构时被调用
