#include <app_api.hpp>

#include <iostream>
#include <string>
#include <vector>
#include "message_queue.hpp"
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <set>
#include <utility>
#include <memory>
#include <chrono>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>

#include "agent_chat_api.hpp"
#include "agent_manager.hpp"
#include "llm_client.hpp"
#include "tool_registry.hpp"
#include "llm_utils.hpp"
#include "config.hpp"
#include "ws_server.hpp"
#include "iface_mod.hpp"

namespace asio  = boost::asio;

// ---- ChatApp state ----

#define CHAT_LOG(level, msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto t = std::chrono::system_clock::to_time_t(now); \
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
            now.time_since_epoch()) % 1000; \
        char _buf[32]; \
        std::strftime(_buf, sizeof(_buf), "%H:%M:%S", std::localtime(&t)); \
        std::cerr << "[" << _buf << "." << ms.count() << "] " << level << " " << msg << std::endl; \
    } while(0)

struct ChatApp;
static void processNextInQueue(ChatApp* app);
static void handleUserMessageAsync(ChatApp* app, const std::string& text, const std::string& sender_name = "用户");
static std::string executeTool(ChatApp* app, const std::string& name, const std::string& argsJson);
static void processToolCalls(ChatApp* app,
    const std::vector<LlmToolCall>& mergedList,
    const std::string& response,
    const std::string& reasoning);

struct AppInstance
{
    AppModule mod;
    AppPtr handle;
    std::string appName;
};

static const char* displayName(const std::string& appName)
{
    static const std::map<std::string, const char*> names = {
        {"gomoku", "五子棋"}, {"snake", "贪食蛇"},
        {"pacman", "吃豆豆"}, {"go", "围棋"},
        {"terminal", "终端"}, {"filemanager", "文件管理器"},
        {"chat", "聊天"},
    };
    auto it = names.find(appName);
    return it != names.end() ? it->second : appName.c_str();
}

static std::string displayAvatar(const std::string& avatar) {
    if (avatar.empty()) return "";
    if (avatar[0] == '/' || avatar.rfind("http", 0) == 0) return "🤖";
    return avatar;
}

struct ChatApp : std::enable_shared_from_this<ChatApp>
{
    std::vector<boost::json::object> history;
    std::vector<boost::json::object> pending_outputs;
    MessageQueue<std::string> input_queue;
    std::mutex mtx;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> streaming{false};
    int round = 0;
    int consecutive_tool_rounds = 0;

    app_output_fn output_cb = nullptr;
    void* output_udata = nullptr;
    void* io_ctx_ptr = nullptr;

    std::shared_ptr<LlmClient> current_stream;
    bool done = false;

    std::shared_ptr<ChatApp> self_holder;

    IModuleCache* mod_cache = nullptr;
    std::map<std::string, AppInstance> instances;

    std::string chatId;
    bool isGroupChat = true;

    std::vector<std::shared_ptr<agent::IAgentChat>> agents;

    std::string current_sender_name = "AI助手";
    std::string current_sender_avatar = "/res/C220748556D18ADBC61177B1A5A8151D.png";
    std::string user_display_name;

    void push_output(boost::json::value val)
    {
        if (val.is_object() && !current_sender_name.empty()) {
            auto& o = val.as_object();
            if (!o.contains("sender_name")) o["sender_name"] = current_sender_name;
            if (!o.contains("sender_avatar")) o["sender_avatar"] = current_sender_avatar;
        }
        bool is_end = false;
        if (val.is_object()) {
            auto& o = val.as_object();
            auto it = o.find("type");
            if (it != o.end() && it->value().is_string()) {
                auto t = it->value().as_string();
                if (t == "stream_start")
                    CHAT_LOG("[chat-out]", "stream_start (round " << round << ")");
                else if (t == "stream_end")
                    is_end = true;
                else if (t == "delta")
                    CHAT_LOG("[chat-out]", "delta (round " << round << ")");
                else if (t == "reasoning")
                    CHAT_LOG("[chat-out]", "reasoning (round " << round << ")");
            }
        }
        {
            app_output_fn cb = nullptr;
            void* udata = nullptr;
            std::string s;
            {
                std::lock_guard<std::mutex> lock(mtx);
                cb = output_cb;
                udata = output_udata;
                if (cb)
                    s = boost::json::serialize(val);
                else
                    pending_outputs.push_back(val.as_object());
            }
            if (cb)
                cb(udata, s.c_str());
        }
        if (is_end) {
            streaming = false;
            CHAT_LOG("[chat-state]", "idle (round " << round << ")");
            processNextInQueue(this);
        }
    }
};

// ---- API key ----

// ---- Command handling ----

