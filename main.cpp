/**
 * gameserver — 统一 WebSocket 服务端（全异步架构）
 *
 * N 个 io_context 线程 + epoll 驱动所有 I/O。
 * 每个连接由 shared_ptr<Session> 管理生命周期，通过 async_read /
 * async_write 处理 WebSocket 消息。
 *
 * 支持两种 app 模式：
 *   1. 异步 API（app_on_input + app_set_output）— 首选
 *   2. 同步 API（app_process）— 遗留，提交到后备线程池
 */

#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <deque>
#include <optional>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include "app_mod.hpp"
#include "threadmgr.hpp"
#include "logger.hpp"
#include "wsutil.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

// ---- 常量 ----

constexpr int PORT = 3001;
constexpr int IO_THREADS = 4;       // epoll 线程数
constexpr int FALLBACK_THREADS = 4; // 遗留同步 app 后备线程池

namespace key {
    constexpr auto APP  = "app";
    constexpr auto TEXT = "text";
    constexpr auto GAME = "game";
}

namespace appname {
    constexpr auto CHAT  = "chat";
    constexpr auto SNAKE = "snake";
}

// ============================================================
//  WebSocket 会话（每个连接一个）
// ============================================================

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, Logger& logger,
            AppModuleCache& cache, ThreadPool* fallback_pool,
            asio::io_context* io_ctx)
        : logger_(logger)
        , cache_(cache)
        , fallback_pool_(fallback_pool)
        , io_ctx_(io_ctx)
    {
        stream_.emplace(std::move(socket));
    }

    void start()
    {
        do_http_read();
    }

    // 由异步 app 的输出回调调用（任意线程）
    void on_app_output(const char* json)
    {
        asio::post(ws_->get_executor(),
            [self = shared_from_this(), s = std::string(json)]() {
                self->enqueue(std::move(s));
            });
    }

private:
    // ---- 状态机 ----

    std::optional<beast::tcp_stream>                          stream_;
    std::optional<websocket::stream<beast::tcp_stream>>       ws_;
    beast::flat_buffer                                         buf_;
    http::request<http::string_body>                           req_;

    // ---- 路由 / app ----

    Logger&         logger_;
    AppModuleCache& cache_;
    AppModule      mod_;
    AppPtr          app_;
    ThreadPool*     fallback_pool_;
    asio::io_context* io_ctx_;
    std::string     first_msg_;

    // ---- 写队列（串行化 async_write） ----

    std::deque<std::string> write_queue_;
    bool writing_ = false;

    // ---- HTTP Upgrade 阶段 ----

    void do_http_read()
    {
        auto self = shared_from_this();
        http::async_read(*stream_, buf_, req_,
            [self](beast::error_code ec, std::size_t) {
                if (ec) return;
                self->do_ws_accept();
            });
    }

    void do_ws_accept()
    {
        ws_.emplace(std::move(*stream_));
        stream_.reset();

        auto self = shared_from_this();
        ws_->async_accept(req_,
            [self](beast::error_code ec) {
                if (ec) return;
                self->do_read_first_msg();
            });
    }

    // ---- 首条消息（路由） ----

    void do_read_first_msg()
    {
        auto self = shared_from_this();
        ws_->async_read(buf_,
            [self](beast::error_code ec, std::size_t) {
                if (ec) return;
                self->first_msg_ = beast::buffers_to_string(self->buf_.data());
                self->buf_.clear();
                self->route_and_setup();
            });
    }

    void route_and_setup()
    {
        // 解析 JSON 决定 app 名称
        std::string app_name;
        try {
            auto val = boost::json::parse(first_msg_);
            if (val.is_object()) {
                std::string s = jsonParseStr(val, key::APP);
                if (!s.empty()) {
                    app_name = std::move(s);
                } else {
                    s = jsonParseStr(val, key::TEXT);
                    if (!s.empty()) {
                        app_name = appname::CHAT;
                    } else {
                        s = jsonParseStr(val, key::GAME);
                        if (!s.empty())
                            app_name = std::move(s);
                        else
                            app_name = appname::SNAKE;
                    }
                }
            }
        } catch (...) {}

        logger_.info() << "Routing to app: " << app_name;

        mod_ = cache_.load(app_name);
        if (!mod_) {
            enqueue(jsonError("failed to load " + app_name));
            close_ws();
            return;
        }

        app_ = mod_.create(first_msg_);
        if (!app_) {
            enqueue(jsonError("failed to create " + app_name + " instance"));
            close_ws();
            return;
        }

        if (mod_.is_async()) {
            // 异步路径
            mod_.app_set_output(app_.get(), &Session::app_output_cb, this);
            if (mod_.app_set_io_context)
                mod_.app_set_io_context(app_.get(), io_ctx_);
            mod_.app_on_input(app_.get(), first_msg_.c_str());
            do_read();
        } else {
            // 遗留同步路径
            process_legacy(first_msg_);
        }
    }

    // ---- 输出回调（异步 app） ----

    static void app_output_cb(void* userdata, const char* json)
    {
        static_cast<Session*>(userdata)->on_app_output(json);
    }

    // ---- WebSocket 消息循环 ----

    void do_read()
    {
        auto self = shared_from_this();
        ws_->async_read(buf_,
            [self](beast::error_code ec, std::size_t) {
                if (ec) return;
                self->on_read(ec, 0);
            });
    }

    void on_read(beast::error_code /*ec*/, std::size_t /*n*/)
    {
        std::string msg = beast::buffers_to_string(buf_.data());
        buf_.clear();

        if (mod_.is_async()) {
            // 异步路径：非阻塞投递，立即继续读取
            mod_.app_on_input(app_.get(), msg.c_str());
            do_read();
        } else {
            // 遗留同步路径：暂停读取，提交到后备线程池
            process_legacy(std::move(msg));
        }
    }

    // ---- 写队列 ----

    void enqueue(std::string json)
    {
        write_queue_.push_back(std::move(json));
        if (!writing_) do_write();
    }

    void do_write()
    {
        if (write_queue_.empty()) {
            writing_ = false;
            return;
        }
        writing_ = true;

        auto self = shared_from_this();
        ws_->async_write(asio::buffer(write_queue_.front()),
            [self](beast::error_code ec, std::size_t) {
                if (ec) return;
                self->write_queue_.pop_front();
                self->do_write();
            });
    }

    // ---- 遗留同步处理（提交到后备线程池） ----

    void process_legacy(const std::string& msg)
    {
        auto app  = app_.get();
        auto mod  = &mod_;
        auto self = shared_from_this();

        fallback_pool_->submit([self, msg, app, mod]() {
            char* out = mod->app_process(app, msg.c_str());
            if (out) {
                std::string results(out);
                mod->app_free_string(out);
                asio::post(self->ws_->get_executor(),
                    [self, results = std::move(results)]() {
                        try {
                            auto arr = boost::json::parse(results).as_array();
                            for (auto& item : arr)
                                self->enqueue(boost::json::serialize(item));
                        } catch (...) {}
                    });
            }

            // 检查是否结束，恢复读取
            asio::post(self->ws_->get_executor(), [self]() {
                if (self->app_is_done()) {
                    self->close_ws();
                } else {
                    self->do_read();
                }
            });
        });
    }

    bool app_is_done() const
    {
        return mod_.app_is_done
            && mod_.app_is_done(app_.get()) != 0;
    }

    void close_ws()
    {
        if (ws_) {
            beast::error_code ec;
            ws_->close(websocket::close_code::normal, ec);
        }
    }
};

