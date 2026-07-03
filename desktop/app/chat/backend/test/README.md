# test/ — 单元测试（Google Test）

共 20 个测试用例，覆盖 ChatApp 的核心逻辑。

## 文件说明

### chat_test.cpp

**基础生命周期（3 个）**
- `InitialStateNotDone` — 创建后 `app_is_done` 返回 0
- `CreateWithNullConfig` — 用 `nullptr` 创建不崩溃
- `CreateWithEmptyConfig` — 用 `"{}"` 创建不崩溃

**命令处理（4 个）**
- `CommandDoesNotSetDone` — 命令不设置 done 标记
- `CommandProducesEmbedOutput` — `/图片` 生成 embed 输出
- `GameCommandProducesEmbed` — `/游戏 snake` 生成 embed 输出
- `UnknownCommandProducesTextEmbed` — 未知命令生成文本 embed

**多轮对话压力（4 个）**
- `MultiRoundTextWithoutApiKeyDoesNotSetDone` — 15 轮命令不设置 done
- `MultiRoundMixedCommandsAndText` — 10 轮命令+文本混合
- `DestroyAfterMultipleRounds` — 20 轮文本后销毁
- `StressMultiRound` — 50 轮压力测试

**消息队列（1 个）**
- `TextWhileStreamingQueues` — 流式进行中消息入队

**Stop 动作（1 个）**
- `StopActionClearsStreaming` — stop 动作清除流状态和队列

**Poll 动作（1 个）**
- `PollActionDoesNotCrash` — poll 动作不崩溃

**流状态测试（1 个）**
- `SetStreamingAndDrainQueue` — 强制设置流状态后排出队列

**多实例独立（2 个）**
- `MultipleInstancesIndependent` — 两个实例互不影响
- `DestroyOneDoesNotAffectOther` — 销毁一个不影响另一个

**连续 stop（1 个）**
- `MultipleStopDoesNotCrash` — 多次 stop 不崩溃

**工具调用（1 个）**
- `ToolCallWithoutCacheGraceful` — 无缓存时工具调用优雅返回

## 运行

```bash
cd build
./output/test/tests
```