static boost::json::array handleCommand(ChatApp* app, const std::string& text)
{
    std::string s = text.substr(1);
    size_t endPos = s.find_last_not_of(" \t");
    if (endPos != std::string::npos) s = s.substr(0, endPos + 1);
    size_t sp = s.find_first_of(" \t");
    std::string cmd = (sp == std::string::npos) ? s : s.substr(0, sp);
    std::string arg;
    if (sp != std::string::npos) {
        arg = s.substr(sp + 1);
        size_t first = arg.find_first_not_of(" \t");
        if (first != std::string::npos) arg = arg.substr(first);
    }

    boost::json::object embed;
    embed["type"] = "embed";

    if (cmd == "agent") {
        if (arg.empty() || arg == "list") {
            std::string agentList = "房间AI助手列表 (" + std::to_string(app->agents.size()) + "):\n";
            for (size_t i = 0; i < app->agents.size(); ++i) {
                auto& a = app->agents[i];
                agentList += std::to_string(i + 1) + ". " + displayAvatar(a->getAvatar()) + " " + a->getName() + "\n";
            }
            embed["kind"] = "text";
            embed["text"] = agentList;
        } else if (arg == "available") {
            std::string profileDir = std::string(PROJ_ROOT) + "/module/agent/config";
            auto agents = agent::loadAgentsFromDir(profileDir);
            std::string list = "可用AI助手配置:\n";
            for (auto& a : agents)
                list += "  " + displayAvatar(a->getAvatar()) + " " + a->getName() + "\n";
            embed["kind"] = "text";
            embed["text"] = list;
        } else if (arg.rfind("add ", 0) == 0) {
            std::string agentName = arg.substr(4);
            std::string profileDir = std::string(PROJ_ROOT) + "/module/agent/config";
            auto agents = agent::loadAgentsFromDir(profileDir);
            bool found = false;
            for (auto& a : agents) {
                if (a->getName() == agentName) {
                    if (app->io_ctx_ptr) a->setIoContext(app->io_ctx_ptr);
                    a->setToolExecutor([app](const std::string& name, const std::string& args) -> std::string {
                        return executeTool(app, name, args);
                    });
                    a->setStreamCallback([app](boost::json::object ev) {
                        if (app->cancelled) return;
                        app->push_output(std::move(ev));
                    });
                    a->setResponseCallback([app](boost::json::object msg) {
                        if (app->cancelled) return;
                        std::lock_guard<std::mutex> lock(app->mtx);
                        app->history.push_back(std::move(msg));
                    });
                    app->agents.push_back(std::move(a));
                    embed["kind"] = "text";
                    embed["text"] = "已添加AI助手: " + agentName;
                    found = true;
                    break;
                }
            }
            if (!found) {
                embed["kind"] = "text";
                embed["text"] = "未找到AI助手: " + agentName + "。可用: /agent available";
            }
        } else if (arg.rfind("remove ", 0) == 0) {
            std::string agentName = arg.substr(7);
            bool removed = false;
            for (auto it = app->agents.begin(); it != app->agents.end(); ++it) {
                if ((*it)->getName() == agentName) {
                    (*it)->stop();
                    app->agents.erase(it);
                    embed["kind"] = "text";
                    embed["text"] = "已移除AI助手: " + agentName;
                    removed = true;
                    break;
                }
            }
            if (!removed) {
                embed["kind"] = "text";
                embed["text"] = "未找到AI助手: " + agentName;
            }
        } else {
            embed["kind"] = "text";
            embed["text"] = "未知agent命令: /agent list, /agent add <name>, /agent remove <name>";
        }
        boost::json::array result;
        result.push_back(std::move(embed));
        return result;
    }

    if (s == "图片") {
        embed["kind"] = "image";
        embed["url"]  = "/res/C220748556D18ADBC61177B1A5A8151D.png";
        embed["title"]= "照片";
    } else if (s == "视频") {
        embed["kind"] = "video";
        embed["url"]  = "/res/result.mp4";
        embed["title"]= "视频";
    } else if (s == "音乐") {
        embed["kind"] = "audio";
        embed["url"]  = "/res/超级敏感.wav";
        embed["title"]= "超级敏感";
    } else if (cmd == "游戏" || s.rfind("游戏", 0) == 0) {
        std::string game;
        if (cmd == "游戏") game = arg;
        else if (s.size() > 6) game = s.substr(6);
        if (game.empty()) game = "snake";
        embed["kind"] = "game";
        embed["name"] = game;
        embed["url"]  = "/" + game;
        embed["title"]= "游戏：" + game;
    } else {
        embed["kind"] = "text";
        embed["text"] = "未知命令: /" + cmd;
    }

    boost::json::array result;
    result.push_back(std::move(embed));
    return result;
}

// ---- Poll support ----

static void drainAndPush(ChatApp* app)
{
    std::lock_guard<std::mutex> lock(app->mtx);
    for (auto& obj : app->pending_outputs)
        app->push_output(std::move(obj));
    app->pending_outputs.clear();
}

// ---- App instance management ----

static AppInstance* ensureAppInstance(ChatApp* app, const std::string& name);

// ---- Session-backed app_process ----
// 优先通过 SessionRegistry 查询用户在 WebSocket 上的真实游戏实例，
// 不存在时说明用户已关闭该应用 → 清理内部实例并返回空。
static std::string appProcessOnApp(ChatApp* app, const std::string& appName, const std::string& input, int instance = 0)
{
    auto sess = Config::instance().sessionRegistry().findSession(appName, instance);
    if (sess) {
        return sess->call_app_process(input);
    }
    // session 不存在 → 用户已手动关闭窗口, 清理内部实例
    app->instances.erase(appName);
    return "[]";
}

