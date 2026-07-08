// terminal.hpp — 命令行终端 C ABI 后端
//
// PTY 会话管理委托给 module/core 的 PtySession。
// 本模块定义终端应用的 JSON 协议适配层：
//   - plugin_create / plugin_destroy / plugin_is_done — 生命周期
//   - plugin_set_output / plugin_on_input — 异步模式回调
//   - plugin_process — 同步模式 (exec, exec_sync, stdin, stdout, resize)
//
// 实现文件: src/terminal.cpp

#include "plugin.hpp"

struct PtySession;
