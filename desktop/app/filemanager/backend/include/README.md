# include/ — 头文件

文件管理器模块的公共接口。

## 文件说明

### filemgr.hpp
模块头文件。实际 C ABI 接口通过 `app_api.hpp` 导入：
- `app_create(config)` — 创建文件管理器实例
- `app_destroy(p)` — 销毁实例
- `app_process(p, json)` — 处理请求（list/read/write/mkdir/remove）
- `app_free_string(s)` — 释放返回字符串
- `app_is_done(p)` — 始终返回 0（文件管理器不自行终止）

后端实现位于 `src/filemgr.cpp`，委托 `VirtualFileSystem` 处理实际文件操作。
