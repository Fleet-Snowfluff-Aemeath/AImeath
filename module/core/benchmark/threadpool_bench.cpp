#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include "threadmgr.hpp"

static void BM_SubmitEmptyTask(benchmark::State& state)
{
    ThreadPool pool(state.range(0));
    for (auto _ : state)
    {
        pool.submit([]() { });
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SubmitEmptyTask)
    ->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16);

static void BM_SubmitAndWait(benchmark::State& state)
{
    for (auto _ : state)
    {
        ThreadPool pool(state.range(0));
        int N = state.range(1);
        for (int i = 0; i < N; ++i)
            pool.submit([]() { });
        pool.wait_all();
    }
}
BENCHMARK(BM_SubmitAndWait)
    ->Args({1, 100})->Args({2, 100})->Args({4, 100})
    ->Args({4, 1000})->Args({8, 1000});

static void BM_SubmitHeavyTask(benchmark::State& state)
{
    ThreadPool pool(state.range(0));
    std::vector<int> data(1000, 42);
    for (auto _ : state)
    {
        pool.submit([data]() {
            int sum = 0;
            for (int x : data) sum += x;
            benchmark::DoNotOptimize(sum);
        });
    }
}
BENCHMARK(BM_SubmitHeavyTask)
    ->Arg(1)->Arg(4)->Arg(8);

static void BM_WaitAllOverhead(benchmark::State& state)
{
    for (auto _ : state)
    {
        ThreadPool pool(4);
        pool.submit([]() { });
        pool.wait_all();
    }
}
BENCHMARK(BM_WaitAllOverhead);

static void BM_ConcurrentSubmit(benchmark::State& state)
{
    int thread_count = state.range(0);
    int tasks_per_thread = state.range(1);
    ThreadPool pool(4);
    for (auto _ : state)
    {
        std::vector<std::thread> threads;
        std::atomic<int> ready{0};
        for (int t = 0; t < thread_count; ++t)
        {
            threads.emplace_back([&]() {
                ready.fetch_add(1);
                while (ready.load() != thread_count) {}
                for (int i = 0; i < tasks_per_thread; ++i)
                    pool.submit([]() { });
            });
        }
        for (auto& th : threads) th.join();
        pool.wait_all();
    }
}
BENCHMARK(BM_ConcurrentSubmit)
    ->Args({2, 50})->Args({4, 50})->Args({8, 50});

BENCHMARK_MAIN();
