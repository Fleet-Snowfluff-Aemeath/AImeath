# frontend/ — 命令行终端前端

基于 xterm.js 构建的 Web 终端模拟器。

## 文件说明

### index.vue
终端主组件，使用 xterm.js + FitAddon：
- WebSocket 连接后端 PTY 会话
- 连接时自动执行 `bash`
- `onData` 捕获用户输入发送 `stdin`
- 接收 `output` 消息写入终端
- 支持窗口大小自适应（resize 事件）
- 断开/错误连接时显示提示

### config.js
应用配置，导出 `info`（名称/图标）、`styles`、`meta`。

## 后端说明

后端通过 PTY 伪终端管理真实 bash 进程，支持完整的命令行交互。
