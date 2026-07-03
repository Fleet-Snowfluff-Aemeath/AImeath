# test/ — 集成压力测试

基于 Node.js WebSocket (`ws`) 的聊天后端集成测试套件。

## 测试说明

| 文件 | 用途 | 默认参数 |
|---|---|---|
| `chat_burst.js` | 单连接 burst 发送：一次打开连接连续发送 N 条消息，验证所有回复 | `--rounds=10` |
| `chat_concurrent.js` | 多连接并发：N 个连接各自 burst 发送 M 条，验证互不干扰 | `--connections=5 --rounds=5` |
| `chat_stress.js` | 单连接持续压力：每收到一次回复再发下一条，持续 N 轮 | `--rounds=10 --delay-ms=50` |
| `chat_command.js` | 测试聊天命令：/图片, /音乐, /视频, /游戏 | `--port=3001` |
| `ws_smoke.js` | WebSocket 连接冒烟测试：连接→发送→接收→关闭 | `--port=3001` |

### 用法

```bash
# 单个测试
node test/chat_burst.js --rounds=10 --port=3001
node test/chat_concurrent.js --connections=5 --rounds=5
node test/chat_stress.js --rounds=10 --delay-ms=50

# 一次运行全部测试
bash test/run_all.sh
```

### 前置条件

1. 先构建 C++ 工程：
   ```bash
   bash build.sh
   ```
2. 安装 Node 依赖：
   ```bash
   cd test && npm install && cd ..
   ```

## 脚本说明

| 脚本 | 用途 |
|---|---|
| `quick_stress.sh` | 启动服务器 → 运行 5 轮 stress → 停止 |
| `run_all.sh` | 启动服务器 → 顺序运行 burst/concurrent/stress → 打印汇总 |
| `run_stress_test.sh` | 启动服务器 → 100 轮 5ms delay 高强度 stress → 停止 |
| `start_server.sh` | 仅启动服务器并打印 PID |

## 注意

- 所有测试需先运行 `build.sh` 编译 `AImeath`
- `config.json` 中的 API key 和 token 为敏感信息，不可提交公开仓库
