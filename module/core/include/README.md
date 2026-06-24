# include/ —— 头文件声明

头文件（`include/*.hpp`）包含类声明和少量必须的内联代码。
非模板实现位于 `src/*.cpp`，编译为共享库 `libcore.so` 后链接使用。

使用方式：`#include "timer.hpp"`（需通过 CMake 将 `include/` 加入头文件搜索路径）。

## 文件说明

### threadmgr.hpp

**class ThreadPool** —— 固定大小的线程池，支持任务队列提交。

```cpp
ThreadPool pool(4);
pool.submit([]() { /* 任务 */ });
pool.wait_all();                    // 等待已提交任务完成
size_t n = pool.active_count();    // 当前正在执行的任务数
pool.shutdown();                    // 停止所有工作线程（析构函数自动调用）
```

设计要点：
- 工作线程在 `m_cond` 上等待新任务；`notify_one()` 避免惊群
- `m_done_cond` 与 `m_cond` 分离，`wait_all()` 只等待任务完成，不干扰空闲工作线程
- 异常安全：每个任务由 try/catch 包裹，异常写入 `std::cerr`

---

### timer.hpp

**class Timer** —— 基于最小堆的定时器，回调通过 ThreadPool 异步执行。

```cpp
ThreadPool pool(2);
Timer timer(pool);

auto id = timer.setTimeout(boost::chrono::milliseconds(100), []() { /* 一次性 */ });
auto id = timer.setTimeoutAt(now + ms(100), []() { /* 绝对时间点触发 */ });
auto id = timer.setInterval(boost::chrono::milliseconds(50), []() { /* 周期执行 */ });

timer.cancel(id);
bool alive = timer.exists(id);
```

设计要点：
- 专用定时器线程睡眠至最近到期时间，仅负责任务调度
- 到期定时器从最小堆弹出，提交到 ThreadPool 执行，永不阻塞定时器循环
- `m_alive`（`unordered_set`）提供 O(1) 的 `exists()` 和 `cancel()`
- `m_cancelled` 集合 + 堆顶惰性清理处理取消，无需从堆中实际删除
- `wait(lock, predicate)` 防止清理堆顶与析构函数并发时的丢失唤醒
- 异常安全：`pool.submit()` 由 try/catch 包裹，提交失败时周期定时器保留并重新排期

---

### logger.hpp

**class Logger** —— 线程安全、基于 RAII 的分级日志器。

```cpp
Logger log(Logger::INFO);              // 默认输出到 std::cerr
log.info() << "hello " << 42;
Logger file_log("/var/log/app.log");   // 输出到文件
file_log.warn() << "warning";
Logger custom(std::cout, Logger::DEBUG); // 指定流
custom.debug() << "这条会输出";         // DEBUG 级别可见
```

设计要点：
- `LogStream`（RAII）构造时获取 `m_mtx` 并写入前缀，析构时追加 `'\n'` 并释放锁
- 时间戳每秒缓存一次，避免每次日志调用 `localtime()`/`strftime()`，吞吐量提升 6-9 倍
- 被过滤的日志（级别低于阈值）跳过前缀和锁，开销极小
- 支持三种构造：默认 `std::cerr`、文件路径、自定义 `std::ostream`
- 静态 mutex 保护共享的 `localtime()` 调用

---

### eventmgr.hpp

**class EventManager** —— 发布-订阅事件派发器，支持同步和异步派发。

```cpp
EventManager mgr;
auto h = mgr.subscribe(EVT_SYS_STARTUP, [](const Event& e) { /* 处理 */ }, priority);
mgr.fire({EVT_SYS_STARTUP});                     // 同步
mgr.fireAsync({EVT_SYS_STARTUP}, pool);          // 异步（通过 ThreadPool）
mgr.unsubscribe(h);
size_t n = mgr.subscriberCount(EVT_SYS_STARTUP);
```

设计要点：
- 订阅者在插入时按优先级降序排列，`fire()` 时无需再次排序
- `m_handle_to_type` 映射实现 Handle 维度的 O(1) 取消订阅
- 失效订阅者标记为 inactive（不删除），累积超过 32 个时惰性压缩
- `fire()` 在锁内复制活跃订阅者列表，锁外执行回调，安全支持重入的 subscribe/unsubscribe
- 异常安全：`fire()`/`fireAsync()` 中单个回调异常不影响其他订阅者

---

### netconn.hpp

**class NetConn** —— 异步网络连接类，支持 HTTP 和 WebSocket 协议。

```cpp
ThreadPool pool(4);
Logger logger(Logger::INFO);
NetConn conn(pool, &logger);

// HTTP GET
conn.httpGet("http://example.com/api",
    []() { /* 已连接 */ },
    [](const std::string& data, bool) { /* 收到数据 */ },
    [](const std::string& err) { /* 错误 */ });

// WebSocket
conn.wsConnect("ws://echo.example.com",
    []() { /* 已连接 */ },
    [](const std::string& msg, bool is_text) { /* 收到消息 */ },
    [](const std::string& err) { /* 错误 */ },
    []() { /* 断开 */ });
conn.wsSend("hello");
conn.wsClose();
```

设计要点：
- 基于 POSIX socket + ThreadPool 异步执行，非阻塞 API
- HTTP 支持 GET/POST 请求，自动解析 URL、状态码和 Content-Length
- WebSocket 支持握手、文本/二进制帧收发、Ping/Pong、Close 帧
- 连接超时控制（`setConnectTimeout`）
- 线程安全：所有公有方法可多线程调用
- 利用 Logger 记录连接日志

---

### wsutil.hpp

**工具函数** —— URL 解析和 JSON 辅助函数。

```cpp
auto pu = parseUrl("https://example.com:8080/path?key=val");  // 解析 URL
std::string err = jsonError("something wrong");                 // {"type":"error","msg":"..."}
std::string ok  = jsonOk();                                     // {"type":"ok"}
std::string val = jsonParseStr(msg, "key");                     // 从 JSON 提取字符串字段
int n = jsonParseInt(msg, "count");                             // 从 JSON 提取整数字段
```

设计要点：
- URL 解析基于 `boost::url`，支持 HTTP/HTTPS/WS/WSS 协议，自动处理端口和 query string
- JSON 函数基于 `boost::json`，异常安全（解析失败返回默认值）
- `jsonParseStr`/`jsonParseInt` 提供字符串和 `boost::json::value` 两种重载

---

### ws_server.hpp

**class Session / class Listener** —— 异步 WebSocket 服务端。

```cpp
ThreadPool fallback(DEFAULT_FALLBACK_THREADS);
Logger log(Logger::INFO);
AppModuleCache cache;
asio::io_context io;

auto listener = std::make_shared<Listener>(io, log, cache, &fallback, 3001);
listener->run();
io.run();
```

设计要点：
- `Listener`：async_accept 循环，每个连接创建 `Session`
- `Session`：HTTP Upgrade → 首消息路由 → 异步/遗留双模 app 处理
- 写队列串行化：`async_write` 完成后再发下一条，避免帧交错
- 路由策略：首消息 JSON 中依次查找 `app`/`text`/`game` 字段确定目标 app
- 异步模式优先：若 app 支持 `app_on_input`/`app_set_output`，走异步回调模式；否则走 `app_process` 遗留模式提交到 fallback_pool
- 依赖 `app_mod.hpp`（app 模块）进行动态加载和 app 实例管理