static AppInstance* ensureAppInstance(ChatApp* app, const std::string& name)
{
    // 如果已经有活跃 session, 不需要内部实例
    auto sess = Config::instance().sessionRegistry().findSession(name, 0);
    if (sess) return nullptr;

    auto it = app->instances.find(name);
    if (it != app->instances.end())
        return &it->second;
    if (!app->mod_cache) return nullptr;
    try {
        AppModule mod = app->mod_cache->load(name);
        if (!mod) return nullptr;
        AppPtr handle = mod.create("{}");
        if (!handle) return nullptr;
        char* initResult = mod.app_process(handle.get(),
            R"({"action":"new_game","width":20,"height":20})");
        if (mod.app_free_string)
            mod.app_free_string(initResult);
        AppInstance inst;
        inst.mod = mod;
        inst.handle = std::move(handle);
        inst.appName = name;
        auto& ref = app->instances[name];
        ref = std::move(inst);
        return &app->instances[name];
    } catch (...) { return nullptr; }
}

static std::string executeTool(ChatApp* app, const std::string& name, const std::string& argsJson) {
    try {
        auto args = boost::json::parse(argsJson);
        if (!args.is_object()) return R"({"success":false,"msg":"invalid args"})";
        auto& a = args.as_object();

        if (name == "open_app") {
            std::string appName = a["app"].as_string().c_str();
            auto sess = Config::instance().sessionRegistry().findSession(appName, 0);
            if (!sess) app->instances.erase(appName);
            ensureAppInstance(app, appName);
            boost::json::object agentMsg;
            agentMsg["type"] = "agent";
            agentMsg["action"] = "open_app";
            agentMsg["app"] = appName;
            if (a.contains("width")) agentMsg["width"] = a["width"];
            if (a.contains("height")) agentMsg["height"] = a["height"];
            app->push_output(std::move(agentMsg));
            return R"({"success":true,"msg":"opened )" + appName + "\"}";
        }
        if (name == "control_app") {
            std::string appName = a["app"].as_string().c_str();
            int instance = a.contains("instance") ? static_cast<int>(a["instance"].as_int64()) : 0;
            int value = -1;
            if (a.contains("coord")) {
                auto& coord = a["coord"].as_array();
                value = static_cast<int>(coord[0].as_int64()) * 20 + static_cast<int>(coord[1].as_int64());
            } else if (a.contains("value")) {
                value = static_cast<int>(a["value"].as_int64());
            }
            boost::json::object cmd;
            cmd["action"] = "tick";
            cmd["value"] = value;
            auto sess = Config::instance().sessionRegistry().findSession(appName, instance);
            std::string result;
            if (sess) result = sess->call_app_process_and_notify(boost::json::serialize(cmd));
            else app->instances.erase(appName);
            if (result.empty()) result = "[]";
            return R"({"success":true,"result":)" + result + "}";
        }
        if (name == "close_app") {
            std::string appName = a["app"].as_string().c_str();
            app->instances.erase(appName);
            boost::json::object agentMsg;
            agentMsg["type"] = "agent"; agentMsg["action"] = "close_app"; agentMsg["app"] = appName;
            app->push_output(std::move(agentMsg));
            return R"({"success":true,"msg":"closed )" + appName + "\"}";
        }
        if (name == "get_app_state") {
            std::string appName = a["app"].as_string().c_str();
            int instance = a.contains("instance") ? static_cast<int>(a["instance"].as_int64()) : 0;
            auto sess = Config::instance().sessionRegistry().findSession(appName, instance);
            if (sess) {
                std::string state = sess->call_app_process(R"({"action":"get_state"})");
                return R"({"success":true,"state":)" + state + "}";
            }
            return R"({"success":false,"msg":"no instance"})";
        }
        if (name == "list_apps" || name == "list_active_windows") {
            auto all = Config::instance().sessionRegistry().listSessions();
            std::map<std::string, int> counts;
            for (auto& [n, idx] : all) counts[n] = std::max(counts[n], idx + 1);
            boost::json::object info;
            info["total"] = static_cast<int64_t>(all.size());
            boost::json::array apps;
            for (auto& [n, cnt] : counts) {
                boost::json::object e; e["app"] = n; e["instances"] = cnt;
                apps.push_back(std::move(e));
            }
            info["apps"] = std::move(apps);
            return boost::json::serialize(info);
        }
        if (name == "chat_send") {
            std::string text = a["text"].as_string().c_str();
            if (a.contains("instance")) {
                int target = static_cast<int>(a["instance"].as_int64());
                auto sess = Config::instance().sessionRegistry().findSession("chat", target);
                if (sess) sess->call_app_process("{\"text\":\"" + text + "\"}");
            }
            return R"({"success":true})";
        }
        if (name == "file_list") {
            std::string path = a.contains("path") ? a["path"].as_string().c_str() : "/";
            boost::json::array entries;
            for (auto& entry : std::filesystem::directory_iterator(path)) {
                boost::json::object e;
                e["name"] = entry.path().filename().string();
                e["is_dir"] = entry.is_directory();
                entries.push_back(std::move(e));
            }
            boost::json::object r;
            r["success"] = true; r["path"] = path; r["entries"] = std::move(entries);
            return boost::json::serialize(r);
        }
        if (name == "file_read") {
            std::string path = a["path"].as_string().c_str();
            std::ifstream ifs(path);
            if (!ifs) return R"({"success":false})";
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            return R"({"success":true,"content":)" + boost::json::serialize(boost::json::string(content)) + "}";
        }
        if (name == "file_write") {
            std::string path = a["path"].as_string().c_str();
            std::ofstream ofs(path);
            if (!ofs) return R"({"success":false})";
            ofs << a["content"].as_string().c_str();
            return R"({"success":true})";
        }
        if (name == "file_mkdir") {
            std::filesystem::create_directories(a["path"].as_string().c_str());
            return R"({"success":true})";
        }
        if (name == "file_remove") {
            std::filesystem::remove(a["path"].as_string().c_str());
            return R"({"success":true})";
        }
        if (name == "terminal_exec") {
            std::string cmd = std::string(R"({"action":"exec_sync","command":")") + a["command"].as_string().c_str() + "\"}";
            auto result = appProcessOnApp(app, "terminal", cmd);
            return R"({"success":true,"result":)" + result + "}";
        }
        if (name == "terminal_stdin") {
            std::string cmd = std::string(R"({"action":"stdin","data":")") + a["data"].as_string().c_str() + "\"}";
            auto result = appProcessOnApp(app, "terminal", cmd);
            return R"({"success":true,"result":)" + result + "}";
        }
        return R"({"success":false,"msg":"unknown tool: )" + name + "\"}";
    } catch (...) {
        return R"({"success":false,"msg":"tool execution error"})";
    }
}

