# src/ —— 源文件实现

## 文件说明

### snake.cpp
`Snake` 类的实现：
- 构造函数：从 `(start_x, start_y)` 开始，初始向右，身体长度 3
- `setDirection()`：禁止 180° 反向，缓冲到 `m_next_dir`
- `advance()`：按当前方向计算新头部位置，`push_front` 插入
- `popTail()`：`pop_back` 移除尾部
- `collidesWithSelf()`：头部与身体其余部分比较

### board.cpp
`Board` 类的实现：
- `generateFood()`：`std::rand()` 随机坐标，循环直到不落在蛇身上

### game.cpp
`Game` 类的实现：
- 构造函数：初始化棋盘和蛇，`std::srand` 种子，生成首个食物
- `tick()` → `update()` + `render()`：更新状态并绘制画面
- `update()`：前进 → 判断吃食物 → 收缩 → 检查撞墙/撞自身
- `render()`：ANSI 转义序列清屏 + 光标定位，`O` 蛇头 `#` 蛇身 `$` 食物
- `renderGameOver()`：显示 Game Over 和最终分数
- C API 包装：`game_new` / `game_free` / `game_tick` / `game_is_over` / `game_get_score`
