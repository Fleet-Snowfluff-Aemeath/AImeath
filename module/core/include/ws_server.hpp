#pragma once

/**
 * ws_server — 异步 WebSocket 服务端基础设施
 *
 * 提供 Session（连接管理 + 路由 + 消息循环）和 Listener（async_accept）。
 * 依赖 app_mod（app 模块）、threadmgr（线程池）、logger、wsutil。
 *
 * 用法见 main.cpp。
 */

#include <string>
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

constexpr int DEFAULT_PORT = 3001;
constexpr int DEFAULT_IO_THREADS = 4;
constexpr int DEFAULT_FALLBACK_THREADS = 4;

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

    void start() { do_http_read(); }

    void on_app_output(const char* json)
    {
        asio::post(ws_->get_executor(),
            [self = shared_from_this(), s = std::string(json)]() {
                self->enqueue(std::move(s));
            });
    }

private:
    std::optional<beast::tcp_stream>                          stream_;
    std::optional<websocket::stream<beast::tcp_stream>>       ws_;
    beast::flat_buffer                                         buf_;
    http::request<http::string_body>                           req_;

    Logger&         logger_;
    AppModuleCache& cache_;
    AppModule      mod_;
    AppPtr          app_;
    ThreadPool*     fallback_pool_;
    asio::io_context* io_ctx_;
    std::string     first_msg_;

    std::deque<std::string> write_queue_;
    bool writing_ = false;

    void enqueue(std::string json)
    {
        write_queue_.push_back(std::move(json));
        if (!writing_) do_write();
    }

    void do_write()
    {
        if (write_queue_.empty()) { writing_ = false; return; }
        writing_ = true;
        auto self = shared_from_this();
        ws_->async_write(asio::buffer(write_queue_.front()),
            [self](beast::error_code ec, std::size_t) {
                if (ec) return;
                self->write_queue_.pop_front();
                self->do_write();
            });
    }

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
                self->buf_.clear();
                self->do_read_first_msg();
            });
    }

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
            mod_.app_set_output(app_.get(), &Session::app_output_cb, this);
            if (mod_.app_set_io_context)
                mod_.app_set_io_context(app_.get(), io_ctx_);
            mod_.app_on_input(app_.get(), first_msg_.c_str());
            do_read();
        } else {
            process_legacy(first_msg_);
        }
    }

    static void app_output_cb(void* userdata, const char* json)
    {
        static_cast<Session*>(userdata)->on_app_output(json);
    }

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
            mod_.app_on_input(app_.get(), msg.c_str());
            do_read();
        } else {
            process_legacy(std::move(msg));
        }
    }

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
             AppModuleCache& cache, ThreadPool* fallback_pool,
             int port = DEFAULT_PORT)
        : io_(io)
        , acceptor_(io, tcp::endpoint(tcp::v4(), port))
        , logger_(logger)
        , cache_(cache)
        , fallback_pool_(fallback_pool)
    {}

    void run() { do_accept(); }

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