// ---- Forward declaration ----

static void doLlmCall(ChatApp* app, const std::string& body,
                      bool withTools,
                      std::function<void(std::string, std::string, std::vector<LlmToolCall>)> onDone);

// ---- Process tool calls from LLM ----

static void processToolCalls(ChatApp* app,
    const std::vector<LlmToolCall>& mergedList,
    const std::string& response,
    const std::string& reasoning)
{
    boost::json::object am;
    am["role"] = "assistant";
    if (!response.empty()) am["content"] = response;
    if (!reasoning.empty()) am["reasoning_content"] = reasoning;

    boost::json::array tcArr;
    for (auto& tc : mergedList) {
        boost::json::object tcObj;
        tcObj["id"] = tc.id;
        tcObj["type"] = "function";
        tcObj["function"] = {
            {"name", tc.function_name},
            {"arguments", tc.function_arguments}
        };
        tcArr.push_back(std::move(tcObj));
    }
    am["tool_calls"] = std::move(tcArr);

    {
        std::lock_guard<std::mutex> lock(app->mtx);
        app->history.push_back(std::move(am));
    }

    for (auto& tc : mergedList) {
        boost::json::object tr;
        tr["role"] = "tool";
        tr["tool_call_id"] = tc.id;
        tr["content"] = "{\"success\":true}";

        if (tc.function_name == "open_app") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string appName = args.as_object()["app"].as_string().c_str();
                // 检查是否有活跃 session, 没有则清理旧内部实例, 再创建新的
                auto sess = Config::instance().sessionRegistry().findSession(appName, 0);
                if (!sess) {
                    app->instances.erase(appName);
                }
                ensureAppInstance(app, appName);
                boost::json::object agentMsg;
                agentMsg["type"] = "agent";
                agentMsg["action"] = "open_app";
                agentMsg["app"] = appName;
                if (args.as_object().contains("width"))
                    agentMsg["width"] = args.as_object()["width"];
                if (args.as_object().contains("height"))
                    agentMsg["height"] = args.as_object()["height"];
                app->push_output(std::move(agentMsg));
                tr["content"] = "{\"success\":true,\"msg\":\"opened " + appName + "\"}";
            } catch (...) {
                tr["content"] = "{\"success\":false,\"msg\":\"failed to parse arguments\"}";
            }
        } else if (tc.function_name == "control_app") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string appName = args.as_object()["app"].as_string().c_str();
                int instance = 0;
                if (args.as_object().contains("instance"))
                    instance = args.as_object()["instance"].as_int64();
                int value = -1;
                if (args.as_object().contains("coord")) {
                    auto& coord = args.as_object()["coord"].as_array();
                    int row = coord[0].as_int64();
                    int col = coord[1].as_int64();
                    value = row * 20 + col;
                } else if (args.as_object().contains("value")) {
                    value = args.as_object()["value"].as_int64();
                }

                boost::json::object cmd;
                cmd["action"] = "tick";
                cmd["value"] = value;
                std::string cmdStr = boost::json::serialize(cmd);

                // 直接处理 tick 并推送状态到游戏 WebSocket 更新前端显示
                auto sess = Config::instance().sessionRegistry().findSession(appName, instance);
                std::string result;
                if (sess) {
                    result = sess->call_app_process_and_notify(cmdStr);
                } else {
                    // session 不存在 → 用户已关闭窗口, 清理内部实例
                    app->instances.erase(appName);
                }
                if (result.empty()) result = "[]";
                tr["content"] = "{\"success\":true,\"result\":" + result + "}";
            } catch (...) {
                tr["content"] = "{\"success\":false,\"msg\":\"failed to parse arguments\"}";
            }
        } else if (tc.function_name == "close_app") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string appName = args.as_object()["app"].as_string().c_str();
                app->instances.erase(appName);
                boost::json::object agentMsg;
                agentMsg["type"] = "agent";
                agentMsg["action"] = "close_app";
                agentMsg["app"] = appName;
                app->push_output(std::move(agentMsg));
                tr["content"] = "{\"success\":true,\"msg\":\"closed " + appName + "\"}";
            } catch (...) {
                tr["content"] = "{\"success\":false,\"msg\":\"failed to parse arguments\"}";
            }
        } else if (tc.function_name == "get_app_state") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string appName = args.as_object()["app"].as_string().c_str();
                int instance = 0;
                if (args.as_object().contains("instance"))
                    instance = args.as_object()["instance"].as_int64();
                auto sess = Config::instance().sessionRegistry().findSession(appName, instance);
                if (sess) {
                    std::string state = sess->call_app_process("{\"action\":\"get_state\"}");
                    tr["content"] = "{\"success\":true,\"state\":" + state + "}";
                } else {
                    tr["content"] = "{\"success\":false,\"msg\":\"no running instance " + std::to_string(instance) + " for " + appName + "\"}";
                }
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"error: " + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "list_apps" || tc.function_name == "list_active_windows") {
            auto all = Config::instance().sessionRegistry().listSessions();
            std::map<std::string, int> counts;
            for (auto& [name, idx] : all)
                counts[name] = std::max(counts[name], idx + 1);
            boost::json::object info;
            info["total"] = static_cast<int64_t>(all.size());
            info["count"] = static_cast<int64_t>(all.size());
            boost::json::array apps;
            for (auto& [name, cnt] : counts) {
                boost::json::object entry;
                entry["app"] = name;
                entry["instances"] = cnt;
                apps.push_back(std::move(entry));
            }
            info["apps"] = std::move(apps);
            tr["content"] = boost::json::serialize(info);
        } else if (tc.function_name == "chat_send") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string text = args.as_object()["text"].as_string().c_str();
                if (args.as_object().contains("instance")) {
                    int target = static_cast<int>(args.as_object()["instance"].as_int64());
                    auto targetSess = Config::instance().sessionRegistry().findSession("chat", target);
                    if (targetSess) {
                        std::string cmd = "{\"text\":\"" + text + "\"}";
                        targetSess->call_app_process(cmd);
                        tr["content"] = "{\"success\":true,\"msg\":\"sent to chat-" + std::to_string(target) + ": " + text + "\"}";
                    } else {
                        tr["content"] = "{\"success\":false,\"msg\":\"chat instance " + std::to_string(target) + " not found\"}";
                    }
                } else {
                    tr["content"] = "{\"success\":true,\"msg\":\"sent: " + text + "\"}";
                }
            } catch (...) {
                tr["content"] = "{\"success\":false,\"msg\":\"failed to parse arguments\"}";
            }
        } else if (tc.function_name == "file_list") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string path = "/";
                if (args.as_object().contains("path"))
                    path = args.as_object()["path"].as_string().c_str();
                namespace fs = std::filesystem;
                boost::json::array entries;
                int count = 0;
                for (auto& entry : fs::directory_iterator(path)) {
                    boost::json::object e;
                    e["name"] = entry.path().filename().string();
                    e["is_dir"] = entry.is_directory();
                    e["size"] = static_cast<int64_t>(entry.is_regular_file() ? fs::file_size(entry) : 0);
                    entries.push_back(std::move(e));
                    ++count;
                }
                boost::json::object result;
                result["success"] = true;
                result["path"] = path;
                result["count"] = count;
                result["entries"] = std::move(entries);
                tr["content"] = boost::json::serialize(result);
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "file_read") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string path = args.as_object()["path"].as_string().c_str();
                std::ifstream ifs(path);
                if (!ifs) {
                    tr["content"] = "{\"success\":false,\"msg\":\"cannot open file: " + path + "\"}";
                } else {
                    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                    boost::json::object result;
                    result["success"] = true;
                    result["path"] = path;
                    result["content"] = content;
                    result["size"] = static_cast<int64_t>(content.size());
                    tr["content"] = boost::json::serialize(result);
                }
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "file_write") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string path = args.as_object()["path"].as_string().c_str();
                std::string content = args.as_object()["content"].as_string().c_str();
                std::ofstream ofs(path);
                if (!ofs) {
                    tr["content"] = "{\"success\":false,\"msg\":\"cannot write file: " + path + "\"}";
                } else {
                    ofs << content;
                    tr["content"] = "{\"success\":true,\"msg\":\"wrote " + std::to_string(content.size()) + " bytes to " + path + "\"}";
                }
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "file_mkdir") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string path = args.as_object()["path"].as_string().c_str();
                std::filesystem::create_directories(path);
                tr["content"] = "{\"success\":true,\"msg\":\"created directory: " + path + "\"}";
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "file_remove") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string path = args.as_object()["path"].as_string().c_str();
                std::filesystem::remove(path);
                tr["content"] = "{\"success\":true,\"msg\":\"removed: " + path + "\"}";
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "terminal_exec") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string command = args.as_object()["command"].as_string().c_str();
                auto result = appProcessOnApp(app, "terminal", R"({"action":"exec_sync","command":")" + command + "\"}");
                tr["content"] = "{\"success\":true,\"result\":" + result + "}";
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else if (tc.function_name == "terminal_stdin") {
            try {
                auto args = boost::json::parse(tc.function_arguments);
                std::string data = args.as_object()["data"].as_string().c_str();
                auto result = appProcessOnApp(app, "terminal", R"({"action":"stdin","data":")" + data + "\"}");
                tr["content"] = "{\"success\":true,\"result\":" + result + "}";
            } catch (std::exception& e) {
                tr["content"] = "{\"success\":false,\"msg\":\"" + std::string(e.what()) + "\"}";
            }
        } else {
            tr["content"] = "{\"success\":false,\"msg\":\"unknown tool: " + tc.function_name + "\"}";
        }

        {
            std::lock_guard<std::mutex> lock(app->mtx);
            app->history.push_back(std::move(tr));
        }
    }

    // 继续LLM调用，让模型回应或做更多工具调用 (最多5轮)
    app->consecutive_tool_rounds++;
    if (app->consecutive_tool_rounds > 5) return;

    std::vector<boost::json::object> hc2;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        hc2 = app->history;
    }
    boost::json::array msgs2;
    for (auto& m : hc2) msgs2.push_back(m);
    std::string body2 = llm::build_chat_body(msgs2, "deepseek-v4-flash", true, false);

    boost::json::object start2;
    start2["type"] = "stream_start";
    app->push_output(std::move(start2));

    doLlmCall(app, body2, true, [app](std::string resp2, std::string reason2, std::vector<LlmToolCall> tcs) {
        if (app->cancelled) return;
        app->consecutive_tool_rounds = 0;
        if (!tcs.empty()) {
            auto merged = llm::merge_tool_calls(tcs);
            std::vector<LlmToolCall> mergedList;
            for (auto& kv : merged)
                mergedList.push_back(kv.second);
            processToolCalls(app, mergedList, resp2, reason2);
        } else {
            std::lock_guard<std::mutex> lock(app->mtx);
            boost::json::object am2;
            am2["role"] = "assistant";
            if (!resp2.empty()) am2["content"] = resp2;
            if (!reason2.empty()) am2["reasoning_content"] = reason2;
            if (!resp2.empty() || !reason2.empty())
                app->history.push_back(std::move(am2));
        }
    });
}

