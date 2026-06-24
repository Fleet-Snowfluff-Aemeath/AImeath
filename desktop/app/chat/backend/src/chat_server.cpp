#include <app_api.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <sstream>
#include <fstream>
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
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "netconn.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ssl   = asio::ssl;
using tcp = asio::ip::tcp;

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
    std::deque<std::string> input_queue;              // pending messages while streaming
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

    // Current async HTTP stream (shared_ptr, cancelled on destroy)
    struct AsyncDeepSeek;
    std::shared_ptr<AsyncDeepSeek> current_stream;

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
        if (output_cb) {
            std::string s = boost::json::serialize(val);
            output_cb(output_udata, s.c_str());
        } else {
            std::lock_guard<std::mutex> lock(mtx);
            pending_outputs.push_back(val.as_object());
        }
        if (is_end) {
            streaming = false;
            CHAT_LOG("[chat-state]", "idle (round " << round << ")");
            processNextInQueue(this);
        }
    }
};

// ---- Async HTTPS DeepSeek stream (zero threads) ----

struct ChatApp::AsyncDeepSeek
    : public std::enable_shared_from_this<ChatApp::AsyncDeepSeek>
{
    using PushFn = std::function<void(boost::json::object)>;
    using DoneFn = std::function<void(std::string, std::string)>;
    static constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(120);

    AsyncDeepSeek(asio::io_context& ioc)
        : io_(ioc), resolver_(ioc)
        , stream_(std::make_unique<ssl::stream<beast::tcp_stream>>(ioc, chat_ssl()))
        , timer_(ioc)
    {}

    void start(const std::string& host, const std::string& port,
               const std::string& body, const std::string& auth,
               PushFn push, DoneFn done)
    {
        push_ = std::move(push);
        done_ = std::move(done);
        body_ = body;
        auth_ = auth;
        host_ = host;
        port_ = port;

        auto self = shared_from_this();
        timer_.expires_after(REQUEST_TIMEOUT);
        timer_.async_wait([self](beast::error_code ec) {
            if (ec == asio::error::operation_aborted) return;
            self->finish_error("request timeout after " +
                std::to_string(REQUEST_TIMEOUT.count()) + "s");
        });

        do_resolve();
    }

    void cancel()
    {
        cancelled_ = true;
        timer_.cancel();
        resolver_.cancel();
        beast::error_code ec;
        stream_->lowest_layer().cancel(ec);
    }

private:
    static ssl::context& chat_ssl()
    {
        static ssl::context ctx(ssl::context::tlsv12_client);
        return ctx;
    }

    void stop_timer()
    {
        timer_.cancel();
    }

    asio::io_context& io_;
    tcp::resolver resolver_;
    std::unique_ptr<ssl::stream<beast::tcp_stream>> stream_;
    asio::steady_timer timer_;
    beast::flat_buffer buf_;
    http::response_parser<http::string_body> parser_;
    http::request<http::string_body> req_;

    std::atomic<bool> cancelled_{false};
    size_t prev_body_size_ = 0;
    std::string leftover_;

    std::string host_, port_, body_, auth_;
    std::string full_response_, full_reasoning_;
    PushFn push_;
    DoneFn done_;

    void do_resolve()
    {
        auto self = shared_from_this();
        resolver_.async_resolve(host_, port_,
            [self](beast::error_code ec, tcp::resolver::results_type results) {
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_connect(results);
            });
    }

    void do_connect(tcp::resolver::results_type results)
    {
        auto self = shared_from_this();
        beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(30));
        beast::get_lowest_layer(*stream_).async_connect(results,
            [self](beast::error_code ec, auto) {
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_handshake();
            });
    }

    void do_handshake()
    {
        auto self = shared_from_this();
        stream_->async_handshake(ssl::stream_base::client,
            [self](beast::error_code ec) {
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_write_request();
            });
    }

    void do_write_request()
    {
        req_ = {};
        req_.method(http::verb::post);
        req_.target("/chat/completions");
        req_.version(11);
        req_.set(http::field::host, host_);
        req_.set(http::field::content_type, "application/json");
        req_.set(http::field::connection, "close");
        if (!auth_.empty()) req_.set(http::field::authorization, auth_);
        req_.body() = body_;
        req_.prepare_payload();

        auto self = shared_from_this();
        http::async_write(*stream_, req_,
            [self](beast::error_code ec, std::size_t) {
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_read_header();
            });
    }

    void do_read_header()
    {
        auto self = shared_from_this();
        http::async_read_header(*stream_, buf_, parser_,
            [self](beast::error_code ec, std::size_t) {
                if (ec) { self->finish_error(ec.message()); return; }
                int status = self->parser_.get().result_int();
                if (status != 200) {
                    http::async_read(*self->stream_, self->buf_, self->parser_,
                        [self](beast::error_code ec2, std::size_t) {
                            std::string msg = "HTTP " + std::to_string(
                                self->parser_.get().result_int());
                            if (!ec2) msg += ": " + self->parser_.get().body();
                            self->finish_error(msg);
                        });
                    return;
                }
                self->parser_.body_limit(std::numeric_limits<std::uint64_t>::max());
                self->prev_body_size_ = 0;
                self->do_read_body();
            });
    }

    void do_read_body()
    {
        auto self = shared_from_this();
        http::async_read_some(*stream_, buf_, parser_,
            [self](beast::error_code ec, std::size_t) {
                if (ec == http::error::end_of_stream) {
                    self->drain_remaining();
                    self->finish_success();
                    return;
                }
                if (ec) { self->finish_error(ec.message()); return; }
                self->drain_remaining();
                if (self->parser_.is_done()) {
                    self->finish_success();
                } else {
                    self->do_read_body();
                }
            });
    }

    void drain_remaining()
    {
        auto& body = parser_.get().body();
        size_t prev = prev_body_size_;
        prev_body_size_ = body.size();
        if (prev < body.size()) {
            std::string chunk = body.substr(prev);
            process_sse(chunk);
        }
    }

    void process_sse(const std::string& chunk)
    {
        leftover_ += chunk;
        size_t pos;
        while ((pos = leftover_.find("\n\n")) != std::string::npos) {
            std::string event = leftover_.substr(0, pos);
            leftover_.erase(0, pos + 2);
            std::istringstream ss(event);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("data: ", 0) != 0) continue;
                std::string data = line.substr(6);
                if (data == "[DONE]") continue;
                try {
                    auto val = boost::json::parse(data);
                    auto& choices = val.as_object().at("choices").as_array();
                    if (choices.empty()) continue;
                    auto& delta = choices[0].as_object().at("delta").as_object();

                    if (delta.contains("reasoning_content") &&
                        delta.at("reasoning_content").is_string()) {
                        std::string r = delta.at("reasoning_content").as_string().c_str();
                        if (!r.empty()) {
                            full_reasoning_ += r;
                            boost::json::object o;
                            o["type"] = "reasoning";
                            o["text"] = std::move(r);
                            push_(std::move(o));
                        }
                    }
                    if (delta.contains("content") &&
                        delta.at("content").is_string()) {
                        std::string c = delta.at("content").as_string().c_str();
                        if (!c.empty()) {
                            full_response_ += c;
                            boost::json::object o;
                            o["type"] = "delta";
                            o["text"] = std::move(c);
                            push_(std::move(o));
                        }
                    }
                } catch (...) {}
            }
        }
    }

    void finish_success()
    {
        stop_timer();
        if (cancelled_) return;
        boost::json::object end;
        end["type"] = "stream_end";
        push_(std::move(end));
        if (done_) done_(std::move(full_response_), std::move(full_reasoning_));
    }

    void finish_error(const std::string& msg)
    {
        stop_timer();
        if (cancelled_) return;
        boost::json::object end;
        end["type"] = "stream_end";
        if (!msg.empty()) end["msg"] = msg;
        push_(std::move(end));
        if (done_) done_(std::move(full_response_), std::move(full_reasoning_));
    }
};

