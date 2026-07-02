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
#include <utility>
#include <memory>
#include <chrono>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/json.hpp>

#include "llm_client.hpp"
#include "llm_utils.hpp"
#include "config.hpp"
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
static void handleUserMessageAsync(ChatApp* app, const std::string& text);

struct AppInstance
{
    AppModule mod;
    AppPtr handle;
    std::string appName;
};

struct ChatApp : std::enable_shared_from_this<ChatApp>
{
    std::vector<boost::json::object> history;
    std::vector<boost::json::object> pending_outputs;
    MessageQueue<std::string> input_queue;
    std::mutex mtx;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> streaming{false};
    int round = 0;

    app_output_fn output_cb = nullptr;
    void* output_udata = nullptr;
    void* io_ctx_ptr = nullptr;

    std::shared_ptr<LlmClient> current_stream;
    bool done = false;

    std::shared_ptr<ChatApp> self_holder;

    IModuleCache* mod_cache = nullptr;
    std::map<std::string, AppInstance> instances;

    void push_output(boost::json::value val)
    {
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

static boost::json::array handleCommand(const std::string& text)
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

// ---- Async LLM call via shared io_context ----

static void doLlmCall(ChatApp* app, const std::string& body,
                      bool withTools,
                      std::function<void(std::string, std::string, std::vector<LlmToolCall>)> onDone);

static void handleUserMessageAsync(ChatApp* app, const std::string& text)
{
    boost::json::object user_msg;
    user_msg["role"] = "user";
    user_msg["content"] = text;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        app->history.push_back(std::move(user_msg));
    }

    std::vector<boost::json::object> history_copy;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        history_copy = app->history;
    }

    boost::json::array msgs;
    for (const auto& m : history_copy) msgs.push_back(m);

    // 注入已打开 app 的状态到 LLM 上下文，使 agent 能感知 state
    if (!app->instances.empty()) {
        std::string stateSummary = "当前已打开的app状态:\n";
        for (auto& [name, inst] : app->instances) {
            char* raw = inst.mod.app_process(inst.handle.get(), "{\"action\":\"get_state\"}");
            std::string s(raw ? raw : "[]");
            if (inst.mod.app_free_string) inst.mod.app_free_string(raw);
            stateSummary += "- " + name + ": " + s + "\n";
        }
        boost::json::object sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = stateSummary;
        msgs.insert(msgs.begin(), std::move(sysMsg));
    }

