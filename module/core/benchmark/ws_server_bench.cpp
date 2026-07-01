#include <benchmark/benchmark.h>
#include "ws_server.hpp"
#include <app_mod.hpp>
#include <boost/asio.hpp>

static void BM_WsServerListenerCreate(benchmark::State& state)
{
    asio::io_context io;
    Logger logger(Logger::ERROR);
    AppModuleCache cache;

    for (auto _ : state)
    {
        auto listener = std::make_shared<Listener>(io, logger, cache, nullptr, 0);
        benchmark::DoNotOptimize(listener);
        listener->shutdown();
    }
}
BENCHMARK(BM_WsServerListenerCreate);

static void BM_WsServerSessionCreate(benchmark::State& state)
{
    asio::io_context io;
    Logger logger(Logger::ERROR);
    AppModuleCache cache;
    ThreadPool fallback(1);

    for (auto _ : state)
    {
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
        tcp::socket socket1(io);
        tcp::socket socket2(io);

        acceptor.async_accept(socket2, [](boost::system::error_code) {});
        boost::system::error_code ec;
        socket1.connect(acceptor.local_endpoint(), ec);
        if (ec) { state.SkipWithError(ec.message().c_str()); return; }
        socket2.close();

        auto session = std::make_shared<Session>(
            std::move(socket1), logger, cache, &fallback, &io);
        benchmark::DoNotOptimize(session);
        fallback.shutdown();
    }
}
BENCHMARK(BM_WsServerSessionCreate);

BENCHMARK_MAIN();
