# 命令行后端

C++ Shell 命令透传后端，通过 `plugin_api.hpp` C ABI 导出。

## 接口

- `plugin_create` — 创建实例
- `plugin_destroy` — 销毁实例
- `plugin_process` — 处理 `{ action: "exec", cmd: "..." }`，返回执行结果
- `plugin_free_string` — 释放返回字符串
- `plugin_is_done` — 检查是否结束（永不结束）

## 构建

```bash
cd desktop/plugin/terminal/backend
mkdir build && cd build
cmake ..
make
```

输出 `libterminal.so` 到 `build/output/lib/`。
