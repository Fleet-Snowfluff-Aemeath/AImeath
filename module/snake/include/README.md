# include/ —— 头文件

本项目的公共接口定义，编译为共享库后通过 `dlopen` 动态加载。

## 文件说明

### snake.hpp
`Snake` 类定义：
- 使用 `std::deque<Position>` 管理蛇身，头部在 front，尾部在 back
- 方向变更缓冲 `m_next_dir`，防止单 tick 内多次转向导致自身碰撞
- 核心操作：`advance()` 前进、`popTail()` 收缩、`collidesWithSelf()` 碰撞检测

### board.hpp
`Board` 类定义：
- 网格大小 `m_width × m_height`
- 食物位置 `m_food`，`generateFood()` 随机生成并避开蛇身

### game.hpp
`Game` 类定义 + `extern "C"` C API：
- 主循环 `tick()`：更新逻辑 + 渲染（ANSI 转义序列）
- C API：`game_new` / `game_free` / `game_tick` / `game_is_over` / `game_get_score`
- 供 `main.cpp` 通过 `dlsym` 动态调用

## 类型

```cpp
enum class Direction { UP, DOWN, LEFT, RIGHT };

struct Position {
    int x, y;
};
```
