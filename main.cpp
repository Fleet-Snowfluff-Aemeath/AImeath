/**
 * gameserver — 统一 WebSocket 服务端（全异步架构）
 *
 * 端口从 config.json 的 "port" 字段读取，默认 3001。
 * 每个连接由 shared_ptr<Session> 管理生命周期，
 * 通过 async_read / async_write 处理 WebSocket 消息。
 * Session / Listener 定义见 module/core/include/ws_server.hpp
 */

#include <iostream>
#include <thread>
#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

#include "config.hpp"
#include "threadmgr.hpp"
#include "logger.hpp"
#include "ws_server.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;

int main()
{
    Logger logger(std::cout, Logger::INFO);

    // L1: Config singleton loaded once; port() defaults to 3001 if missing.
    int port = Config::instance().port();
    logger.info() << "Loaded port from config.json: " << port;

    ThreadPool io_pool(DEFAULT_IO_THREADS);
    auto& io = io_pool.io_context();

    ThreadPool fallback_pool(DEFAULT_FALLBACK_THREADS);
    AppModuleCache cache;

    auto listener = std::make_shared<Listener>(io, logger, cache, &fallback_pool, port);
    listener->run();

    asio::io_context sig_io;
    asio::signal_set signals(sig_io, SIGINT, SIGTERM);
    signals.async_wait([&listener, &logger](auto ec, auto sig) {
        if (!ec) {
            logger.info() << "Signal " << sig << " received, shutting down...";
            listener->shutdown();
        }
    });
    std::thread sig_thread([&sig_io] { sig_io.run(); });

    logger.info() << "Game server listening on port " << port;

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
