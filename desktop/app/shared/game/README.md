# shared/game — 游戏共享库

所有棋盘游戏（围棋、五子棋、贪食蛇、吃豆豆）共享的基础类型和 C ABI 框架。

## 目录结构

```
shared/game/
├── include/
│   ├── direction.hpp   # Direction 枚举 + 方向工具函数
│   ├── game_base.hpp   # Game 抽象基类
│   └── game_api.hpp    # APP_GAME_API_COMMON 宏 + C ABI 生成
├── test/
│   ├── direction_test.cpp
│   └── game_base_test.cpp
├── benchmark/
│   ├── direction_bench.cpp
│   └── game_bench.cpp
└── CMakeLists.txt
```

## API 说明

### Game 基类 (`game_base.hpp`)
抽象基类，定义游戏统一接口：
- `tick(int action)` — 处理一个操作
- `isOver()` — 游戏是否结束
- `score()` — 当前得分
- `getState()` — 序列化完整状态为 JSON

### C ABI (`game_api.hpp`)
通过 `APP_GAME_API_COMMON()` 宏自动生成游戏的 `app_create` / `app_destroy` / `app_process` / `app_free_string` / `app_is_done` 等 C 函数。

**使用方法：**
1. 在游戏的 `.cpp` 中 `#define GAME_CLASS YourGameClass`
2. 如果构造函数需要自定义参数：`#define GAME_CONSTRUCT(w, h) new YourGameClass(w)`
3. 调用 `APP_GAME_API_COMMON()` 一次性生成全部 C ABI 函数

### Direction (`direction.hpp`)
- `Direction` 枚举：UP, DOWN, LEFT, RIGHT
- `isOppositeDir()` — 判断是否反向
- `applyDir()` — 将方向应用到坐标
