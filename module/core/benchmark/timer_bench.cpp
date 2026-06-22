#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <chrono>
#include "timer.hpp"

static void BM_TimerSetTimeout(benchmark::State& state)
{
    ThreadPool pool(2);
    int N = state.range(0);
    for (auto _ : state)
    {
        Timer timer(pool);
        for (int i = 0; i < N; ++i)
            timer.setTimeout(std::chrono::milliseconds(100), []() { });
    }
}
BENCHMARK(BM_TimerSetTimeout)->Arg(1)->Arg(10)->Arg(100);

static void BM_TimerSetTimeoutAt(benchmark::State& state)
{
    ThreadPool pool(2);
    int N = state.range(0);
    for (auto _ : state)
    {
        Timer timer(pool);
        auto base = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i)
            timer.setTimeoutAt(base + std::chrono::milliseconds(100 + i), []() { });
    }
}
BENCHMARK(BM_TimerSetTimeoutAt)->Arg(1)->Arg(10)->Arg(100);

static void BM_TimerSetAndCancel(benchmark::State& state)
{
    ThreadPool pool(2);
    int N = state.range(0);
    for (auto _ : state)
    {
        Timer timer(pool);
        std::vector<uint64_t> ids;
        for (int i = 0; i < N; ++i)
            ids.push_back(timer.setTimeout(
                std::chrono::milliseconds(5000), []() { }));
        for (auto id : ids)
            timer.cancel(id);
    }
}
BENCHMARK(BM_TimerSetAndCancel)->Arg(1)->Arg(10)->Arg(100);

static void BM_TimerFire(benchmark::State& state)
{
    ThreadPool pool(4);
    int N = state.range(0);
    std::atomic<int> count{0};
    for (auto _ : state)
    {
        count.store(0);
        Timer timer(pool);
        for (int i = 0; i < N; ++i)
            timer.setTimeout(std::chrono::milliseconds(5), [&]() {
                count.fetch_add(1, std::memory_order_relaxed);
            });
        while (count.load() < N)
            std::this_thread::yield();
    }
}
BENCHMARK(BM_TimerFire)->Arg(10)->Arg(50);

static void BM_TimerSetInterval(benchmark::State& state)
{
    ThreadPool pool(2);
    for (auto _ : state)
    {
        Timer timer(pool);
        std::atomic<int> count{0};
        auto id = timer.setInterval(std::chrono::milliseconds(2), [&]() {
            count.fetch_add(1);
        });
        while (count.load() < state.range(0))
            std::this_thread::yield();
        timer.cancel(id);
    }
}
BENCHMARK(BM_TimerSetInterval)->Arg(5)->Arg(20);

static void BM_TimerExists(benchmark::State& state)
{
    ThreadPool pool(2);
    Timer timer(pool);
    int N = state.range(0);
    std::vector<uint64_t> ids;
    for (int i = 0; i < N; ++i)
        ids.push_back(timer.setTimeout(
            std::chrono::milliseconds(5000), []() { }));
    uint64_t fake = 999999;
    for (auto _ : state)
    {
        for (auto id : ids)
            benchmark::DoNotOptimize(timer.exists(id));
        benchmark::DoNotOptimize(timer.exists(fake));
    }
}
BENCHMARK(BM_TimerExists)->Arg(10)->Arg(100);

BENCHMARK_MAIN();
