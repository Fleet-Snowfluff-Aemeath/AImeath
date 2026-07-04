# chat — 聊天应用

AI 聊天应用，集成 DeepSeek LLM，支持工具调用和 Agent 功能。

## 目录结构

```
chat/
├── frontend/
│   ├── config.js          # 应用配置（名称、图标、路由）
│   ├── index.vue          # 聊天 UI 组件（Vue 3）
│   ├── __tests__/
│   │   ├── chat.test.js   # 功能测试（Vitest）
│   │   └── chat.bench.js  # 性能测试（Vitest Bench）
│   └── README.md
├── backend/
│   ├── include/
│   │   ├── chat_server.hpp # 头文件
│   │   └── README.md
│   ├── src/
│   │   ├── chat_server.cpp # 后端实现（ChatApp、LLM、工具调用）
│   │   └── README.md
│   ├── test/
│   │   ├── chat_test.cpp   # 单元测试（Google Test）
│   │   └── README.md
│   ├── benchmark/
│   │   ├── chat_bench.cpp  # 性能测试（Google Benchmark）
│   │   └── README.md
│   └── CMakeLists.txt
└── README.md
```

## 功能

- 与 DeepSeek LLM 流式对话
- 内置命令：`/图片`、`/视频`、`/音乐`、`/游戏`
- Agent 工具调用：打开/控制/关闭应用、查询状态、读写文件、执行终端命令
- 多实例支持：可同时打开多个聊天窗口独立对话
