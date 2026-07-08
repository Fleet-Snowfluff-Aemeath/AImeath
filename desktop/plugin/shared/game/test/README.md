# test/ — 单元测试（Google Test）

共 9 个测试用例，覆盖 Game 基类和 C ABI 框架。

## 文件说明

### direction_test.cpp（3 个）
- `AllDirectionsDistinct` — 四个方向各不相同
- `IsOppositeDir` — 反向判断正确性
- `ApplyDir` — 方向应用到坐标

### game_base_test.cpp（6 个）
- `VirtualDispatch` — 虚函数多态调用
- `TickIgnoredWhenOver` — 结束后忽略操作
- `CreateAndDestroy` — C ABI 创建销毁
- `NewGameAndTick` — C ABI new_game + tick
- `PauseResume` — 暂停/恢复机制
- `EndGameAndDone` — 结束游戏 + is_done
- `UnknownAction` — 未知操作返回 error
- `GetState` — 获取状态序列化

## 运行

```bash
cd build
./output/test/game_shared_tests
```
