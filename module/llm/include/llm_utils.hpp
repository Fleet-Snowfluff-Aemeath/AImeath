#pragma once

#include <boost/json.hpp>
#include <string>
#include <map>
#include <vector>
#include "llm_client.hpp"

namespace llm {

inline std::string build_chat_body(const boost::json::array& messages,
                                   const std::string& model = "deepseek-v4-flash",
                                   bool stream = true,
                                   bool thinking = true)
{
    boost::json::object body;
    body["model"] = model;
    body["messages"] = messages;
    body["stream"] = stream;
    if (thinking)
        body["thinking"] = {{"type", "enabled"}, {"budget_tokens", 4096}};
    return boost::json::serialize(body);
}

inline boost::json::array get_default_tools()
{
    boost::json::array tools;

    boost::json::object t1;
    t1["type"] = "function";
    t1["function"] = {
        {"name", "open_app"},
        {"description", "打开一个应用程序窗口. Use when the user asks to open or launch an app."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"app", {{"type","string"},{"description","应用名称, 可选: snake, gomoku, pacman, go, chat, terminal, filemanager"}}}
            }},
            {"required", boost::json::array{"app"}}
        }}
    };
    tools.push_back(std::move(t1));

    boost::json::object t2;
    t2["type"] = "function";
    t2["function"] = {
        {"name", "control_app"},
        {"description", "向已打开的应用发送操作指令. Direction values: 0=up, 1=down, 2=left, 3=right. For go: -1=pass, -2=resign. For gomoku/snake/go/pacman 棋盘格子类游戏, 使用 coord 传入落子位置坐标 [row,col], 如第一行第一列为 [0,0]."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"app", {{"type","string"},{"description","目标应用名称"}}},
                {"value", {{"type","integer"},{"description","方向值 (0=up/1=down/2=left/3=right), 仅方向类游戏使用"}}},
                {"coord", {{"type","array"},{"items", {{"type","integer"}}},{"description","落子坐标 [row, col], 棋盘格子类游戏使用"}}}
            }},
            {"required", boost::json::array{"app"}}
        }}
    };
    tools.push_back(std::move(t2));

    boost::json::object t3;
    t3["type"] = "function";
    t3["function"] = {
        {"name", "close_app"},
        {"description", "关闭一个已打开的应用窗口."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"app", {{"type","string"},{"description","要关闭的应用名称"}}}
            }},
            {"required", boost::json::array{"app"}}
        }}
    };
    tools.push_back(std::move(t3));

    boost::json::object t4;
    t4["type"] = "function";
    t4["function"] = {
        {"name", "get_app_state"},
        {"description", "获取当前已打开 app 的状态信息（棋盘、分数、游戏状态等），用于 agent 感知 app 状态后再决定控制操作."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"app", {{"type","string"},{"description","应用名称: snake, gomoku, pacman, go, terminal, filemanager"}}}
            }},
            {"required", boost::json::array{"app"}}
        }}
    };
    tools.push_back(std::move(t4));

    return tools;
}

inline void inject_tools(std::string& body, bool with_tools)
{
    if (!with_tools) return;
    auto body_json = boost::json::parse(body);
    if (!body_json.is_object()) return;
    body_json.as_object()["tools"] = get_default_tools();
    body_json.as_object()["tool_choice"] = boost::json::string("auto");
    body = boost::json::serialize(body_json);
}

inline std::map<std::string, LlmToolCall> merge_tool_calls(const std::vector<LlmToolCall>& chunks)
{
    std::map<std::string, LlmToolCall> merged;
    for (auto& tc : chunks) {
        if (tc.id.empty()) {
            if (!merged.empty())
                merged.rbegin()->second.function_arguments += tc.function_arguments;
            continue;
        }
        auto& entry = merged[tc.id];
        if (entry.id.empty()) entry.id = tc.id;
        if (!tc.function_name.empty()) entry.function_name = tc.function_name;
        entry.function_arguments += tc.function_arguments;
    }
    return merged;
}

} // namespace llm
