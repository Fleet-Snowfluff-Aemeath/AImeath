# access — 网络接入模块

## 概述

接入模块负责所有网络连接的管理：WebSocket 服务端、HTTP/WS 客户端、PTY 伪终端会话。

## 组件

| 组件 | 文件 | 说明 |
|------|------|------|
| **ws_server** | `ws_server.hpp/cpp` | WebSocket 服务端：Listener（accept 循环）、Session（连接管理+消息路由）、SessionManager（会话注册表）、PluginStateNotifier（状态通知） |
| **netconn** | `netconn.hpp/cpp` | HTTP/WebSocket 客户端：HttpClient（GET/POST）、WsClient（WebSocket 连接） |
| **pty_session** | `pty_session.hpp/cpp` | PTY 伪终端会话管理 |

## 架构

```
module/access/
├── include/          # 公共头文件
│   ├── ws_server.hpp
│   ├── netconn.hpp
│   └── pty_session.hpp
├── src/              # 实现代码
├── test/             # GTest 单元测试
├── benchmark/        # Google Benchmark 基准测试
├── CMakeLists.txt
├── test_build.sh
└── README.md
```

## 依赖

- **core** — config, logger, threadmgr, iface_mod, toolbox, timer
- **Boost** — json, url, asio, beast
- **OpenSSL** — SSL/TLS

## 构建

```bash
# 作为子项目构建（从根目录）
cmake .. && make -j$(nproc)

# 独立构建
bash test_build.sh
```
