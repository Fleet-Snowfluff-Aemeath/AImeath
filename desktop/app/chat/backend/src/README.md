# src/ — 源文件实现

## 文件说明

### chat_server.cpp

聊天后端的完整实现，包含：

- **ChatApp 结构体**：会话状态管理（对话历史、消息队列、流状态、LLM 客户端引用等）
- **命令处理**：`handleCommand()` 处理 `/图片`、`/视频`、`/音乐`、`/游戏`、`/终端` 等内置命令，返回 embed 类型的输出
- **LLM 调用**：`doLlmCall()` 对接 DeepSeek API，支持流式输出（stream_start/delta/stream_end）和工具调用
- **工具调用处理**：`processToolCalls()` 处理 LLM 返回的 `open_app`、`control_app`、`close_app`、`get_app_state`、`list_apps` 等函数调用
- **状态注入**：`handleUserMessageAsync()` 在每个对话轮次中自动注入当前已打开 app 的状态到 LLM 上下文
- **C ABI**：`app_create`、`app_destroy`、`app_set_output`、`app_on_input`、`app_process` 等导出函数，供 `ws_server` 通过模块接口调用
