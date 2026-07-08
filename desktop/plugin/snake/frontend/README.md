# snake/frontend/ —�?贪食蛇前端页�?
## 文件说明

| 文件 | 说明 |
|------|------|
| `index.vue` | 贪食蛇游戏页面，渲染网格、蛇身、食物，响应键盘方向�?WASD |
| `config.js` | 应用配置：名称、样式、布局元信�?|

## 实现

使用 `useGame('snake')` 组合式函数（`frontend/src/composables/useGame.js`）管�?WebSocket 连接和游戏状态。方向键切换方向，棋盘状态实时同步�?
## 相关链接

- [后端 README](../backend/README.md) �?C++ 游戏模块说明