    std::string body = llm::build_chat_body(msgs);
    doLlmCall(app, body, true, [app](std::string response, std::string reasoning, std::vector<LlmToolCall> toolCalls) {
        if (app->cancelled) return;

        if (!toolCalls.empty()) {
            auto merged = llm::merge_tool_calls(toolCalls);

            boost::json::object am;
            am["role"] = "assistant";
            if (!response.empty()) am["content"] = response;
            if (!reasoning.empty()) am["reasoning_content"] = reasoning;

            boost::json::array tcArr;
            std::vector<LlmToolCall> mergedList;
            for (auto& kv : merged) {
                mergedList.push_back(kv.second);
                boost::json::object tcObj;
                tcObj["id"] = kv.second.id;
                tcObj["type"] = "function";
                tcObj["function"] = {
                    {"name", kv.second.function_name},
                    {"arguments", kv.second.function_arguments}
                };
                tcArr.push_back(std::move(tcObj));
            }
            am["tool_calls"] = std::move(tcArr);

            {
                std::lock_guard<std::mutex> lock(app->mtx);
                app->history.push_back(std::move(am));
            }

            auto ensureAppInstance = [app](const std::string& name) -> AppInstance* {
                auto it = app->instances.find(name);
                if (it != app->instances.end())
                    return &it->second;
                if (!app->mod_cache) return nullptr;
                try {
                    AppModule mod = app->mod_cache->load(name);
                    if (!mod) return nullptr;
                    AppPtr handle = mod.create("{}");
                    if (!handle) return nullptr;
                    // 自动启动游戏/应用，使内部实例有状态可查
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
            };

            for (auto& tc : mergedList) {
                boost::json::object tr;
                tr["role"] = "tool";
                tr["tool_call_id"] = tc.id;
                tr["content"] = "{\"success\":true}";

                if (tc.function_name == "open_app") {
                    try {
                        auto args = boost::json::parse(tc.function_arguments);
                        std::string appName = args.as_object()["app"].as_string().c_str();
                        // 预加载模块到内部状态
                        ensureAppInstance(appName);
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
                        int value = args.as_object().contains("value")
                            ? args.as_object()["value"].as_int64() : -1;

                        // 仍发送前端消息 (保持 UI 同步)
                        boost::json::object agentMsg;
                        agentMsg["type"] = "agent";
                        agentMsg["action"] = "control_app";
                        agentMsg["app"] = appName;
                        agentMsg["value"] = value;
                        app->push_output(std::move(agentMsg));

                        // 直接调用 app_process 获取新状态
                        auto* inst = ensureAppInstance(appName);
                        if (inst) {
                            boost::json::object cmd;
                            cmd["action"] = "tick";
                            cmd["value"] = value;
                            std::string cmdStr = boost::json::serialize(cmd);
                            char* resultStr = inst->mod.app_process(
                                inst->handle.get(), cmdStr.c_str());
                            std::string result(resultStr ? resultStr : "[]");
                            if (inst->mod.app_free_string)
                                inst->mod.app_free_string(resultStr);
                            tr["content"] = "{\"success\":true,\"result\":" + result + "}";
                        } else {
                            tr["content"] = "{\"success\":true}";
                        }
                    } catch (...) {
                        tr["content"] = "{\"success\":false,\"msg\":\"failed to parse arguments\"}";
                    }
                } else if (tc.function_name == "close_app") {
                    try {
                        auto args = boost::json::parse(tc.function_arguments);
                        std::string appName = args.as_object()["app"].as_string().c_str();
                        // 清理内部实例
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
                        auto* inst = ensureAppInstance(appName);
                        if (!inst) {
                            tr["content"] = "{\"success\":false,\"msg\":\"failed to load " + appName + "\"}";
                        } else {
                            char* stateStr = inst->mod.app_process(
                                inst->handle.get(), "{\"action\":\"get_state\"}");
                            std::string state(stateStr ? stateStr : "[]");
                            if (inst->mod.app_free_string)
                                inst->mod.app_free_string(stateStr);
                            tr["content"] = "{\"success\":true,\"state\":" + state + "}";
                        }
                    } catch (std::exception& e) {
                        tr["content"] = "{\"success\":false,\"msg\":\"error: " + std::string(e.what()) + "\"}";
                    }
                } else {
                    tr["content"] = "{\"success\":false,\"msg\":\"unknown tool: " + tc.function_name + "\"}";
                }

                {
                    std::lock_guard<std::mutex> lock(app->mtx);
                    app->history.push_back(std::move(tr));
                }
            }

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

            doLlmCall(app, body2, false, [app](std::string resp2, std::string reason2, std::vector<LlmToolCall>) {
                if (app->cancelled) return;
                {
                    std::lock_guard<std::mutex> lock(app->mtx);
                    boost::json::object am2;
                    am2["role"] = "assistant";
                    if (!resp2.empty()) am2["content"] = resp2;
                    if (!reason2.empty()) am2["reasoning_content"] = reason2;
                    if (!resp2.empty() || !reason2.empty())
                        app->history.push_back(std::move(am2));
                }
            });
        } else {
            {
                std::lock_guard<std::mutex> lock(app->mtx);
                boost::json::object am;
                am["role"] = "assistant";
                if (!response.empty()) am["content"] = response;
                if (!reasoning.empty()) am["reasoning_content"] = reasoning;
                if (!response.empty() || !reasoning.empty())
                    app->history.push_back(std::move(am));
            }
        }
    });

    boost::json::object start;
    start["type"] = "stream_start";
    app->push_output(std::move(start));
}

static void doLlmCall(ChatApp* app, const std::string& body,
                      bool withTools,
                      std::function<void(std::string, std::string, std::vector<LlmToolCall>)> onDone)
{
    std::string finalBody = body;
    llm::inject_tools(finalBody, withTools);

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
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                CHAT_LOG("[chat-in]", "command: " << text);
                auto arr = handleCommand(text);
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
                    handleUserMessageAsync(app, text);
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
                auto arr = handleCommand(text);
                for (auto& item : arr)
                    app->push_output(std::move(item));
            } else {
                if (app->streaming)
                    app->input_queue.push(text);
                else {
                    app->streaming = true;
                    ++app->round;
                    handleUserMessageAsync(app, text);
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
