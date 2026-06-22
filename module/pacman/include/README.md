# include/ —— 头文件

吃豆豆模块的公共接口定义，编译为共享库后通过 `dlopen` 动态加载。

## 文件说明

### board.hpp
`Board` 类定义：
- 使用 `std::vector<bool>` 标记豆子位置
- 核心操作：`generateBeans()` 随机生成豆子、`hasBean()` 查询、`removeBean()` 吃掉
- `beanCount()` 返回剩余豆子数量

### game.hpp
`Game` 类定义 + `extern "C"` C API：
- `Direction` 枚举：`UP` / `DOWN` / `LEFT` / `RIGHT`
- 方向变更缓冲（`m_next_dir`）：当前 tick 内首次按键后忽略后续输入（复用 snake 模式）
- `tick(dir)`：移动玩家 → 检测撞墙 → 吃豆 → 检测胜利
- C API：`game_new` / `game_free` / `game_tick` / `game_is_over` / `game_get_score`
