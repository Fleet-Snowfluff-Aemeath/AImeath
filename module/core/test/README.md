# test/ —— 单元测试（Google Test）

共 88 个测试用例，覆盖全部 6 个组件。

## 文件说明

### threadpool_test.cpp（12 个测试）
- 基本 submit 和 wait_all
- 活跃任务数追踪
- 关闭行为
- 密集型任务执行
- 多线程并发提交
- 异常安全：任务抛出异常被线程池捕获，不影响其他任务

### timer_test.cpp（18 个测试）
- setTimeout 精确触发一次
- setTimeoutAt 绝对时间点触发一次
- setTimeoutAt 过去时间点立即触发
- setTimeoutAt 可取消
- setTimeoutAt 与 setTimeout 混合顺序正确
- setInterval 重复触发
- cancel 阻止回调执行
- exists 反映定时器存活状态
- 多定时器调度与顺序
- 析构函数等待定时器循环退出
- 从回调内部并发取消
- 异常安全：周期定时器回调抛出异常后继续重复触发
- 异常安全：回调提交到线程池异常被隔离，不导致 std::terminate

### logger_test.cpp（13 个测试）
- 各级别单条消息
- 级别过滤（DEBUG < INFO < WARN < ERROR）
- 多条消息序列
- 长消息处理
- 多线程并发日志
- LogStream 移动语义
- 操纵符支持（std::endl）
- 默认构造输出到 std::cerr
- 文件路径构造自动写入文件

### eventmgr_test.cpp（18 个测试）
- 订阅与派发（单/多订阅者）
- unsubscribe 标记失效
- fireAsync 通过 ThreadPool 异步派发
- 优先级排序（高优先者先执行）
- subscriberCount 准确性
- 失效订阅者压缩
- 回调内重入 subscribe/unsubscribe
- 全部取消后无内存泄漏
- 异常安全：单个回调异常不影响其他订阅者接收事件

### netconn_test.cpp（15 个测试）
- `ParseHttpUrl` / `ParseHttpUrlDefaultPath` / `ParseHttpUrlCustomPort`：HTTP URL 解析
- `ParseWsUrl` / `ParseHttpsUrl` / `ParseInvalidUrl`：WS/HTTPS/无效 URL
- `BuildHttpGetRequest` / `BuildHttpPostRequest`：HTTP 请求构造
- `Base64Encode` / `Base64EncodePadding` / `Base64EncodeDoublePadding` / `Base64EncodeEmpty`：Base64 编解码
- `BuildWsKeyLength`：WebSocket 握手 key 生成
- `WsFrameRoundTrip` / `WsFrameEmptyPayload` / `WsFrameLargePayload` / `WsFrameTruncated`：WebSocket 帧编码/解码
- `InitialState` / `SetConnectTimeout`：状态管理
- `ConstructWithLogger`：Logger 复用

### ws_server_test.cpp（10 个测试）
- 常量验证：`DEFAULT_PORT`、`DEFAULT_IO_THREADS`、`DEFAULT_FALLBACK_THREADS`
- 命名空间验证：`key::APP`/`key::TEXT`/`key::GAME`、`appname::CHAT`/`appname::SNAKE`
- `Listener` 构造和 shutdown（默认端口和自定义端口）
- `Session` 构造（有/无 fallback_pool）

## 运行

```bash
cd build
make tests
./tests                 # 全部测试
./tests --gtest_filter=TimerTest.*  # 指定测试套件
```
