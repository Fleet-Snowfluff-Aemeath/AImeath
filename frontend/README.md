# frontend/ —— Vue 3 游戏前端

基于 Vue 3 + Vite 的 WebSocket 游戏前端，通过浏览器游玩贪食蛇（Snake）、五子棋（Gomoku）、吃豆豆（Pac-Man）和围棋（Go），支持多标签页同时进行。

## 目录结构

```
├── index.html              HTML 入口
├── package.json            依赖配置
├── vite.config.js          Vite 构建配置
└── src/
    ├── main.js             Vue 应用入口（vue-router 设置，路由自动生成）
    ├── App.vue             根组件（<router-view> 布局）
    ├── config/
    │   └── games.js         统一游戏配置（INFO, STYLES, KEY_MAP, STATE_FIELDS）
    ├── views/
    │   ├── HomePage.vue    导航首页（NavCard 列表）
    │   └── GamePage.vue    通用游戏页面（从 config 读取配置，Composition API）
    ├── services/
    │   └── gameSocket.js   WebSocket 客户端（断线自动重连 + 指数退避）
    └── components/
        └── NavCard.vue     可复用导航卡片组件（点击 target="_blank" 打开新标签页）
```

## 路由

| 路径 | 组件 | 说明 |
|------|------|------|
| `/` | `HomePage` | 导航首页，显示所有游戏卡片 |
| `/snake` | `GamePage` (game=snake) | 贪食蛇（方向键控制） |
| `/gomoku` | `GamePage` (game=gomoku) | 五子棋（鼠标点击落子） |
| `/pacman` | `GamePage` (game=pacman) | 吃豆豆（方向键控制） |
| `/go` | `GamePage` (game=go) | 围棋（鼠标点击落子） |
| `/chat` | `ChatPage` | 气泡聊天室，自动追加"喵" |

## 前提条件

**必须在 WSL 中同时运行 C++ WebSocket 游戏服务器（端口 3001）：**

```bash
# 在 WSL 中，先启动游戏服务器
cd /home/huanli/lab
LD_LIBRARY_PATH=build/output/lib ./build/output/gameserver &
```

前端通过 `ws://WSL_IP:3001` 直连游戏服务器（非 Vite proxy）。

## 安装依赖

```bash
cd frontend
npm install --ignore-scripts
```

> WSL 环境下 `\\wsl.localhost\` 路径含 UNC 前缀，需 `--ignore-scripts` 跳过 esbuild 的 postinstall 脚本。首次 `npm run dev` 时会自动下载 esbuild 二进制文件。

## 开发（WSL）

```bash
npm run dev
```

启动后 Vite 输出 `Network: http://192.168.x.x:5173/`，在 Windows 浏览器打开该地址即可。确保端口 3001 的游戏服务器已启动。

如需查看 WSL IP：
```bash
ip addr show eth0 | grep -oP 'inet \K[\d.]+'
```

## 操作说明

| 游戏 | 操作 | 按钮 |
|------|------|------|
| 贪食蛇 | **方向键**（↑↓←→）控制移动 | 开始 / 暂停 / 结束 |
| 五子棋 | **鼠标点击**棋盘格子落子 | 开始新游戏 / 认输 |
| 吃豆豆 | **方向键**（↑↓←→）控制移动 | 开始 / 暂停 / 结束 |
| 围棋 | **鼠标点击**棋盘交叉点落子 | 开始新游戏 / 认输 / 放弃 |
| Chat | **键盘输入**文本 | WebSocket 发送，自动追加"喵"返回 |

## 后端协议

前端通过 WebSocket 发送 JSON 指令，服务器返回游戏状态 JSON：

```json
// 发送
{"action":"new_game","game":"snake","width":20,"height":20}
{"action":"tick","value":3}         // 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT

// 接收
{"type":"snake","w":20,"h":20,"grid":"...","score":0,"over":false}
```

## 构建

```bash
npm run build      # 输出到 dist/
npm run preview    # 预览构建结果
```
