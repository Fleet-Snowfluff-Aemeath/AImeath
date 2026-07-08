#include <fstream>
#include <thread>
#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

#include "config.hpp"
#include "threadmgr.hpp"
#include "logger.hpp"
#include "ws_server.hpp"
#include "plugin_cache.hpp"
#include "plugin_manager.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;

int main()
{
    std::ofstream logFile("logserver.log", std::ios::app);
    Logger logger(logFile, Logger::INFO);

    int port = Config::instance().port();
    int io_threads = Config::instance().ioThreads();
    int fb_threads = Config::instance().fallbackThreads();
    int max_conn = Config::instance().maxConnections();
    int stash_ttl = Config::instance().stashTtlSec();
    logger.info("main") << "Port: " << port
                  << " Stash TTL: " << (stash_ttl > 0 ? std::to_string(stash_ttl) + "s" : "unlimited");

    ThreadPool io_pool(io_threads);
    auto& io = io_pool.io_context();
    logger.info("main") << "Starting io_pool with " << io_threads << " IO threads.";

    ThreadPool fallback_pool(fb_threads);
    if (max_conn > 0) {
        fallback_pool.set_max_queue_size(static_cast<size_t>(max_conn) / 10);
    }
    logger.info("main") << "Starting fallback_pool with " << fb_threads << " threads. Max connections: "
        << (max_conn > 0 ? std::to_string(max_conn) : "unlimited");

    PluginManager::instance().init(&PluginCache::instance());
    logger.info("main") << "PluginManager initialized.";

    SessionManager::instance().setStashTtlSec(stash_ttl);

    auto listener = std::make_shared<Listener>(io, logger, PluginCache::instance(), &fallback_pool, port);
    if (max_conn > 0)
        listener->set_max_connections(max_conn);
    listener->run();

    asio::io_context sig_io;
    asio::signal_set signals(sig_io, SIGINT, SIGTERM);
    signals.async_wait([&listener, &logger](auto ec, auto sig) {
        if (!ec) {
            logger.info("main") << "Signal " << sig << " received, shutting down...";
            listener->shutdown();
        }
    });
    std::thread sig_thread([&sig_io] { sig_io.run(); });

    logger.info("main") << "Game server listening on port " << port;

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
