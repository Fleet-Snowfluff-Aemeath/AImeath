# chat/ —�?文本处理模块

C++17 实现的文本处理模块，编译为共享库 `libchat.so`，通过 `dlopen` 动态加载�?
通过回调函数对输入文本进行处理（默认追加"�?）�?
## 目录结构

```
frontend/plugin/chat/
├── backend/
�?  ├── include/          头文�?�?  �?  └── chat.hpp      Chat 类、C API
�?  ├── src/              源文件实�?�?  �?  └── chat.cpp
�?  ├── test/             单元测试（Google Test�?�?  �?  └── chat_test.cpp
�?  ├── benchmark/        性能基准测试（Google Benchmark�?�?  �?  └── chat_bench.cpp
�?  ├── test_build.sh     快速构建脚�?�?  └── CMakeLists.txt    构建配置
├── frontend/
�?  ├── index.vue      Vue 页面组件
�?  └── config.js         应用配置
└── README.md             （本文档�?```

## 依赖

| 依赖 | 用�?|
|------|------|
| C++17 编译�?| 语言标准 |
| CMake �?3.12 | 构建系统 |
| Google Test | 单元测试框架 |
| Google Benchmark | 性能基准测试框架 |

## 构建

```bash
# 独立构建
cd frontend/plugin/chat/backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 根项目构�?cd ../../../../
./build.sh
```

## 测试

```bash
cd build
./output/test/tests
```

## 基准测试

```bash
cd build
./output/bench/bench_chat
```

## 设计要点

- Chat 类接�?`std::function<std::string(const std::string&)>` 回调，构造函数注�?- C API 接收 `char* (*)(const char*)`，内部包装为 `std::function`，自动管�?C 字符串内�?- `chat_process()` 返回 `malloc` 分配�?C 字符串，调用者负责释�?- 支持多个独立实例并行处理
- 游戏服务器（项目根目�?`backend/main.cpp`）通过 `dlopen` 加载 `libchat.so`，在 `/chat` 路径提供 WebSocket 聊天服务
- 前端页面 `../frontend/index.vue` 通过 `/chat` 路径连接，气泡聊天室风格

## 前端页面

Vue 页面组件位于 `../frontend/index.vue`，通过 WebSocket 连接后端聊天服务器，提供实时消息发送和接收功能�?
页面配置�?`../frontend/config.js`�?
## 相关文档

- [include/README.md](include/README.md) �?头文件说�?- [src/README.md](src/README.md) �?源文件实�?- [test/README.md](test/README.md) �?单元测试
- [benchmark/README.md](benchmark/README.md) �?性能基准测试
- [../frontend/README.md](../frontend/README.md) �?前端页面说明