// ---- Process next queued message ----

static void processNextInQueue(ChatApp* app)
{
    if (app->cancelled) return;
    std::string next;
    if (!app->input_queue.try_pop(next)) {
        CHAT_LOG("[chat-q]", "queue empty, idle (round " << app->round << ")");
        return;
    }
    CHAT_LOG("[chat-q]", "dequeue pending message (round " << app->round
             << ", " << app->input_queue.size() << " remaining in queue)");
    app->streaming = true;
    ++app->round;
    handleUserMessageAsync(app, next);
}

static void handleUserMessageAsync(ChatApp* app, const std::string& text, const std::string& sender_name)
{
    boost::json::object user_msg;
    user_msg["role"] = "user";
    user_msg["sender_name"] = sender_name;
    user_msg["sender_avatar"] = "";
    user_msg["content"] = text;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        app->history.push_back(std::move(user_msg));
    }

    std::string targetAgent;
    std::string actualText = text;
    if (!text.empty() && text[0] == '@') {
        size_t sp = text.find(' ');
        if (sp != std::string::npos) {
            targetAgent = text.substr(1, sp - 1);
            actualText = text.substr(sp + 1);
            size_t first = actualText.find_first_not_of(" \t");
            if (first != std::string::npos) actualText = actualText.substr(first);
            else actualText.clear();
        }
    }

    if (!app->agents.empty()) {
        std::vector<boost::json::object> histCopy;
        {
            std::lock_guard<std::mutex> lock(app->mtx);
            histCopy = app->history;
        }
        if (!targetAgent.empty()) {
            for (auto& agent : app->agents) {
                if (agent->getName() == targetAgent) {
                    agent->onUserMessage(actualText, sender_name, histCopy);
                    break;
                }
            }
        } else {
            for (auto& agent : app->agents) {
                agent->onUserMessage(text, sender_name, histCopy);
            }
        }
    }

}