// ---- API key ----

static std::string loadDeepSeekApiKey()
{
    std::ifstream f("config.json");
    if (!f.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    try {
        auto val = boost::json::parse(content);
        auto& obj = val.as_object();
        if (obj.contains("deepseek_api_key"))
            return obj["deepseek_api_key"].as_string().c_str();
    } catch (...) {}
    return "";
}

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

// ---- Legacy synchronous DeepSeek (for app_process fallback) ----

using LegacyPush = std::function<void(boost::json::object)>;

static void callDeepSeekLegacy(
    const std::vector<boost::json::object>& history,
    LegacyPush push_event,
    std::string& out_full_response,
    std::string& out_full_reasoning)
{
    out_full_response.clear();
    out_full_reasoning.clear();

    boost::json::array msgs;
    for (const auto& m : history) msgs.push_back(m);

    boost::json::object body;
    body["model"] = "deepseek-v4-flash";
    body["messages"] = msgs;
    body["stream"] = true;
    body["thinking"] = {{"type", "enabled"}, {"budget_tokens", 4096}};

    std::string apiKey = loadDeepSeekApiKey();
    if (apiKey.empty()) {
        boost::json::object end;
        end["type"] = "stream_end";
        end["msg"]  = "missing deepseek_api_key in config.json";
        push_event(std::move(end));
        return;
    }

    std::string leftover;

    auto on_chunk = [&](const std::string& chunk) {
        leftover += chunk;
        size_t pos;
        while ((pos = leftover.find("\n\n")) != std::string::npos) {
            std::string event = leftover.substr(0, pos);
            leftover.erase(0, pos + 2);
            std::istringstream ss(event);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("data: ", 0) != 0) continue;
                std::string data = line.substr(6);
                if (data == "[DONE]") continue;
                try {
                    auto val = boost::json::parse(data);
                    auto& choices = val.as_object().at("choices").as_array();
                    if (choices.empty()) continue;
                    auto& delta = choices[0].as_object().at("delta").as_object();
                    if (delta.contains("reasoning_content") && delta.at("reasoning_content").is_string()) {
                        std::string r = delta.at("reasoning_content").as_string().c_str();
                        if (!r.empty()) {
                            out_full_reasoning += r;
                            boost::json::object o;
                            o["type"] = "reasoning";
                            o["text"] = std::move(r);
                            push_event(std::move(o));
                        }
                    }
                    if (delta.contains("content") && delta.at("content").is_string()) {
                        std::string c = delta.at("content").as_string().c_str();
                        if (!c.empty()) {
                            out_full_response += c;
                            boost::json::object o;
                            o["type"] = "delta";
                            o["text"] = std::move(c);
                            push_event(std::move(o));
                        }
                    }
                } catch (...) {}
            }
        }
    };

    try {
        httpsPostStream("api.deepseek.com", "443", "/chat/completions",
            boost::json::serialize(body), "application/json",
            "Bearer " + apiKey, on_chunk);
        size_t pos;
        while ((pos = leftover.find("\n\n")) != std::string::npos) {
            std::string event = leftover.substr(0, pos);
            leftover.erase(0, pos + 2);
            std::istringstream ss(event);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("data: ", 0) != 0) continue;
                std::string data = line.substr(6);
                if (data == "[DONE]") continue;
                try {
                    auto val = boost::json::parse(data);
                    auto& choices = val.as_object().at("choices").as_array();
                    if (choices.empty()) continue;
                    auto& delta = choices[0].as_object().at("delta").as_object();
                    if (delta.contains("reasoning_content") && delta.at("reasoning_content").is_string()) {
                        std::string r = delta.at("reasoning_content").as_string().c_str();
                        if (!r.empty()) {
                            out_full_reasoning += r;
                            boost::json::object o;
                            o["type"] = "reasoning";
                            o["text"] = std::move(r);
                            push_event(std::move(o));
                        }
                    }
                    if (delta.contains("content") && delta.at("content").is_string()) {
                        std::string c = delta.at("content").as_string().c_str();
                        if (!c.empty()) {
                            out_full_response += c;
                            boost::json::object o;
                            o["type"] = "delta";
                            o["text"] = std::move(c);
                            push_event(std::move(o));
                        }
                    }
                } catch (...) {}
            }
        }
    } catch (const std::exception& e) {
        std::string what = e.what();
        if (what.find("eof") == std::string::npos &&
            what.find("end of stream") == std::string::npos) {
            boost::json::object end_val;
            end_val["type"] = "stream_end";
            end_val["msg"] = std::move(what);
            push_event(std::move(end_val));
            return;
        }
    }

    boost::json::object end;
    end["type"] = "stream_end";
    push_event(std::move(end));
}

