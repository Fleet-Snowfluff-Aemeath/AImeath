#include <benchmark/benchmark.h>
#include <atomic>
#include "eventmgr.hpp"

static void BM_EventSubscribe(benchmark::State& state)
{
    EventManager mgr;
    int N = state.range(0);
    for (auto _ : state)
    {
        for (int i = 0; i < N; ++i)
            mgr.subscribe(100, [](const Event&) { });
    }
}
BENCHMARK(BM_EventSubscribe)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(1000);

static void BM_EventFire(benchmark::State& state)
{
    EventManager mgr;
    int N = state.range(0);
    for (int i = 0; i < N; ++i)
        mgr.subscribe(100, [](const Event&) { });
    for (auto _ : state)
        mgr.fire({100});
}
BENCHMARK(BM_EventFire)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(200);

static void BM_EventFireAsync(benchmark::State& state)
{
    EventManager mgr;
    int subs = state.range(0);
    for (int i = 0; i < subs; ++i)
        mgr.subscribe(100, [](const Event&) { });
    ThreadPool pool(4);
    for (auto _ : state)
    {
        mgr.fireAsync({100}, pool);
        pool.wait_all();
    }
}
BENCHMARK(BM_EventFireAsync)
    ->Arg(1)->Arg(10)->Arg(50);

static void BM_EventSubscribeAndUnsubscribe(benchmark::State& state)
{
    int N = state.range(0);
    for (auto _ : state)
    {
        EventManager mgr;
        std::vector<EventManager::Handle> handles;
        for (int i = 0; i < N; ++i)
            handles.push_back(mgr.subscribe(100, [](const Event&) { }));
        for (auto h : handles)
            mgr.unsubscribe(h);
    }
}
BENCHMARK(BM_EventSubscribeAndUnsubscribe)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(500);

static void BM_EventPriorityFire(benchmark::State& state)
{
    EventManager mgr;
    int N = state.range(0);
    for (int i = 0; i < N; ++i)
        mgr.subscribe(100, [](const Event&) { }, i % 10);
    for (auto _ : state)
        mgr.fire({100});
}
BENCHMARK(BM_EventPriorityFire)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(200);

static void BM_EventSubscriberCount(benchmark::State& state)
{
    EventManager mgr;
    int N = state.range(0);
    for (int i = 0; i < N; ++i)
        mgr.subscribe(100, [](const Event&) { });
    for (auto _ : state)
        benchmark::DoNotOptimize(mgr.subscriberCount(100));
}
BENCHMARK(BM_EventSubscriberCount)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(1000);

BENCHMARK_MAIN();
