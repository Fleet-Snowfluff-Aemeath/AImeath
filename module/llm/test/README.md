# 单元测试说明

## llm_test.cpp

共 20 个测试（12 API 测试 + 8 工具函数测试）。

### LlmClient / LlmRequest（12 个）
- 构造/析构生命周期
- cancel 行为（启动前/启动后）
- 无效主机连接失败处理
- SSL 验证开关
- 自定义 target / 连接超时
- 同步 POST/GET 错误处理
- LlmResponse 字段默认值

### llm_utils（8 个）
- `build_chat_body` — 模型名/stream/thinking 配置
- `get_default_tools` — 返回 4+ 工具，名称正确
- `inject_tools` — 注入 tools + tool_choice
- `merge_tool_calls` — 基本合并/分块合并/多 ID/空输入
