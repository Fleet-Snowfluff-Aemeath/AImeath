# include/ — 头文件

聊天模块的公共接口定义，编译为共享库后通过 `dlopen` 动态加载。

## 文件说明

### chat_server.hpp

`chatServe` 函数声明：在已建立的 WebSocket 连接上运行聊天会话。

- `chatServe(ws, first_msg)`：接管 WebSocket，`first_msg` 为已从 WebSocket 读到的首条 JSON 消息

实际实现位于 `src/chat_server.cpp`，包含 `ChatApp` 结构体及其 C ABI 包装函数。
