# src/ —— 源文件实现

`src/*.cpp` 包含头文件中声明函数的实现，编译后打包为共享库 `libapp.so`。

## 文件说明

### app_mod.cpp

`AppModuleCache` 的实现。

- `try_load()` —— 多路径 fallback 的 dlopen 策略：
  1. 先用 bare soname（依赖 LD_LIBRARY_PATH/RUNPATH/ld.so.cache）
  2. 失败后通过 `/proc/self/exe` 获取可执行文件路径，依次尝试 `lib/`、`output/lib/` 等相对路径
- `load()` —— 线程安全的缓存加载，首次加载时解析必需的 5 个符号（`app_create`/`app_destroy`/`app_process`/`app_free_string`/`app_is_done`），可选的 3 个异步符号（`app_on_input`/`app_set_output`/`app_set_io_context`）
- `evict()` / `clear()` —— 缓存驱逐和清空

### game_app_adapter.cpp

游戏 C ABI 到统一 `app_*` 接口的适配器（由各 game .so 编译，非 libapp.so 的一部分）。

- 调用 `game_new`/`game_tick`/`game_get_state`（来自 game 模块的 C ABI）
- 包装为 `app_create`/`app_process`/`app_is_done`/`app_free_string` 统一接口
- 处理 JSON 输入输出格式转换
