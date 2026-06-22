# src/ —— 源文件实现

## 文件说明

### board.cpp
`Board` 类实现：
- 构造函数：分配 `width × height` 的 `bool` 网格，初始无豆
- `generateBeans()`：随机选坐标放置指定数量豆子，避开玩家起始位置
- `removeBean()`：吃豆，更新计数
- 生成数量超过网格容量时自动截断

### game.cpp
`Game` 类实现：
- 构造函数：初始化棋盘，在中心放置玩家，随机生成 30 颗豆子
- `tick(dir)`：设置方向 → 计算新位置 → 撞墙则结束 → 移动玩家 → 吃豆 +10 分 → 豆子吃完则胜利
- C API 包装：`game_new` / `game_free` / `game_tick` / `game_is_over` / `game_get_score`
