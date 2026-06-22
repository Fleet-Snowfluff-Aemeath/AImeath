#include <benchmark/benchmark.h>
#include <sstream>
#include "logger.hpp"
#include "threadmgr.hpp"

static void BM_LogSingleMessage(benchmark::State& state)
{
    std::ostringstream oss;
    Logger log(oss);
    int n = state.range(0);
    for (auto _ : state)
    {
        for (int i = 0; i < n; ++i)
            log.info() << "benchmark log message " << i;
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_LogSingleMessage)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(1000);

static void BM_LogWithAllLevels(benchmark::State& state)
{
    std::ostringstream oss;
    Logger log(oss, Logger::DEBUG);
    for (auto _ : state)
    {
        log.debug() << "debug";
        log.info() << "info";
        log.warn() << "warn";
        log.error() << "error";
    }
}
BENCHMARK(BM_LogWithAllLevels);

static void BM_LogFilteredLevel(benchmark::State& state)
{
    std::ostringstream oss;
    Logger log(oss, Logger::ERROR);
    for (auto _ : state)
    {
        log.debug() << "filtered debug";
        log.info() << "filtered info";
        log.warn() << "filtered warn";
        log.error() << "visible error";
    }
}
BENCHMARK(BM_LogFilteredLevel);

static void BM_LogLongMessage(benchmark::State& state)
{
    std::ostringstream oss;
    Logger log(oss);
    std::string big(state.range(0), 'x');
    for (auto _ : state)
        log.info() << big;
}
BENCHMARK(BM_LogLongMessage)
    ->Arg(100)->Arg(1000)->Arg(10000);

static void BM_LogConcurrent(benchmark::State& state)
{
    int thread_count = state.range(0);
    std::ostringstream oss;
    Logger log(oss);
    for (auto _ : state)
    {
        ThreadPool pool(thread_count);
        for (int i = 0; i < 100; ++i)
            pool.submit([&]() { log.info() << "concurrent log"; });
        pool.wait_all();
    }
}
BENCHMARK(BM_LogConcurrent)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();
