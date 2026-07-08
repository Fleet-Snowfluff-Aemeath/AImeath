# include/ — 共享头文件

游戏框架的公共接口定义，被围棋、五子棋、贪食蛇、吃豆豆模块依赖。

## 文件说明

### direction.hpp
`Direction` 枚举：UP、DOWN、LEFT、RIGHT。
提供 `isOppositeDir()` 和 `applyDir()` 内联工具函数。

### game_base.hpp
`Game` 抽象基类，定义统一的游戏接口：
- 纯虚方法：`tick`、`isOver`、`score`、`getState`
- 继承 `boost::noncopyable`

### game_api.hpp
通过 `APP_GAME_API_COMMON()` 宏自动生成 C ABI 包装函数：
- `plugin_create` / `plugin_destroy` / `plugin_process` / `plugin_free_string` / `plugin_is_done`
- 包含 `_GamePluginCtx` 上下文结构和 JSON 辅助函数
