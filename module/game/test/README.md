# game/test/ —— 单元测试

覆盖 game 模块的全部公开接口：

| 测试套件 | 测试项 | 说明 |
|----------|--------|------|
| `GameBaseTest` | VirtualDispatch, TickIgnoredWhenOver | Game 抽象基类虚函数分发 + 游戏结束后 tick 忽略 |
| `DirectionTest` | AllDirectionsDistinct, IsOppositeDir, ApplyDir | Direction 枚举值、反向判断、坐标移动 |
| `GameApiTest` | ApiDispatch | GAME_API_COMMON 宏生成的 C API 函数正确性 |