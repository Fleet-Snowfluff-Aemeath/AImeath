# Lab —— 基于 Boost 的并发工具库

C++17 库，提供线程安全的线程池、定时器、日志、事件管理器和网络连接器。
通过 `add_subdirectory(core)` 集成到父项目，也可独立构建测试与基准测试。

## 特性

- **ThreadPool** —— 固定大小线程池，`notify_one()` 避免惊群，`m_done` condition_variable 隔离 `wait_all()` 与工作线程。任务通过 `boost::asio::post` 投递，工作线程调用 `m_io.run()` 处理。异常写入 `stderr` 不崩溃。
- **Timer** —— 基于 `boost::asio::steady_timer` 的定时器，回调通过 ThreadPool 异步提交（不阻塞 io_context 事件循环）。内部状态由 `shared_ptr<TimerState>` 管理（互斥锁 + 定时器表），析构安全无 UAF。O(1) `exists()`/`cancel()`。
- **Logger** —— RAII 线程安全日志，消息先缓冲到 `ostringstream` 再一次性写入（减少锁持有时间）。支持 `std::cerr`/文件/自定义流。时间戳秒级缓存。
- **EventManager** —— 发布-订阅模式，基于 `boost::signals2`。O(1) 取消订阅，`subscriberCount` 返回 0 时自动清理空信号，支持 `cleanup()` 惰性压缩。异常隔离，锁内复制信号、锁外执行回调。
- **NetConn** —— 异步 HTTP/WebSocket 客户端，基于 ThreadPool 的 io_context。支持 GET/POST/WS connect/send/close。WebSocket 支持自动重连（指数退避，最多 10 次）。状态机正确更新 CONNECTED/DISCONNECTED。
- **ws_server** —— 异步 WebSocket 服务端基础设施。Session 管理单连接生命周期（HTTP Upgrade → 路由 → 消息循环），Listener 提供 async_accept 循环。支持异步/遗留双模 app，写队列串行化 async_write。
- **wsutil** —— URL 解析（基于 `boost::url`，含 query string）和 JSON 工具函数（`jsonParseStr`/`jsonParseInt` 基于 `boost::json::parse` + `try_value_to`）。
## 目录结构

```
├── include/         头文件（类声明 + 少量必须内联代码）
│   ├── threadmgr.hpp
│   ├── timer.hpp
│   ├── logger.hpp
│   ├── eventmgr.hpp
│   ├── netconn.hpp
│   ├── wsutil.hpp
│   └── ws_server.hpp
├── src/             源文件实现（编译为 libcore.so）
├── test/            Google Test 单元测试（6 文件，83 用例）
├── benchmark/       Google Benchmark 性能测试
└── CMakeLists.txt   构建配置
```

## 依赖

- C++17 编译器（GCC 11+）
- CMake ≥ 3.12
- Boost（url, json, filesystem）— 通过 `BOOST_ROOT` 指定安装路径
- OpenSSL
- Google Test（测试，可选）
- Google Benchmark（基准测试，可选）

## 快速开始

```cpp
#include "threadmgr.hpp"
#include "timer.hpp"
#include "logger.hpp"
#include "eventmgr.hpp"

int main()
{
    ThreadPool pool(4);
    Logger log(Logger::INFO);

    // 定时器（回调提交到 ThreadPool 异步执行）
    Timer timer(pool);
    timer.setTimeout(std::chrono::milliseconds(100), [&]() {
        log.info() << "timer fired";
    });

    // 事件
    EventManager evt;
    evt.subscribe(100, [&](const Event&) { log.info() << "event received"; });
    evt.fire({100});

    pool.wait_all();
}
```

## 构建与运行

```bash
# 独立构建（含测试和基准测试）
cd core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make tests                 # 编译单元测试
./output/test/tests        # 运行全部测试
./output/test/tests --gtest_filter=TimerTest.*  # 筛选测试套件

make bench                 # 编译全部基准测试
./output/bench/bench_timer --benchmark_min_time=0.2

# 作为子项目（仅 libcore.so，不含测试/基准测试）
# 在父 CMakeLists.txt 中添加 add_subdirectory(core)
```

## 设计要点

- **头/实现分离**：非模板实现在 `src/*.cpp`；必须的模板内联代码少量保留在 `include/*.hpp`
- **零阻塞定时器**：Timer 回调通过 ThreadPool `submit()` 异步提交，不阻塞 io_context 事件循环线程
- **UAF 安全**：Timer 内部状态（互斥锁 + 定时器表）由 `shared_ptr<TimerState>` 管理，回调持有 `weak_ptr<TimerState>` 确保安全访问。析构时 move 出状态后 cancel，in-flight 回调不会访问已析构对象
- **异常安全**：ThreadPool 任务异常写入 stderr 不崩溃；Timer 回调异常由 ThreadPool 隔离不 `std::terminate`；EventManager 单回调异常不扩散
- **锁粒度优化**：Timer 释放锁后提交回调到 ThreadPool；EventManager 锁内复制信号、锁外执行；Logger 缓冲完整消息后一次写锁
- **自动清理**：EventManager `subscriberCount` 在返回 0 时自动 erase 空信号，防止 map 无限增长

## 相关文档

- [include/README.md](include/README.md) — 头文件说明
- [src/README.md](src/README.md) — 源文件实现
- [test/README.md](test/README.md) — 单元测试
- [benchmark/README.md](benchmark/README.md) — 性能基准测试