static std::string buildSystemMsg() {
    static std::string cached;
    if (cached.empty()) {
        YAML::Node config = YAML::LoadFile("module/agent/config/agent_prompt.yml");
        cached = config["system_prompt"].as<std::string>();
    }
    return cached;
}

static boost::json::array& chatToolDefs() {
    static boost::json::array tools;
    if (tools.empty())
        tools = agent::loadToolsFromYaml("module/agent/config/tools.yml");
    return tools;
}

static void doLlmCall(ChatApp* app, const std::string& body,
                      bool withTools,
                      std::function<void(std::string, std::string, std::vector<LlmToolCall>)> onDone)
{
    std::string finalBody = body;
    llm::inject_tools(finalBody, withTools, {}, chatToolDefs());

    std::string apiKey = Config::instance().deepSeekApiKey();
    if (apiKey.empty()) {
        boost::json::object end;
        end["type"] = "stream_end";
        end["msg"] = "missing deepseek_api_key in config.json";
        app->push_output(std::move(end));
        return;
    }

    asio::io_context* io = static_cast<asio::io_context*>(app->io_ctx_ptr);
    if (!io) {
        auto self = app->shared_from_this();
        std::thread t([self, finalBody = std::move(finalBody), apiKey, onDone = std::move(onDone)]() {
            asio::io_context io;
            auto client = std::make_shared<LlmClient>(io);
            std::string fullResponse;
            std::string fullReasoning;
            std::vector<LlmToolCall> toolCalls;

            client->start("api.deepseek.com", "443", finalBody, "Bearer " + apiKey,
                [&](LlmEvent ev) {
                    if (self->cancelled) { client->cancel(); return; }
                    if (!ev.tool_calls.empty())
                        toolCalls.insert(toolCalls.end(), ev.tool_calls.begin(), ev.tool_calls.end());
                    boost::json::object obj;
                    obj["type"] = ev.type;
                    if (ev.type == "delta" || ev.type == "reasoning")
                        obj["text"] = std::move(ev.text);
                    if (!ev.msg.empty())
                        obj["msg"] = std::move(ev.msg);
                    if (!ev.tool_calls.empty()) {
                        boost::json::array tcArr;
                        for (auto& tc : ev.tool_calls) {
                            boost::json::object tcObj;
                            tcObj["id"] = tc.id;
                            tcObj["function_name"] = tc.function_name;
                            tcObj["function_arguments"] = tc.function_arguments;
                            tcArr.push_back(std::move(tcObj));
                        }
                        obj["tool_calls"] = std::move(tcArr);
                    }
                    self->push_output(std::move(obj));
                },
                [&](std::string response, std::string reasoning, int, int) {
                    fullResponse = std::move(response);
                    fullReasoning = std::move(reasoning);
                });

            io.run();

            if (!self->cancelled)
                onDone(std::move(fullResponse), std::move(fullReasoning), std::move(toolCalls));
        });
        t.detach();
        return;
    }

    auto toolCalls = std::make_shared<std::vector<LlmToolCall>>();
    std::weak_ptr<ChatApp> weak_app = app->shared_from_this();

    auto push_event = [weak_app, toolCalls](LlmEvent ev) {
        auto self = weak_app.lock();
        if (!self) return;
        if (!ev.tool_calls.empty())
            toolCalls->insert(toolCalls->end(), ev.tool_calls.begin(), ev.tool_calls.end());
        boost::json::object obj;
        obj["type"] = ev.type;
        if (ev.type == "delta" || ev.type == "reasoning")
            obj["text"] = std::move(ev.text);
        if (!ev.msg.empty())
            obj["msg"] = std::move(ev.msg);
        if (!ev.tool_calls.empty()) {
            boost::json::array tcArr;
            for (auto& tc : ev.tool_calls) {
                boost::json::object tcObj;
                tcObj["id"] = tc.id;
                tcObj["function_name"] = tc.function_name;
                tcObj["function_arguments"] = tc.function_arguments;
                tcArr.push_back(std::move(tcObj));
            }
            obj["tool_calls"] = std::move(tcArr);
        }
        self->push_output(std::move(obj));
    };

    auto on_done = [weak_app, toolCalls, onDone = std::move(onDone)](std::string response, std::string reasoning, int, int) {
        auto self = weak_app.lock();
        if (!self || self->cancelled) return;
        onDone(std::move(response), std::move(reasoning), std::move(*toolCalls));
    };

    auto stream = std::make_shared<LlmClient>(*io);
    app->current_stream = stream;
    stream->start("api.deepseek.com", "443",
        finalBody, "Bearer " + apiKey,
        std::move(push_event), std::move(on_done));
}

