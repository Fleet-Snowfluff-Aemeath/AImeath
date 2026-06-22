# benchmark/ —— 性能基准测试（Google Benchmark）

四个可执行文件，分别测量各组件的延迟和吞吐量。测试平台：24 核 AMD @ 3.07 GHz（GCC 11.4, Release）。

## 文件说明

### threadpool_bench.cpp
- `BM_SubmitEmptyTask` —— 提交空任务的耗时（1 线程池约 63ns）
- `BM_SubmitAndWait` —— P 线程处理 N 个任务的吞吐量
- `BM_SubmitHeavyTask` —— CPU 密集型任务的调度开销
- `BM_WaitAllOverhead` —— 线程池创建/提交/等待/销毁的完整周期
- `BM_ConcurrentSubmit` —— 外部线程并发提交时的锁争用

### timer_bench.cpp
- `BM_TimerSetTimeout` —— 定时器创建开销（约 19μs/个）
- `BM_TimerSetTimeoutAt` —— 绝对时间点定时器创建开销（与 setTimeout 一致）
- `BM_TimerSetAndCancel` —— 创建 + 取消的开销
- `BM_TimerFire` —— 端到端定时器触发延迟（含 5ms 真实等待）
- `BM_TimerSetInterval` —— 周期定时器吞吐量
- `BM_TimerExists` —— O(1) 查询 `m_alive`（10 个定时器约 128ns）

### logger_bench.cpp
- `BM_LogSingleMessage` —— 单条消息耗时（约 122ns，860 万条/秒，启用时间戳缓存）
- `BM_LogWithAllLevels` —— 连续输出所有级别
- `BM_LogFilteredLevel` —— 被过滤消息的耗时（约 80ns，无锁）
- `BM_LogLongMessage` —— 大字符串格式化开销
- `BM_LogConcurrent` —— 多线程并发争用

### netconn_bench.cpp
- `BM_NetConnParseUrl` —— URL 解析（HTTP）
- `BM_NetConnParseWsUrl` —— URL 解析（WebSocket）
- `BM_NetConnBuildHttpGet` —— HTTP GET 请求构造
- `BM_NetConnBuildHttpPost` —— HTTP POST 请求构造（含 1KB body）
- `BM_NetConnBase64Encode` —— Base64 编码 256 字节
- `BM_NetConnWsBuildKey` —— WebSocket 握手 key 生成
- `BM_NetConnWsEncodeFrame` —— WebSocket 帧编码（128 字节 payload）
- `BM_NetConnWsDecodeFrame` —— WebSocket 帧解码

### eventmgr_bench.cpp
- `BM_EventSubscribe` —— 订阅开销（优化后约 7.9μs vs 优化前 41μs）
- `BM_EventFire` —— 同步派发延迟（1 订阅者约 14ns，200 订阅者约 760ns）
- `BM_EventFireAsync` —— 异步派发（通过 ThreadPool）
- `BM_EventSubscribeAndUnsubscribe` —— 订阅 + 取消的完整周期
- `BM_EventPriorityFire` —— 按优先级排序的派发
- `BM_EventSubscriberCount` —— 活跃订阅者计数

## 运行

```bash
cd build
make bench                          # 编译全部
./bench_threadpool --benchmark_min_time=0.2
./bench_timer     --benchmark_min_time=0.2
./bench_logger    --benchmark_min_time=0.2
./bench_eventmgr  --benchmark_min_time=0.2
./bench_netconn   --benchmark_min_time=0.2

# 筛选指定测试
./bench_eventmgr --benchmark_filter=BM_EventFire
```
