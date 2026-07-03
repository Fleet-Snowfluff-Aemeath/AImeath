# src/ — 前端框架共享代码

基于 Vue 3 构建的桌面环境框架，包含游戏状态管理、WebSocket 通信、配置解析等模块。

## 结构

```
src/
├── composables/useGame.js    # 核心游戏 composable
├── services/
│   ├── channel.js            # WebSocket 通道（含重连）
│   ├── gameSocket.js         # 游戏协议包装
│   └── config.js             # 后端端口发现
├── config/games.js           # 应用自动发现 + 常量
├── views/
│   ├── HomePage.vue          # 桌面窗口管理器
│   └── FileViewer.vue        # 通用文件预览
├── types/protocol.ts         # TypeScript 协议定义
└── __tests__/                # 框架代码测试
```

## 模块说明

### composables/useGame.js
游戏状态管理的核心 composable，提供：
- 响应式游戏状态（棋盘、分数、回合）
- 键盘/点击事件处理
- 方向控制、pass/resign 等 Go 特有操作
- 自动启停游戏连接

### services/channel.js
协议无关的 WebSocket 通道，支持：
- 自动重连（指数退避）
- 类型化消息路由
- 连接生命周期回调

### services/gameSocket.js
基于 channel.js 的游戏协议包装，自动发送 `new_game`，处理 `window_closing`。

### services/config.js
后端端口发现（通过 `/api/config` 接口），含重试逻辑。

### config/games.js
通过 Vite `import.meta.glob` 自动发现 `app/*/frontend/config.js`，构建 APPS/INFO/STYLES/META 导出表。定义方向键映射、Go 操作常量等。
