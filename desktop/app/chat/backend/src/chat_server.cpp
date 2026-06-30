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
#include <utility>
#include <memory>
#include <chrono>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/json.hpp>

#include "llm_client.hpp"
#include "llm_utils.hpp"
#include "config.hpp"

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

struct ChatApp : std::enable_shared_from_this<ChatApp>
{
    std::vector<boost::json::object> history;
    std::vector<boost::json::object> pending_outputs; // legacy
    MessageQueue<std::string> input_queue;            // pending messages while streaming
    std::mutex mtx;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> streaming{false};               // true while DeepSeek request in flight
    std::thread stream_thread;                        // legacy background thread
    int round = 0;                                    // message round counter for logging

    // Async output
    app_output_fn output_cb = nullptr;
    void* output_udata = nullptr;

    // Async I/O context (for async HTTP)
    void* io_ctx_ptr = nullptr;

    // Current async LLM stream (shared_ptr, cancelled on destroy)
    std::shared_ptr<LlmClient> current_stream;

    bool done = false;

    // Self-holder to manage lifetime: app_destroy releases this,
    // but actual deletion waits until all callbacks release their weak_ptr.
    std::shared_ptr<ChatApp> self_holder;

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
        if (is_end) {
            CHAT_LOG("[chat-out]", "stream_end (round " << round << ")");
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
// L1: replaced duplicated ifstream+parse with Config::instance() singleton

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

// ---- Legacy sync support ----

static void drainAndPush(ChatApp* app)
{
    std::lock_guard<std::mutex> lock(app->mtx);
    for (auto& obj : app->pending_outputs)
        app->push_output(std::move(obj));
    app->pending_outputs.clear();
}

static boost::json::array drainPollArray(ChatApp* app)
{
    std::lock_guard<std::mutex> lock(app->mtx);
    boost::json::array arr;
    for (auto& obj : app->pending_outputs)
        arr.push_back(std::move(obj));
    app->pending_outputs.clear();
    return arr;
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

// ---- Async path: zero-thread DeepSeek via io_context ----

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

    std::string body = llm::build_chat_body(msgs);

    std::string apiKey = Config::instance().deepSeekApiKey();
    if (apiKey.empty()) {
        boost::json::object end;
        end["type"] = "stream_end";
        end["msg"]  = "missing deepseek_api_key in config.json";
        app->push_output(std::move(end));
        return;
    }

    asio::io_context* io = static_cast<asio::io_context*>(app->io_ctx_ptr);
    if (!io) {
        // Fallback: LlmClient on a local io_context in a background thread
        auto self = app->shared_from_this();
        std::thread t([self, body = std::move(body), apiKey]() {
            asio::io_context io;
            auto client = std::make_shared<LlmClient>(io);
            std::string full_response;

            client->start("api.deepseek.com", "443", body, "Bearer " + apiKey,
                [&](LlmEvent ev) {
                    if (self->cancelled) { client->cancel(); return; }
                    boost::json::object obj;
                    obj["type"] = ev.type;
                    if (ev.type == "delta" || ev.type == "reasoning")
                        obj["text"] = std::move(ev.text);
                    if (!ev.msg.empty())
                        obj["msg"] = std::move(ev.msg);
                    self->push_output(std::move(obj));
                },
                [&](std::string response, std::string, int, int) {
                    full_response = std::move(response);
                });

            io.run();

            if (!self->cancelled && !full_response.empty()) {
                std::lock_guard<std::mutex> lock(self->mtx);
                if (self->cancelled) return;
                boost::json::object am;
                am["role"] = "assistant";
                am["content"] = std::move(full_response);
                self->history.push_back(std::move(am));
            }
        });
        {
            std::lock_guard<std::mutex> lock(app->mtx);
            if (app->stream_thread.joinable()) app->stream_thread.detach();
            app->stream_thread = std::move(t);
        }
    } else {
        // Async HTTP on io_context — route all output through push_output
        // for consistent logging and end-of-stream queue draining
        std::weak_ptr<ChatApp> weak_app = app->shared_from_this();
        auto push_event = [weak_app](LlmEvent ev) {
            auto self = weak_app.lock();
            if (!self) return;
            boost::json::object obj;
            obj["type"] = ev.type;
            if (ev.type == "delta" || ev.type == "reasoning")
                obj["text"] = std::move(ev.text);
            if (!ev.msg.empty())
                obj["msg"] = std::move(ev.msg);
            self->push_output(std::move(obj));
        };

        std::weak_ptr<ChatApp> weak_self = app->shared_from_this();
        auto on_done = [weak_self](std::string response, std::string, int, int) {
            auto self = weak_self.lock();
            if (!self) return;
            if (self->cancelled) return;
            std::lock_guard<std::mutex> lock(self->mtx);
            if (!response.empty()) {
                boost::json::object am;
                am["role"] = "assistant";
                am["content"] = std::move(response);
                self->history.push_back(std::move(am));
            }
        };

        auto stream = std::make_shared<LlmClient>(*io);
        app->current_stream = stream;
        stream->start("api.deepseek.com", "443",
            body, "Bearer " + apiKey,
            std::move(push_event), std::move(on_done));
    }

    // Push stream_start immediately
    boost::json::object start;
    start["type"] = "stream_start";
    app->push_output(std::move(start));
}

// ---- Legacy path for app_process ----

static boost::json::array handleUserMessageLegacy(ChatApp* app, const std::string& text)
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

    auto self = app->shared_from_this();
    std::thread t([self, history_copy]() {
        std::string apiKey = Config::instance().deepSeekApiKey();
        if (apiKey.empty()) {
            boost::json::object end;
            end["type"] = "stream_end";
            end["msg"]  = "missing deepseek_api_key in config.json";
            if (!self->cancelled) self->push_output(std::move(end));
            return;
        }

        boost::json::array msgs;
        for (const auto& m : history_copy) msgs.push_back(m);
        std::string body = llm::build_chat_body(msgs);

        asio::io_context io;
        auto client = std::make_shared<LlmClient>(io);
        std::string full_response;

        client->start("api.deepseek.com", "443", body, "Bearer " + apiKey,
            [&](LlmEvent ev) {
                if (self->cancelled) { client->cancel(); return; }
                boost::json::object obj;
                obj["type"] = ev.type;
                if (ev.type == "delta" || ev.type == "reasoning")
                    obj["text"] = std::move(ev.text);
                if (!ev.msg.empty())
                    obj["msg"] = std::move(ev.msg);
                self->push_output(std::move(obj));
            },
            [&](std::string response, std::string, int, int) {
                full_response = std::move(response);
            });

        io.run();

        if (!self->cancelled && !full_response.empty()) {
            std::lock_guard<std::mutex> lock(self->mtx);
            if (self->cancelled) return;
            boost::json::object am;
            am["role"] = "assistant";
            am["content"] = std::move(full_response);
            self->history.push_back(std::move(am));
        }
    });
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        if (app->stream_thread.joinable()) app->stream_thread.detach();
        app->stream_thread = std::move(t);
    }

    boost::json::array result;
    boost::json::object start;
    start["type"] = "stream_start";
    result.push_back(std::move(start));
    return result;
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
    if (app->stream_thread.joinable())
        app->stream_thread.detach();
    app->current_stream.reset();
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
        if (action_it != obj.end() && action_it->value().is_string() &&
            action_it->value().as_string() == "poll") {
            drainAndPush(app);
            return;
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
    boost::json::array outputs;
    std::string msg(input_json);
    try {
        auto val = boost::json::parse(msg);
        if (!val.is_object()) goto done;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string() &&
            action_it->value().as_string() == "poll") {
            outputs = drainPollArray(app);
            goto done;
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                outputs = handleCommand(text);
            } else {
                outputs = handleUserMessageLegacy(app, text);
            }
        }
    } catch (...) {}

done:
    std::string result = boost::json::serialize(outputs);
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

// ---- Test helpers (for unit test queue-drain verification) ----

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
