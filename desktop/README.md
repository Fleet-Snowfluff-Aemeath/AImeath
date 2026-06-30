# desktop/ —— Vue 3 游戏前端

基于 Vue 3 + Vite 的 WebSocket 游戏前端，通过浏览器游玩贪食蛇（Snake）、五子棋（Gomoku）、吃豆豆（Pac-Man）和围棋（Go），支持多标签页同时进行。

## 目录结构

```
frontend/
├── index.html              HTML 入口
├── package.json            依赖配置
├── vite.config.js          Vite 构建配置（@apps → app/）
├── dist/                   构建产物
├── app/                    ← 各应用代码（完全自治）
│   └── <name>/
│       ├── backend/        C++ 游戏模块（共享库 .so）
│       └── frontend/
│           ├── index.vue   应用页面（路由自动挂载）
│           └── config.js   应用配置（name / icon / styles / meta）
└── src/                    ← 共享基础设施
    ├── main.js             Vue 入口（从 APPS 自动生成路由）
    ├── App.vue             根组件
    ├── config/
    │   └── games.js        自动发现 app/*/frontend/，注册路由
    ├── views/
    │   └── HomePage.vue    桌面启动器（从 APPS 动态读取应用列表）
    ├── services/
    │   ├── channel.js      通用 WebSocket Channel（协议无关，断线重连）
    │   └── gameSocket.js   Game 协议层薄封装（基于 channel.js）
    └── composables/
        └── useGame.js      游戏状态管理（基于 gameSocket.js）
```

## 路由

| 路径 | 组件 | 说明 |
|------|------|------|
| `/` | `HomePage` | 桌面启动器，显示所有应用图标 |
| `/<name>` | `app/<name>/frontend/index.vue` | 自动挂载，根据 config.js 加载 |

路由自动由 `games.js` 扫描 `app/*/frontend/` 生成，添加新应用无需修改共享代码。

## 前提条件

**必须在 WSL 中同时运行 C++ WebSocket 游戏服务器（端口 3001）：**

```bash
# 在 WSL 中，先启动游戏服务器（在项目根目录下执行）
cd /path/to/project/root
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

## 添加新应用

只需在 `app/` 下创建目录：

1. `frontend/app/<name>/frontend/config.js` — 应用配置
2. `frontend/app/<name>/frontend/index.vue` — 应用页面
3. `frontend/app/<name>/backend/CMakeLists.txt` — C++ 后端（可选）

无需修改 `src/` 下任何共享代码。
