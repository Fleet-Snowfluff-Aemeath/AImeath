# 文件管理器后端

通过统一 C ABI (`plugin_api.hpp`) 提供目录列表功能。

## 目录结构

```
backend/
  CMakeLists.txt    # 构建配置
  README.md
  include/
    filemgr.hpp     # 头文件
  src/
    filemgr.cpp     # C ABI 实现
  test/             # Google Test（可选）
  benchmark/        # Google Benchmark（可选）
```

## C ABI

| 函数 | 说明 |
|------|------|
| `plugin_create` | 初始化 |
| `plugin_destroy` | 销毁 |
| `plugin_process` | 处理请求（`list`） |
| `plugin_free_string` | 释放返回字符串 |
| `plugin_is_done` | 返回 0（持续运行） |

## 请求格式

```json
{"action":"list","path":"/home"}
```

## 响应

`list` 返回 `[{"type":"listing","path":"/","entries":[...]}]`，每个 entry 含 name/kind/size/modified/url。

## 路径映射

- `/home` 或 `/` → `desktop/public/home/`
- `/home/xxx` → `desktop/public/home/xxx`
- 目录穿越攻击被阻止
