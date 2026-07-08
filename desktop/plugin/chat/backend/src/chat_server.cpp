#include "chat_server.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <ctime>

#include <filesystem>
#include <fstream>

#include "agent_chat_api.hpp"
#include "agent_manager.hpp"
#include "config.hpp"
#include "ws_server.hpp"
#include "plugin_cache.hpp"
#include "logger.hpp"

namespace {

Logger& chatLog()
{
    static Logger ls(std::cerr, Logger::INFO);
    return ls;
}

} // namespace

#define CHAT_LOG(tag, msg)  do { auto _ls = chatLog().debug(tag); _ls << msg; } while(0)
#define CHAT_INFO(tag, msg) do { auto _ls = chatLog().info(tag);  _ls << msg; } while(0)

static void processNextInQueue(ChatPlugin* plugin);
static void handleUserMessageAsync(ChatPlugin* plugin, const std::string& text, const std::string& sender_name = "用户");
static std::string executeTool(ChatPlugin* plugin, const std::string& name, const std::string& argsJson);

static std::string displayAvatar(const std::string& avatar) {
    if (avatar.empty()) return "";
    if (avatar[0] == '/' || avatar.rfind("http", 0) == 0) return "🤖";
    return avatar;
}

void ChatPlugin::push_output(boost::json::value val)
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
                    CHAT_INFO("[chat-out]", "stream_start (round " << round << ")");
                else if (t == "stream_end")
                    is_end = true;
                else if (t == "delta")
                    CHAT_INFO("[chat-out]", "delta (round " << round << ")");
                else if (t == "reasoning")
                    CHAT_INFO("[chat-out]", "reasoning (round " << round << ")");
            }
        }
        {
            plugin_output_fn cb = nullptr;
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

// ---- Command handling ----

static boost::json::array handleCommand(ChatPlugin* plugin, const std::string& text)
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
            std::string agentList = "房间AI助手列表 (" + std::to_string(plugin->agents.size()) + "):\n";
            for (size_t i = 0; i < plugin->agents.size(); ++i) {
                auto& a = plugin->agents[i];
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
                    if (plugin->io_ctx_ptr) a->setIoContext(plugin->io_ctx_ptr);
                    a->setToolExecutor([plugin](const std::string& name, const std::string& args) -> std::string {
                        return executeTool(plugin, name, args);
                    });
                    a->setStreamCallback([plugin](boost::json::object ev) {
                        if (plugin->cancelled) return;
                        plugin->push_output(std::move(ev));
                    });
                    a->setResponseCallback([plugin](boost::json::object msg) {
                        if (plugin->cancelled) return;
                        std::lock_guard<std::mutex> lock(plugin->mtx);
                        plugin->history.push_back(std::move(msg));
                    });
                    plugin->agents.push_back(std::move(a));
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
            for (auto it = plugin->agents.begin(); it != plugin->agents.end(); ++it) {
                if ((*it)->getName() == agentName) {
                    (*it)->stop();
                    plugin->agents.erase(it);
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

static void drainAndPush(ChatPlugin* plugin)
{
    std::lock_guard<std::mutex> lock(plugin->mtx);
    for (auto& obj : plugin->pending_outputs)
        plugin->push_output(std::move(obj));
    plugin->pending_outputs.clear();
}

// ---- Session-backed plugin_process ----
// 优先通过 SessionRegistry 查询用户在 WebSocket 上的真实游戏实例，
// 不存在时说明用户已关闭该应用 → 清理内部实例并返回空。
static std::string pluginProcessOnPlugin(ChatPlugin* plugin, const std::string& pluginName, const std::string& input, int instance = 0)
{
    auto sess = SessionManager::instance().findSession(pluginName, instance);
    if (sess) {
        return sess->call_plugin_process(input);
    }
    // session 不存在 → 用户已手动关闭窗口, 清理内部实例
    plugin->instances.erase(pluginName);
    return "[]";
}

// ---- Plugin instance management ----
static PluginInstance* ensurePluginInstance(ChatPlugin* plugin, const std::string& name)
{
    auto sess = SessionManager::instance().findSession(name, 0);
    if (sess) return nullptr;

    auto it = plugin->instances.find(name);
    if (it != plugin->instances.end())
        return &it->second;
    if (!plugin->mod_cache) return nullptr;
    try {
        auto mod = plugin->mod_cache->load(name);
        if (!mod) return nullptr;
        auto inst = mod.createInstance("{}");
        if (!inst) return nullptr;
        inst.process(R"({"action":"new_game","width":20,"height":20})");
        auto& ref = plugin->instances[name];
        ref = std::move(inst);
        return &plugin->instances[name];
    } catch (...) { return nullptr; }
}

static std::string executeTool(ChatPlugin* plugin, const std::string& name, const std::string& argsJson) {
    try {
        auto args = boost::json::parse(argsJson);
        if (!args.is_object()) return R"({"success":false,"msg":"invalid args"})";
        auto& a = args.as_object();

        if (name == "open_plugin") {
            std::string pluginName = a["plugin"].as_string().c_str();
            auto sess = SessionManager::instance().findSession(pluginName, 0);
            if (!sess) plugin->instances.erase(pluginName);
            ensurePluginInstance(plugin, pluginName);
            boost::json::object agentMsg;
            agentMsg["type"] = "agent";
            agentMsg["action"] = "open_plugin";
            agentMsg["plugin"] = pluginName;
            if (a.contains("width")) agentMsg["width"] = a["width"];
            if (a.contains("height")) agentMsg["height"] = a["height"];
            plugin->push_output(std::move(agentMsg));
            return R"({"success":true,"msg":"opened )" + pluginName + "\"}";
        }
        if (name == "control_plugin") {
            std::string pluginName = a["plugin"].as_string().c_str();
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
            auto sess = SessionManager::instance().findSession(pluginName, instance);
            if (!sess) {
                plugin->instances.erase(pluginName);
                return R"({"success":false,"msg":"no active session for )" + pluginName + R"(. The plugin may still be starting. Use list_active_windows to verify."})";
            }
            std::string result = sess->call_plugin_process_and_notify(boost::json::serialize(cmd));
            if (result.empty()) result = "[]";
            return R"({"success":true,"result":)" + result + "}";
        }
        if (name == "close_plugin") {
            std::string pluginName = a["plugin"].as_string().c_str();
            plugin->instances.erase(pluginName);
            boost::json::object agentMsg;
            agentMsg["type"] = "agent"; agentMsg["action"] = "close_plugin"; agentMsg["plugin"] = pluginName;
            plugin->push_output(std::move(agentMsg));
            return R"({"success":true,"msg":"closed )" + pluginName + "\"}";
        }
        if (name == "get_plugin_state") {
            std::string pluginName = a["plugin"].as_string().c_str();
            int instance = a.contains("instance") ? static_cast<int>(a["instance"].as_int64()) : 0;
            auto sess = SessionManager::instance().findSession(pluginName, instance);
            if (sess) {
                std::string state = sess->call_plugin_process(R"({"action":"get_state"})");
                return R"({"success":true,"state":)" + state + "}";
            }
            return R"({"success":false,"msg":"no instance"})";
        }
        if (name == "list_plugins" || name == "list_active_windows") {
            auto all = SessionManager::instance().listSessions();
            std::map<std::string, int> counts;
            for (auto& [n, idx] : all) counts[n] = std::max(counts[n], idx + 1);
            boost::json::object info;
            info["total"] = static_cast<int64_t>(all.size());
            boost::json::array plugins;
            for (auto& [n, cnt] : counts) {
                boost::json::object e; e["plugin"] = n; e["instances"] = cnt;
                plugins.push_back(std::move(e));
            }
            info["plugins"] = std::move(plugins);
            return boost::json::serialize(info);
        }
        if (name == "chat_send") {
            std::string text = a["text"].as_string().c_str();
            if (a.contains("instance")) {
                int target = static_cast<int>(a["instance"].as_int64());
                auto sess = SessionManager::instance().findSession("chat", target);
                if (sess) sess->call_plugin_process("{\"text\":\"" + text + "\"}");
            }
            return R"({"success":true})";
        }
        if (name == "file_list") {
            std::string path = a.contains("path") ? a["path"].as_string().c_str() : Config::instance().fileRoot();
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
            auto result = pluginProcessOnPlugin(plugin, "terminal", cmd);
            return R"({"success":true,"result":)" + result + "}";
        }
        if (name == "terminal_stdin") {
            std::string cmd = std::string(R"({"action":"stdin","data":")") + a["data"].as_string().c_str() + "\"}";
            auto result = pluginProcessOnPlugin(plugin, "terminal", cmd);
            return R"({"success":true,"result":)" + result + "}";
        }
        return R"({"success":false,"msg":"unknown tool: )" + name + "\"}";
    } catch (...) {
        return R"({"success":false,"msg":"tool execution error"})";
    }
}

// ---- Process next queued message ----

static void processNextInQueue(ChatPlugin* plugin)
{
    if (plugin->cancelled) return;
    std::string next;
    if (!plugin->input_queue.try_pop(next)) {
        CHAT_LOG("[chat-q]", "queue empty, idle (round " << plugin->round << ")");
        return;
    }
    CHAT_LOG("[chat-q]", "dequeue pending message (round " << plugin->round
             << ", " << plugin->input_queue.size() << " remaining in queue)");
    plugin->streaming = true;
    ++plugin->round;
    handleUserMessageAsync(plugin, next);
}

static void handleUserMessageAsync(ChatPlugin* plugin, const std::string& text, const std::string& sender_name)
{
    boost::json::object user_msg;
    user_msg["role"] = "user";
    user_msg["sender_name"] = sender_name;
    user_msg["sender_avatar"] = "";
    user_msg["content"] = text;
    {
        std::lock_guard<std::mutex> lock(plugin->mtx);
        plugin->history.push_back(std::move(user_msg));
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

    if (!plugin->agents.empty()) {
        auto idx = std::make_shared<int>(0);
        auto round = std::make_shared<int>(0);
        auto oldCallbacks = std::make_shared<std::vector<agent::IAgentChat::ResponseCallback>>();
        for (auto& ag : plugin->agents) oldCallbacks->push_back(nullptr);

        std::string msgText = text;
        std::string msgSender = sender_name;
        std::string msgTarget = targetAgent;
        std::string msgActual = actualText;
        bool autoLoop = targetAgent.empty() && plugin->agents.size() > 1;

        auto processNext = std::make_shared<std::function<void()>>();
        *processNext = [plugin, idx, round, msgText, msgSender, msgTarget, msgActual, oldCallbacks, processNext, autoLoop]() {
            if (plugin->cancelled) return;
            int i = (*idx)++;

            if (i >= static_cast<int>(plugin->agents.size())) {
                if (autoLoop && (*round)++ < 10) {
                    *idx = 0;
                    i = (*idx)++;
                } else {
                    for (size_t j = 0; j < plugin->agents.size(); ++j)
                        if ((*oldCallbacks)[j])
                            plugin->agents[j]->setResponseCallback(std::move((*oldCallbacks)[j]));
                    return;
                }
            }

            std::vector<boost::json::object> h;
            {
                std::lock_guard<std::mutex> lock(plugin->mtx);
                h = plugin->history;
            }

            if (!msgTarget.empty()) {
                if (plugin->agents[i]->getName() != msgTarget) { (*processNext)(); return; }
                plugin->agents[i]->onUserMessage(msgActual, msgSender, h);
            } else {
                std::string prompt = msgText;
                if (autoLoop && (*round) > 0) {
                    prompt = "轮到你了，请继续对话";
                }
                plugin->agents[i]->onUserMessage(prompt, msgSender, h);
            }

            (*oldCallbacks)[i] = [plugin, processNext](boost::json::object msg) {
                if (!plugin->cancelled) {
                    std::lock_guard<std::mutex> lock(plugin->mtx);
                    plugin->history.push_back(std::move(msg));
                }
                (*processNext)();
            };
            plugin->agents[i]->setResponseCallback((*oldCallbacks)[i]);
        };
        (*processNext)();
    }

}

// ============================================================
//  C ABI
// ============================================================

extern "C"
{

void* plugin_create(const char* config_json)
{
    (void)config_json;
    auto ptr = std::make_shared<ChatPlugin>();
    ptr->self_holder = ptr;

    ptr->mod_cache = &PluginCache::instance();

    ptr->chatId = agent::AgentManager::instance().allocId();
    agent::AgentManager::instance().setChatType(ptr->chatId, agent::ChatType::GROUP);

    try {
        auto mainAi = agent::AgentManager::instance().createFromYaml(
            std::string(PROJ_ROOT) + "/module/agent/config/default.yml");
        ptr->current_sender_name = mainAi->getName();
        auto mainAiId = agent::AgentManager::instance().allocId();
        agent::AgentManager::instance().joinChat(mainAiId, ptr->chatId, agent::ChatType::GROUP);
        mainAi->setToolExecutor([raw = ptr.get()](const std::string& name, const std::string& args) -> std::string {
            return executeTool(raw, name, args);
        });
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

void plugin_destroy(void* p)
{
    auto* plugin = static_cast<ChatPlugin*>(p);
    plugin->cancelled = true;
    {
        std::lock_guard<std::mutex> lock(plugin->mtx);
        plugin->output_cb = nullptr;
        plugin->output_udata = nullptr;
    }
    plugin->instances.clear();
    plugin->self_holder.reset();
}

void plugin_set_output(void* p, plugin_output_fn cb, void* userdata)
{
    auto* plugin = static_cast<ChatPlugin*>(p);
    std::lock_guard<std::mutex> lock(plugin->mtx);
    plugin->output_cb = cb;
    plugin->output_udata = userdata;
}

void plugin_set_io_context(void* p, void* io_ctx)
{
    auto* plugin = static_cast<ChatPlugin*>(p);
    plugin->io_ctx_ptr = io_ctx;
}

void plugin_on_input(void* p, const char* input_json)
{
    auto* plugin = static_cast<ChatPlugin*>(p);
    try {
        auto val = boost::json::parse(input_json);
        if (!val.is_object()) return;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string())
        {
            auto action = action_it->value().as_string();
            if (action == "poll") {
                drainAndPush(plugin);
                return;
            }
            if (action == "stop") {
                CHAT_INFO("[chat-in]", "stop request");
                plugin->cancelled = true;
                plugin->streaming = false;
                plugin->input_queue.clear();
                boost::json::object end;
                end["type"] = "stream_end";
                end["msg"] = "stopped";
                plugin->push_output(std::move(end));
                return;
            }
            if (action == "init") {
                auto name = obj.find("display_name");
                if (name != obj.end() && name->value().is_string())
                    plugin->user_display_name = name->value().as_string().c_str();
                return;
            }
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                CHAT_INFO("[chat-in]", "command: " << text);
                auto arr = handleCommand(plugin, text);
                for (auto& item : arr)
                    plugin->push_output(std::move(item));
            } else {
                CHAT_INFO("[chat-in]", "text: \"" << text.substr(0, 20)
                         << (text.size() > 20 ? "..." : "") << "\""
                         << " (streaming=" << plugin->streaming << ")");
                if (plugin->streaming) {
                    plugin->input_queue.push(text);
                    CHAT_LOG("[chat-q]", "queued message (queue size="
                             << plugin->input_queue.size() << ")");
                } else {
                    plugin->streaming = true;
                    ++plugin->round;
                    handleUserMessageAsync(plugin, text, plugin->user_display_name);
                }
            }
        }
    } catch (...) {}
}

char* plugin_process(void* p, const char* input_json)
{
    auto* plugin = static_cast<ChatPlugin*>(p);
    std::string msg(input_json);
    try {
        auto val = boost::json::parse(msg);
        if (!val.is_object()) goto done;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string() &&
            action_it->value().as_string() == "poll") {
            drainAndPush(plugin);
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                auto arr = handleCommand(plugin, text);
                for (auto& item : arr)
                    plugin->push_output(std::move(item));
            } else {
                if (plugin->streaming)
                    plugin->input_queue.push(text);
                else {
                    plugin->streaming = true;
                    ++plugin->round;
                    handleUserMessageAsync(plugin, text, plugin->user_display_name);
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

void plugin_free_string(char* str)
{
    std::free(str);
}

int plugin_is_done(void* p)
{
    return static_cast<ChatPlugin*>(p)->done ? 1 : 0;
}

char* plugin_get_info(void)
{
    const char* json = "{\"name\":\"chat\",\"display_name\":\"聊天\",\"type\":\"chat\"}";
    char* buf = (char*)malloc(strlen(json) + 1);
    if (buf) memcpy(buf, json, strlen(json) + 1);
    return buf;
}

} // extern "C"
