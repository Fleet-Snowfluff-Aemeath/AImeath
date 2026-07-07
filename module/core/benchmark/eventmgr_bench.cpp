#include <benchmark/benchmark.h>
#include <atomic>
#include <vector>
#include "eventmgr.hpp"
#include "threadmgr.hpp"

struct BE {};

static void BM_EventSubscribe(benchmark::State& state)
{
    EventBus bus;
    int N = state.range(0);
    for (auto _ : state)
    {
        for (int i = 0; i < N; ++i)
            bus.subscribe<BE>([](const BE&) { });
    }
}
BENCHMARK(BM_EventSubscribe)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(1000);

static void BM_EventFire(benchmark::State& state)
{
    EventBus bus;
    int N = state.range(0);
    for (int i = 0; i < N; ++i)
        bus.subscribe<BE>([](const BE&) { });
    for (auto _ : state)
        bus.fire(BE{});
}
BENCHMARK(BM_EventFire)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(200);

static void BM_EventFireAsync(benchmark::State& state)
{
    ThreadPool pool(4);
    EventBus bus(threadPoolExecutor(pool));
    int subs = state.range(0);
    for (int i = 0; i < subs; ++i)
        bus.subscribe<BE>([](const BE&) { });
    for (auto _ : state)
    {
        bus.fireAsync(BE{});
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
        EventBus bus;
        std::vector<Subscription> handles;
        for (int i = 0; i < N; ++i)
            handles.push_back(bus.subscribe<BE>([](const BE&) { }));
    }
}
BENCHMARK(BM_EventSubscribeAndUnsubscribe)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(500);

static void BM_EventPriorityFire(benchmark::State& state)
{
    EventBus bus;
    int N = state.range(0);
    for (int i = 0; i < N; ++i)
        bus.subscribe<BE>([](const BE&) { }, i % 10);
    for (auto _ : state)
        bus.fire(BE{});
}
BENCHMARK(BM_EventPriorityFire)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(200);

static void BM_EventSubscriberCount(benchmark::State& state)
{
    EventBus bus;
    int N = state.range(0);
    for (int i = 0; i < N; ++i)
        bus.subscribe<BE>([](const BE&) { });
    for (auto _ : state)
        benchmark::DoNotOptimize(bus.subscriberCount<BE>());
}
BENCHMARK(BM_EventSubscriberCount)
    ->Arg(1)->Arg(10)->Arg(100)->Arg(1000);

BENCHMARK_MAIN();