// ============================================================
//  监听器（async_accept 循环）
// ============================================================

class Listener : public std::enable_shared_from_this<Listener>
{
public:
    Listener(asio::io_context& io, Logger& logger,
             AppModuleCache& cache, ThreadPool* fallback_pool)
        : io_(io)
        , acceptor_(io, tcp::endpoint(tcp::v4(), PORT))
        , logger_(logger)
        , cache_(cache)
        , fallback_pool_(fallback_pool)
    {}

    void run()
    {
        do_accept();
    }

    void shutdown()
    {
        beast::error_code ec;
        acceptor_.close(ec);
    }

private:
    void do_accept()
    {
        auto self = shared_from_this();
        acceptor_.async_accept(
            [self](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<Session>(
                        std::move(socket), self->logger_,
                        self->cache_, self->fallback_pool_,
                        &self->io_);
                    session->start();
                }
                if (ec != asio::error::operation_aborted)
                    self->do_accept();
            });
    }

    asio::io_context& io_;
    tcp::acceptor    acceptor_;
    Logger&          logger_;
    AppModuleCache&  cache_;
    ThreadPool*      fallback_pool_;
};

// ============================================================
//  主函数
// ============================================================

int main()
{
    Logger logger(std::cout, Logger::INFO);

    // epoll I/O 线程池（N = CPU 核数）
    ThreadPool io_pool(IO_THREADS);
    auto& io = io_pool.io_context();

    // 遗留同步 app 的后备线程池（不阻塞 epoll）
    ThreadPool fallback_pool(FALLBACK_THREADS);

    // 模块缓存（全局共享）
    AppModuleCache cache;

    // 监听器
    auto listener = std::make_shared<Listener>(io, logger, cache, &fallback_pool);
    listener->run();

    // 信号处理（独立线程，防止死锁）
    asio::io_context sig_io;
    asio::signal_set signals(sig_io, SIGINT, SIGTERM);
    signals.async_wait([&listener, &logger](auto ec, auto sig) {
        if (!ec) {
            logger.info() << "Signal " << sig << " received, shutting down...";
            listener->shutdown();
        }
    });
    std::thread sig_thread([&sig_io] { sig_io.run(); });

    logger.info() << "Game server listening on port " << PORT;

    // 主线程也参与 epoll 循环
    io.run();

    sig_io.stop();
    sig_thread.join();

    logger.info() << "Waiting for active connections...";
    io_pool.wait_all();
    io_pool.shutdown();
    fallback_pool.wait_all();
    fallback_pool.shutdown();

    logger.info() << "Server stopped.";
    return 0;
}
