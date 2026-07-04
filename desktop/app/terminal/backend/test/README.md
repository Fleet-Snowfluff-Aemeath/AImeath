# test/ — 单元测试（Google Test）

共 13 个测试用例，覆盖终端后端 C ABI 核心逻辑。

## 文件说明

### terminal_test.cpp

**基础生命周期（2 个）**
- `CreateAndDestroy` — 创建后销毁不崩溃
- `CreateWithNullConfig` — 用 `nullptr` 创建

**ExecSync（3 个）**
- `ExecSyncEcho` — `echo hello` 返回 output 包含 "hello"
- `ExecSyncMissingCommand` — 缺少 command 字段返回 error
- `ExecSyncInvalidCommand` — 不存在的命令返回 error
- `ExecSyncEmptyCommand` — `true` 命令返回 output

**错误处理（3 个）**
- `InvalidJson` — 非法 JSON 返回 error
- `MissingActionField` — 缺少 action 返回 error
- `UnknownAction` — 未知 action 返回 error
- `ExecMissingCmd` — exec 缺少 cmd 返回 error

**其他（3 个）**
- `ResizeWithoutSession` — 无会话时 resize 返回空数组
- `IsDoneWithoutSession` — 无会话时 is_done 返回 0
- `MultipleExecSyncCalls` — 20 次 exec_sync 不崩溃
- `MultipleCreatesAndDestroys` — 多次创建销毁 + exec_sync

## 运行

```bash
cd build
./output/test/tests
```
