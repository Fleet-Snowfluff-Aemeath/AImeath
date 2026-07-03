# benchmark/ — 性能基准测试（Google Benchmark）

## 文件说明

### chat_bench.cpp

- `BM_AppCreateDestroy` — 创建/销毁 ChatApp 实例的性能
- `BM_CommandShortInput` — `/图片` 命令处理吞吐量
- `BM_TextMessageThroughput` — 短文本消息处理吞吐量
- `BM_TextMessageChinese` — 中文文本消息处理性能
- `BM_TextMessageLong` — 长文本（1000 字符）消息处理性能
- `BM_QueueWhileStreaming` — 流式状态下消息入队+排队的性能

## 运行

```bash
cd build
./output/bench/bench_chat
```
