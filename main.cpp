/**
 * AImeath �?统一 WebSocket 服务端（全异步架构）
 *
 * 端口�?config.json �?"port" 字段读取，默�?3001�?
 * 每个连接�?shared_ptr<Session> 管理生命周期�?
 * 通过 async_read / async_write 处理 WebSocket 消息�?
 * Session / Listener 定义�?module/core/include/ws_server.hpp
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
#include "app_mod.hpp"
#include "app_manager.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;

int main()
{
    Logger logger(std::cout, Logger::INFO);

    int port = Config::instance().port();
    int io_threads = Config::instance().ioThreads();
    int fb_threads = Config::instance().fallbackThreads();
    int max_conn = Config::instance().maxConnections();
    int stash_ttl = Config::instance().stashTtlSec();
    logger.info() << "Port: " << port
                  << " IO threads: " << io_threads
                  << " Fallback threads: " << fb_threads
                  << " Max connections: " << (max_conn > 0 ? std::to_string(max_conn) : "unlimited")
                  << " Stash TTL: " << (stash_ttl > 0 ? std::to_string(stash_ttl) + "s" : "unlimited");

    ThreadPool io_pool(io_threads);
    auto& io = io_pool.io_context();

    ThreadPool fallback_pool(fb_threads);
    if (max_conn > 0) {
        fallback_pool.set_max_queue_size(static_cast<size_t>(max_conn) / 10);
    }
    AppModuleCache cache;

    AppManager::instance().init(&cache);

    Config::instance().setChatCachePtr(reinterpret_cast<uintptr_t>(&cache));
    SessionManager::instance().setStashTtlSec(stash_ttl);

    auto listener = std::make_shared<Listener>(io, logger, cache, &fallback_pool, port);
    if (max_conn > 0)
        listener->set_max_connections(max_conn);
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
