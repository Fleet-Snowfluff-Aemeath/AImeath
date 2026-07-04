// terminal.hpp — 命令行终端 C ABI 后端
//
// PTY 会话管理委托给 module/core 的 PtySession。
// 本模块定义终端应用的 JSON 协议适配层：
//   - app_create / app_destroy / app_is_done — 生命周期
//   - app_set_output / app_on_input — 异步模式回调
//   - app_process — 同步模式 (exec, exec_sync, stdin, stdout, resize)
//
// 实现文件: src/terminal.cpp

#include "app_api.hpp"

struct PtySession;
