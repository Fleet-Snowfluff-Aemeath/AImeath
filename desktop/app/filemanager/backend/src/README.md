# src/ — 源文件实现

## 文件说明

### filemgr.cpp

文件管理器后端实现，提供 C ABI 接口：

- **listAction(path)** — 列出目录内容，返回 `{type:"listing", entries:[...]}`
- **readAction(path)** — 读取文件内容，返回 `{type:"file", content:"..."}`
- **mkdirAction(path)** — 创建目录
- **writeAction(path, content)** — 写入文件
- **removeAction(path)** — 删除文件或目录

所有文件操作委托给 `VirtualFileSystem` 单例（`module/core` 提供），支持路径遍历防护。
使用 Boost.JSON 进行序列化/反序列化。
