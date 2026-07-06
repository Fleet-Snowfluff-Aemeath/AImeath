# agent — AI Agent 模块

## 概述

agent 模块实现 AI Agent 功能：通过 LLM（DeepSeek）推理 + 工具调用机制，
让 AI 能够打开、操控和关闭桌面应用程序。

## 架构

```
用户 → Agent 聊天 UI → WebSocket → libagent.so → DeepSeek API (LLM)
                              ↓
                      function_call: open_app / control_app / close_app
                              ↓
                      前端 postMessage → HomePage → 应用窗口操作
```

## C ABI

标准 app C ABI（参见 `app_api.hpp`）：
- `app_create` / `app_destroy` — 生命周期
- `app_on_input` / `app_set_output` — 异步 API
- `app_process` / `app_free_string` — 同步回退
- `app_is_done` — 状态查询

## C++ API

```cpp
#include "agent_api.hpp"
// IAgent 接口供其他模块调用
class IAgent {
    virtual bool openApp(const std::string& name, paramsJson) = 0;
    virtual bool controlApp(const std::string& name, commandJson) = 0;
    virtual bool closeApp(const std::string& name) = 0;
    virtual void stop() = 0;
    // Chat
    virtual bool chatSend(const std::string& text) = 0;
    // FileManager
    virtual bool fileList(const std::string& path) = 0;
    virtual bool fileRead(const std::string& path) = 0;
    virtual bool fileWrite(const std::string& path, const std::string& content) = 0;
    virtual bool fileMkdir(const std::string& path) = 0;
    virtual bool fileRemove(const std::string& path) = 0;
    // Terminal
    virtual bool terminalExec(const std::string& command) = 0;
    virtual bool terminalStdin(const std::string& data) = 0;
};
```

## 工具定义

Agent 通过 DeepSeek function_call 使用以下工具。

> ⚠️ **关键规则**：用户要求执行 shell 命令时，必须使用 **terminal_exec**，严禁先 open_app terminal 再尝试控制。terminal_exec 是执行 shell 命令的唯一方式，直接返回输出。

| 工具 | 参数 | 描述 |
|---|---|---|---|
| open_app | app(string), width(int), height(int) | 打开应用 |
| control_app | app(string), value(int), coord(array) | 操控应用（方向/落子） |
| close_app | app(string), window_id(string) | 关闭应用或指定窗口 |
| get_app_state | app(string), instance(int) | 查询应用状态 |
| list_active_windows | (无) | 列出所有活跃窗口及数量 |
| chat_send | text(string) | 向聊天发送消息 |
| file_list | path(string) | 列出目录内容 |
| file_read | path(string) | 读取文件内容 |
| file_write | path(string), content(string) | 写入文件内容 |
| file_mkdir | path(string) | 创建目录 |
| file_remove | path(string) | 删除文件或目录 |
| terminal_exec | command(string) | 执行终端命令 |
| terminal_stdin | data(string) | 向终端发送输入数据 |

## 通信协议

Agent 通过 WebSocket 输出 agent_action 消息：

```json
{"type":"agent","action":"open_app","app":"snake","params":{"width":20,"height":20}}
{"type":"agent","action":"control_app","app":"snake","command":{"value":3}}
{"type":"agent","action":"close_app","app":"snake"}
```

前端 agent iframe 通过 `postMessage` 将这些消息转发给 HomePage 处理。

## 依赖

- `libcore.so` — iface_mod.hpp, message_queue.hpp
- `libllm.so` — LlmClient (DeepSeek API)
- Boost.Json — JSON 序列化