// ============================================================
//  C ABI
// ============================================================

extern "C"
{

void* app_create(const char* config_json)
{
    (void)config_json;
    auto ptr = std::make_shared<ChatApp>();
    ptr->self_holder = ptr;

    auto cachePtr = Config::instance().chatCachePtr();
    if (cachePtr)
        ptr->mod_cache = reinterpret_cast<IModuleCache*>(static_cast<uintptr_t>(cachePtr));

    ptr->chatId = agent::AgentManager::instance().allocId();
    agent::AgentManager::instance().setChatType(ptr->chatId, agent::ChatType::GROUP);

    try {
        auto mainAi = agent::AgentManager::instance().createFromYaml(
            std::string(PROJ_ROOT) + "/module/agent/config/default.yml");
        ptr->current_sender_name = mainAi->getName();
        auto mainAiId = agent::AgentManager::instance().allocId();
        agent::AgentManager::instance().joinChat(mainAiId, ptr->chatId, agent::ChatType::GROUP);
        mainAi->setStreamCallback([raw = ptr.get()](boost::json::object ev) {
            if (raw->cancelled) return;
            raw->push_output(std::move(ev));
        });
        mainAi->setResponseCallback([raw = ptr.get()](boost::json::object msg) {
            if (raw->cancelled) return;
            std::lock_guard<std::mutex> lock(raw->mtx);
            raw->history.push_back(std::move(msg));
        });
        ptr->agents.push_back(std::move(mainAi));
    } catch (...) {}

    return ptr.get();
}

void app_destroy(void* p)
{
    auto* app = static_cast<ChatApp*>(p);
    app->cancelled = true;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        app->output_cb = nullptr;
        app->output_udata = nullptr;
    }
    if (app->current_stream)
        app->current_stream->cancel();
    app->current_stream.reset();
    app->instances.clear();
    app->self_holder.reset();
}

