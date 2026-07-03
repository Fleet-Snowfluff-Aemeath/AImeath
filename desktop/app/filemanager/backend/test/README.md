# test/ — 单元测试（Google Test）

共 15 个测试用例，覆盖文件管理器 C ABI 的核心逻辑。

## 文件说明

### filemanager_test.cpp

**基础生命周期（2 个）**
- `CreateAndDestroy` — 创建后 `app_is_done` 返回 0
- `CreateWithNullConfig` — 用 `nullptr` 创建不崩溃

**List 操作（3 个）**
- `ListRootDirectory` — 列出根目录
- `ListHomeDirectory` — 列出 `/home` 目录
- `ListInvalidPath` — 非法路径返回 error

**Read 操作（1 个）**
- `ReadNonexistentFile` — 读取不存在的文件返回 error

**错误处理（3 个）**
- `MissingActionField` — 缺少 action 字段返回 error
- `UnknownAction` — 未知 action 返回 error
- `InvalidJson` — 非法 JSON 返回 error
- `EmptyArrayInput` — 数组输入返回 error

**Mkdir / Write / Remove（3 个）**
- `MkdirAction` — 创建目录
- `WriteAction` — 写入文件
- `RemoveNonexistentFile` — 删除不存在文件返回 error

**其他（3 个）**
- `IsDoneAlwaysFalse` — done 永远为 0
- `StressListManyTimes` — 100 次 list 不崩溃
- `MultipleInstancesIndependent` — 多实例互不影响
- `WriteThenRead` — 写入后读取验证

## 运行

```bash
cd build
./output/test/filemanager_tests
```
