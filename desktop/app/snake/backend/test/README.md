# test/ — 单元测试（Google Test）

共 35 个测试用例，覆盖 Snake、Board、Game 的核心逻辑。

## 文件说明

### snake_test.cpp

**SnakeTest（18 个）**
- 初始状态：位置、方向、身体长度
- 四方向移动（上/下/左/右）
- `popTail()` 长度恢复
- 反向移动被阻止
- 方向缓冲（下一 tick 生效）
- 自碰撞检测（U 形后撞入身体）
- 正常移动无自碰撞
- `hasBodyAt()` 身体位置查询
- 多次移动 + popTail 组合
- 连续方向变化

**BoardTest（7 个）**
- 食物坐标在网格范围内
- 食物生成尺寸验证
- 食物不在蛇身上生成（多次验证）
- `isFoodAt()` 位置匹配
- 满棋盘食物生成

**SnakeGameTest（9 个 + 1 C API）**
- 构造与析构
- tick 移动蛇
- 撞墙游戏结束
- 游戏结束后 tick 被忽略
- 反向方向 tick 被忽略
- 多次 tick 不崩溃
- 累积得分
- 游戏结束后保留分数
- CApi — 完整 C ABI 集成测试

## 运行

```bash
cd build
./output/test/tests
```
