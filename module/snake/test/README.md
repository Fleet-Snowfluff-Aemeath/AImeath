# test/ —— 单元测试（Google Test）

共 16 个测试用例，覆盖 Snake、Board 和 Game 构造。

## 文件说明

### snake_test.cpp（16 个测试）

**SnakeTest（12 个）**
- 初始状态：位置、方向、身体长度
- 四方向移动（上/下/左/右）
- `popTail()` 长度恢复
- 反向移动被阻止
- 方向缓冲（下一 tick 生效）
- 自碰撞检测（绕 U 形后撞入身体）
- 正常移动无自碰撞
- `hasBodyAt()` 身体位置查询
- 多次移动 + popTail 组合

**BoardTest（3 个）**
- 食物坐标在网格范围内
- 食物不在蛇身上生成（100 次随机验证）
- `isFoodAt()` 位置匹配

**GameTest（1 个）**
- 构造与析构

## 运行

```bash
cd snake/build
make tests
./output/test/tests
```
