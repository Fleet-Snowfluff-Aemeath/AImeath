# src/ —— 源文件实现

`src/*.cpp` 包含 `include/*.hpp` 中声明函数的非模板实现，编译后打包为共享库 `libcore.so`。

## 文件说明

### threadmgr.cpp

**ThreadPool** 的实现。

- 构造函数创建指定数量的工作线程，每个线程循环等待队列任务
- `submit()` 将任务入队，`notify_one()` 唤醒一个空闲线程
- `wait_all()` 在 `m_done_cond` 上等待，直到队列为空且活跃任务数为 0
- `shutdown()` 设置停止标志，唤醒所有线程，等待退出
- 工作线程执行任务前释放锁，允许其他线程并发提交

### timer.cpp

**Timer** 的实现。

- `addTimer()` 创建定时器条目，push 到最小堆后通知调度线程
- `timerLoop()` 是核心调度循环：
  1. 惰性清理堆顶已取消条目
  2. 等待到堆顶到期或被新定时器唤醒
  3. 弹出到期条目，提交回调到 ThreadPool
  4. 周期定时器重新入堆
- `pool.submit()` 异常时，周期定时器保留并重新排期，一次性定时器丢弃

### logger.cpp

**Logger** 的实现。

- `log()` 根据级别过滤决定是否构造活跃的 LogStream（获取锁 + 写入前缀）
- `timestamp()` 缓存 `localtime()`/`strftime()` 结果，每秒最多调用一次
- 静态 mutex 保护 `localtime()` 调用（非线程安全）

### eventmgr.cpp

**EventManager** 的实现。

- `subscribe()` 按优先级降序插入；惰性压缩失效订阅者（阈值 32）
- `unsubscribe()` 标记 inactive，维护失效计数
- `fire()` 锁内复制活跃列表，锁外串行执行回调
- `fireAsync()` 锁内复制活跃列表，锁外通过 ThreadPool 并行提交
- 所有回调执行由 try/catch 包裹，异常不扩散

### netconn.cpp

**NetConn** 的实现。

- `parseUrl()` 解析 HTTP/HTTPS/WS/WSS URL，提取 host/port/path
- `buildHttpRequest()` 构造 HTTP 请求报文
- `connectSocket()` POSIX socket + `getaddrinfo` + 非阻塞 connect + poll 超时
- `doHttp()` 在 ThreadPool 线程中同步执行 HTTP 请求/响应
- `doWsConnect()` WebSocket 升级握手（Sec-WebSocket-Key）
- `doWsReadLoop()` 持续读取 WebSocket 帧，处理文本/二进制/Ping/Pong/Close
- `wsEncodeFrame()` / `wsDecodeFrame()` 客户端帧编码（带 mask）和解码
- 复用 ThreadPool 异步执行、Logger 日志输出
