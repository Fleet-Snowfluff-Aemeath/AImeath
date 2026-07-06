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
                                   bool thinking = true,
                                   double temperature = 0.7,
                                   int max_tokens = 4096)
{
    boost::json::object body;
    body["model"] = model;
    body["messages"] = messages;
    body["stream"] = stream;
    body["temperature"] = temperature;
    body["max_tokens"] = max_tokens;
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
        {"description", "打开一个应用程序窗口. 支持的应用: snake (贪吃蛇), gomoku (五子棋), pacman (吃豆人), go (围棋), chat (聊天), terminal (终端), filemanager (文件管理器). NOTE: 如果用户想浏览/查看/管理文件, 请使用 file_list/file_read/file_write 等工具, 不要打开 filemanager 应用. 只有用户明确要求打开文件管理器可视化界面时才使用 open_app filemanager. 同理, shell 命令用 terminal_exec, 不需要打开 terminal."},
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
        {"description", "向已打开的应用发送操作指令. NOTE: This tool ONLY works for game apps (snake, gomoku, pacman, go). For running shell commands, use terminal_exec instead. For browsing files, use file_list/file_read instead. Direction values: 0=up, 1=down, 2=left, 3=right. For go: -1=pass, -2=resign. For gomoku/snake/go/pacman 棋盘格子类游戏, 使用 coord 传入落子位置坐标 [row,col], 如第一行第一列为 [0,0]. 同名app有多个实例时, 用 instance 参数指定 (从0开始). 状态注入中 -N 后缀的数字即 instance 值."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"app", {{"type","string"},{"description","目标应用名称"}}},
                {"value", {{"type","integer"},{"description","方向值 (0=up/1=down/2=left/3=right), 仅方向类游戏使用"}}},
                {"coord", {{"type","array"},{"items", {{"type","integer"}}},{"description","落子坐标 [row, col], 棋盘格子类游戏使用"}}},
                {"instance", {{"type","integer"},{"description","实例编号 (同名多实例时使用, 从0开始)"}}}
            }},
            {"required", boost::json::array{"app"}}
        }}
    };
    tools.push_back(std::move(t2));

    boost::json::object t3;
    t3["type"] = "function";
    t3["function"] = {
        {"name", "close_app"},
        {"description", "关闭一个已打开的应用窗口. 可用 list_active_windows 获取 window_id 来指定关闭哪一个."},
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
        {"description", "查询一个应用的当前状态. NOTE: This tool ONLY works for game apps (snake, gomoku, pacman, go). For chat it returns empty (chat messages cannot be read via this tool). For file browsing use file_list/file_read. For terminal use terminal_exec. 如果返回 success:false 则表示该应用未在运行. 先用 list_active_windows 确认哪些应用在运行再查询."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"app", {{"type","string"},{"description","应用名称: snake, gomoku, pacman, go, terminal, filemanager"}}},
                {"instance", {{"type","integer"},{"description","实例编号 (同名多实例时使用, 从0开始)"}}}
            }},
            {"required", boost::json::array{"app"}}
        }}
    };
    tools.push_back(std::move(t4));

    boost::json::object t5;
    t5["type"] = "function";
    t5["function"] = {
        {"name", "list_apps"},
        {"description", "列出当前所有正在运行的应用窗口和数量。返回每个app的名称和实例数及总数。这是获取运行中应用数量的最可靠方法。"},
        {"parameters", {
            {"type", "object"},
            {"properties", {{"dummy", {{"type","string"},{"description","无需参数"}}}}
        }}
    }};
    tools.push_back(std::move(t5));

    // ========== New tools ==========

    boost::json::object t6;
    t6["type"] = "function";
    t6["function"] = {
        {"name", "chat_send"},
        {"description", "向当前用户或指定聊天实例发送消息。不指定 instance 时回复当前用户；指定 instance 时发送到对应聊天窗口(chat-0, chat-1等)。使用前先调用 list_active_windows 查看当前有哪些聊天实例。"},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"text", {{"type","string"},{"description","要发送的消息内容"}}},
                {"instance", {{"type","integer"},{"description","目标聊天实例编号(0,1,2...)，不指定则回复当前用户"}}}
            }},
            {"required", boost::json::array{"text"}}
        }}
    };
    tools.push_back(std::move(t6));

    boost::json::object t7;
    t7["type"] = "function";
    t7["function"] = {
        {"name", "file_list"},
        {"description", "列出指定目录中的文件和文件夹. 当用户想浏览/查看文件夹内容时, 请使用此工具而非打开文件管理器. 这是查看目录内容的唯一方式."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type","string"},{"description","目录路径, 默认为 '/'"}}}
            }},
            {"required", boost::json::array{}}
        }}
    };
    tools.push_back(std::move(t7));

    boost::json::object t8;
    t8["type"] = "function";
    t8["function"] = {
        {"name", "file_read"},
        {"description", "读取指定文件的内容. 可以读取文本文件、代码文件、配置文件等. 使用绝对路径."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type","string"},{"description","文件路径"}}}
            }},
            {"required", boost::json::array{"path"}}
        }}
    };
    tools.push_back(std::move(t8));

    boost::json::object t9;
    t9["type"] = "function";
    t9["function"] = {
        {"name", "file_write"},
        {"description", "向指定文件写入内容. 如果文件不存在则创建, 如果已存在则覆盖. 可用于创建新文件、修改文件内容等. 使用绝对路径."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type","string"},{"description","文件路径"}}},
                {"content", {{"type","string"},{"description","要写入的内容"}}}
            }},
            {"required", boost::json::array{"path","content"}}
        }}
    };
    tools.push_back(std::move(t9));

    boost::json::object t10;
    t10["type"] = "function";
    t10["function"] = {
        {"name", "file_mkdir"},
        {"description", "创建一个新目录. 如果父目录不存在也会尝试创建. 使用绝对路径."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type","string"},{"description","目录路径"}}}
            }},
            {"required", boost::json::array{"path"}}
        }}
    };
    tools.push_back(std::move(t10));

    boost::json::object t11;
    t11["type"] = "function";
    t11["function"] = {
        {"name", "file_remove"},
        {"description", "删除指定文件或空目录. 只能删除空目录, 非空目录无法删除. 使用绝对路径."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type","string"},{"description","要删除的文件或目录路径"}}}
            }},
            {"required", boost::json::array{"path"}}
        }}
    };
    tools.push_back(std::move(t11));

    boost::json::object t12;
    t12["type"] = "function";
    t12["function"] = {
        {"name", "terminal_exec"},
        {"description", "在终端中执行一条 shell 命令并获取输出. 当用户想执行 ls/pwd/echo/cat/date 等命令时, 请直接使用此工具, 不要先打开终端应用. 可以执行任何命令如 ls (列出文件), pwd (当前路径), echo (输出文字), cat (查看文件), whoami (当前用户), date (日期) 等. 这是执行 shell 命令的唯一方式. 不需要先 open_app terminal, 直接使用此工具即可."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"command", {{"type","string"},{"description","要执行的命令"}}}
            }},
            {"required", boost::json::array{"command"}}
        }}
    };
    tools.push_back(std::move(t12));

    boost::json::object t13;
    t13["type"] = "function";
    t13["function"] = {
        {"name", "terminal_stdin"},
        {"description", "向正在运行的终端命令发送标准输入数据 (用于交互式命令)."},
        {"parameters", {
            {"type", "object"},
            {"properties", {
                {"data", {{"type","string"},{"description","要发送的输入数据"}}}
            }},
            {"required", boost::json::array{"data"}}
        }}
    };
    tools.push_back(std::move(t13));

    boost::json::object t14;
    t14["type"] = "function";
    t14["function"] = {
        {"name", "list_active_windows"},
        {"description", "列出当前所有活跃的应用窗口及其 session 信息. 这是获取当前运行应用数量的唯一可靠方法. 返回包含 count 字段表示窗口总数."},
        {"parameters", {
            {"type", "object"},
            {"properties", {}}
        }}
    };
    tools.push_back(std::move(t14));

    return tools;
}

inline void inject_tools(std::string& body, bool with_tools,
    const std::vector<std::string>& toolWhitelist = {},
    const boost::json::array* toolDefs = nullptr)
{
    if (!with_tools) return;
    auto body_json = boost::json::parse(body);
    if (!body_json.is_object()) return;
    if (!toolDefs) {
        body_json.as_object()["tools"] = get_default_tools();
    } else if (toolWhitelist.empty()) {
        body_json.as_object()["tools"] = *toolDefs;
    } else {
        boost::json::array filtered;
        for (auto& t : *toolDefs) {
            std::string name = t.at("function").as_object().at("name").as_string().c_str();
            for (auto& w : toolWhitelist) {
                if (name == w) {
                    filtered.push_back(t);
                    break;
                }
            }
        }
        body_json.as_object()["tools"] = std::move(filtered);
    }
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