void app_set_output(void* p, app_output_fn cb, void* userdata)
{
    auto* app = static_cast<ChatApp*>(p);
    std::lock_guard<std::mutex> lock(app->mtx);
    app->output_cb = cb;
    app->output_udata = userdata;
}

void app_set_io_context(void* p, void* io_ctx)
{
    auto* app = static_cast<ChatApp*>(p);
    app->io_ctx_ptr = io_ctx;
}

void app_on_input(void* p, const char* input_json)
{
    auto* app = static_cast<ChatApp*>(p);
    try {
        auto val = boost::json::parse(input_json);
        if (!val.is_object()) return;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string())
        {
            auto action = action_it->value().as_string();
            if (action == "poll") {
                drainAndPush(app);
                return;
            }
            if (action == "stop") {
                CHAT_LOG("[chat-in]", "stop request");
                app->cancelled = true;
                if (app->current_stream)
                    app->current_stream->cancel();
                app->streaming = false;
                app->input_queue.clear();
                boost::json::object end;
                end["type"] = "stream_end";
                end["msg"] = "stopped";
                app->push_output(std::move(end));
                return;
            }
            if (action == "init") {
                auto name = obj.find("display_name");
                if (name != obj.end() && name->value().is_string())
                    app->user_display_name = name->value().as_string().c_str();
                return;
            }
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                CHAT_LOG("[chat-in]", "command: " << text);
                auto arr = handleCommand(app, text);
                for (auto& item : arr)
                    app->push_output(std::move(item));
            } else {
                CHAT_LOG("[chat-in]", "text: \"" << text.substr(0, 20)
                         << (text.size() > 20 ? "..." : "") << "\""
                         << " (streaming=" << app->streaming << ")");
                if (app->streaming) {
                    app->input_queue.push(text);
                    CHAT_LOG("[chat-q]", "queued message (queue size="
                             << app->input_queue.size() << ")");
                } else {
                    app->streaming = true;
                    ++app->round;
                    handleUserMessageAsync(app, text, app->user_display_name);
                }
            }
        }
    } catch (...) {}
}

char* app_process(void* p, const char* input_json)
{
    auto* app = static_cast<ChatApp*>(p);
    std::string msg(input_json);
    try {
        auto val = boost::json::parse(msg);
        if (!val.is_object()) goto done;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string() &&
            action_it->value().as_string() == "poll") {
            drainAndPush(app);
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                auto arr = handleCommand(app, text);
                for (auto& item : arr)
                    app->push_output(std::move(item));
            } else {
                if (app->streaming)
                    app->input_queue.push(text);
                else {
                    app->streaming = true;
                    ++app->round;
                    handleUserMessageAsync(app, text, app->user_display_name);
                }
            }
        }
    } catch (...) {}

done:
    std::string result = "[]";
    char* buf = static_cast<char*>(std::malloc(result.size() + 1));
    if (buf) std::memcpy(buf, result.data(), result.size() + 1);
    return buf;
}

void app_free_string(char* str)
{
    std::free(str);
}

int app_is_done(void* p)
{
    return static_cast<ChatApp*>(p)->done ? 1 : 0;
}

// ---- Test helpers ----

int app_queue_size(void* p)
{
    auto* app = static_cast<ChatApp*>(p);
    return static_cast<int>(app->input_queue.size());
}

int app_streaming(void* p)
{
    return static_cast<ChatApp*>(p)->streaming ? 1 : 0;
}

void app_test_set_streaming(void* p, int val)
{
    static_cast<ChatApp*>(p)->streaming = (val != 0);
}

void app_test_drain_queue(void* p)
{
    auto* app = static_cast<ChatApp*>(p);
    app->streaming = false;
    processNextInQueue(app);
}

} // extern "C"
