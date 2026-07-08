# frontend/ — 文件管理器前端

基于 Vue 3 构建的文件管理器界面。

## 文件说明

### index.vue
文件管理器主组件，提供：
- **目录浏览**：网格显示文件/文件夹，支持导航、后退/前进、向上一级
- **面包屑路径**：显示当前路径，点击可跳转到任意层级
- **搜索过滤**：实时过滤当前目录下的条目
- **文件打开**：双击文件调用 `openFile` postMessage 打开 FileViewer
- **连接状态**：底部状态栏显示 WebSocket 连接状态和条目数量

### config.js
应用配置，导出 `info`（名称、图标）、`styles`、`meta`。

### fileSystem.js
文件系统工具函数：
- `getIcon(item)` — 根据文件扩展名返回对应图标
- `getMimeCategory(name)` — 判断文件类型（text/image/video/audio/markdown/pdf/binary）
- `formatSize(bytes)` — 格式化文件大小
- `formatDate(dateStr)` — 格式化日期

### filemgrChannel.js
WebSocket 通道封装，基于 `channel.js`：
- `createFilemgrChannel()` — 创建通道实例
- `listDir(path)` — 请求目录列表
- 支持 `window_closing` postMessage 发送 `close_window`
