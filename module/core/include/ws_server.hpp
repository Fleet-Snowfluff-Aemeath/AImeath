#pragma once

/**
 * ws_server — 异步 WebSocket 服务端基础设施
 *
 * 提供 Session（连接管理 + 路由 + 消息循环）和 Listener（async_accept）。
 * 依赖 app_mod（app 模块）、threadmgr（线程池）、logger、wsutil。
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

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, Logger& logger,
            AppModuleCache& cache, ThreadPool* fallback_pool,
            asio::io_context* io_ctx, int port);

    void start();
    void on_app_output(const char* json);

private:
    void enqueue(std::string json);
    void do_write();
    void do_http_read();
    void do_http_response();
    void do_ws_accept();
    void do_read_first_msg();
    void route_and_setup();
    static void app_output_cb(void* userdata, const char* json);
    void do_read();
    void on_read(beast::error_code ec, std::size_t n);
    void process_legacy(const std::string& msg);
    bool app_is_done() const;
    void close_ws();

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
    asio::strand<asio::io_context::executor_type> strand_;
    int             port_;
    std::string     first_msg_;

    std::deque<std::string> write_queue_;
    bool writing_ = false;
    bool closing_ = false;
};

class Listener : public std::enable_shared_from_this<Listener>
{
public:
    Listener(asio::io_context& io, Logger& logger,
             AppModuleCache& cache, ThreadPool* fallback_pool,
             int port = DEFAULT_PORT);

    int port() const { return port_; }

    void run();
    void shutdown();

private:
    void do_accept();

    asio::io_context& io_;
    tcp::acceptor    acceptor_;
    Logger&          logger_;
    AppModuleCache&  cache_;
    ThreadPool*      fallback_pool_;
    int              port_;
};
