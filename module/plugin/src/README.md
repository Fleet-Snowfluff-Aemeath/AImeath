# src/ —— 源文件实现

`src/*.cpp` 包含头文件中声明函数的实现，编译后打包为共享库 `libplugin.so`。

## 文件说明

### plugin_mod.cpp

`PluginModuleCache` 的实现。

- `try_load()` —— 多路径 fallback 的 dlopen 策略：
  1. 先用 bare soname（依赖 LD_LIBRARY_PATH/RUNPATH/ld.so.cache）
  2. 失败后通过 `/proc/self/exe` 获取可执行文件路径，依次尝试 `lib/`、`output/lib/` 等相对路径
- `load()` —— 线程安全的缓存加载，首次加载时解析必需的 5 个符号（`plugin_create`/`plugin_destroy`/`plugin_process`/`plugin_free_string`/`plugin_is_done`），可选的 3 个异步符号（`plugin_on_input`/`plugin_set_output`/`plugin_set_io_context`）
- `evict()` / `clear()` —— 缓存驱逐和清空

### game_plugin_adapter.cpp

游戏 C ABI 到统一 `plugin_*` 接口的适配器（由各 game .so 编译，非 libplugin.so 的一部分）。

- 调用 `game_new`/`game_tick`/`game_get_state`（来自 game 模块的 C ABI）
- 包装为 `plugin_create`/`plugin_process`/`plugin_is_done`/`plugin_free_string` 统一接口
- 处理 JSON 输入输出格式转换