// ---- Process next queued message ----

// Must NOT hold mtx when calling handleUserMessageAsync (non-recursive mutex)
static void processNextInQueue(ChatApp* app)
{
    std::string next;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        if (app->cancelled) return;
        if (app->input_queue.empty()) {
            CHAT_LOG("[chat-q]", "queue empty, idle (round " << app->round << ")");
            return;
        }
        next = std::move(app->input_queue.front());
        app->input_queue.pop_front();
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

    boost::json::object body;
    body["model"] = "deepseek-v4-flash";
    body["messages"] = msgs;
    body["stream"] = true;
    body["thinking"] = {{"type", "enabled"}, {"budget_tokens", 4096}};

    std::string apiKey = loadDeepSeekApiKey();
    if (apiKey.empty()) {
        boost::json::object end;
        end["type"] = "stream_end";
        end["msg"]  = "missing deepseek_api_key in config.json";
        app->push_output(std::move(end));
        return;
    }

    asio::io_context* io = static_cast<asio::io_context*>(app->io_ctx_ptr);
    if (!io) {
        // Fallback: legacy background thread
        // Shared ownership keeps ChatApp alive until the thread completes
        auto self = app->shared_from_this();
        std::thread t([self, history_copy]() {
            auto push_event = [self](boost::json::object obj) {
                if (self->cancelled) return;
                self->push_output(std::move(obj));
            };
            std::string fr, fg;
            callDeepSeekLegacy(history_copy, push_event, fr, fg);
            if (!fr.empty()) {
                std::lock_guard<std::mutex> lock(self->mtx);
                if (self->cancelled) return;
                boost::json::object am;
                am["role"] = "assistant";
                am["content"] = std::move(fr);
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
        auto push_event = [weak_app](boost::json::object obj) {
            auto self = weak_app.lock();
            if (!self) return;
            self->push_output(std::move(obj));
        };

        // weak_ptr breaks potential cycle: ChatApp → AsyncDeepSeek → on_done → ChatApp
        std::weak_ptr<ChatApp> weak_self = app->shared_from_this();
        auto on_done = [weak_self](std::string response, std::string) {
            auto self = weak_self.lock();
            if (!self) return;
            if (self->cancelled) return;
            {
                std::lock_guard<std::mutex> lock(self->mtx);
                if (!response.empty()) {
                    boost::json::object am;
                    am["role"] = "assistant";
                    am["content"] = std::move(response);
                    self->history.push_back(std::move(am));
                }
                self->current_stream.reset();
            }
        };

        auto stream = std::make_shared<ChatApp::AsyncDeepSeek>(*io);
        app->current_stream = stream;
        stream->start("api.deepseek.com", "443",
            boost::json::serialize(body), "Bearer " + apiKey,
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
        auto push_event = [self](boost::json::object obj) {
            if (self->cancelled) return;
            self->push_output(std::move(obj));
        };
        std::string fr, fg;
        callDeepSeekLegacy(history_copy, push_event, fr, fg);
        if (!fr.empty()) {
            std::lock_guard<std::mutex> lock(self->mtx);
            if (self->cancelled) return;
            boost::json::object am;
            am["role"] = "assistant";
            am["content"] = std::move(fr);
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
    app->output_cb = nullptr;
    app->output_udata = nullptr;
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
                    std::lock_guard<std::mutex> lock(app->mtx);
                    app->input_queue.push_back(text);
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
    std::lock_guard<std::mutex> lock(app->mtx);
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
