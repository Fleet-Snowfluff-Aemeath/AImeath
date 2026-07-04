#include <benchmark/benchmark.h>
#include "app_mod.hpp"
#include "app_manager.hpp"
#include <string>
#include <thread>
#include <vector>
#include <boost/json.hpp>

// ====== AppModuleCache: load (dlopen) ======

static void BM_AppModuleLoad(benchmark::State& state)
{
    for (auto _ : state)
    {
        AppModuleCache cache;
        auto m = cache.load("mock_app");
        benchmark::DoNotOptimize(m);
    }
}
BENCHMARK(BM_AppModuleLoad);

// ====== AppModuleCache: cache hit ======

static void BM_AppCacheHit(benchmark::State& state)
{
    AppModuleCache cache;
    cache.load("mock_app");
    for (auto _ : state)
    {
        auto m = cache.load("mock_app");
        benchmark::DoNotOptimize(m);
    }
}
BENCHMARK(BM_AppCacheHit);

// ====== AppModuleCache: evict ======

static void BM_AppCacheEvict(benchmark::State& state)
{
    for (auto _ : state)
    {
        AppModuleCache cache;
        cache.load("mock_app");
        cache.evict("mock_app");
    }
}
BENCHMARK(BM_AppCacheEvict);

// ====== AppModuleCache: clear with N entries ======

static void BM_AppCacheClear(benchmark::State& state)
{
    int N = state.range(0);
    for (auto _ : state)
    {
        AppModuleCache cache;
        for (int i = 0; i < N; ++i)
            cache.load("mock_app");
        cache.clear();
    }
}
BENCHMARK(BM_AppCacheClear)->Arg(1)->Arg(10)->Arg(100);

// ====== AppModuleCache: concurrent load ======

static void BM_AppCacheConcurrentLoad(benchmark::State& state)
{
    int thread_count = state.range(0);
    for (auto _ : state)
    {
        AppModuleCache cache;
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t)
        {
            threads.emplace_back([&]() {
                auto m = cache.load("mock_app");
                benchmark::DoNotOptimize(m);
            });
        }
        for (auto& t : threads) t.join();
        cache.clear();
    }
}
BENCHMARK(BM_AppCacheConcurrentLoad)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

// ====== AppModule: app_create ======

static void BM_AppCreate(benchmark::State& state)
{
    AppModuleCache cache;
    auto m = cache.load("mock_app");
    for (auto _ : state)
    {
        auto app = m.create(R"({"bench":true})");
        benchmark::DoNotOptimize(app);
    }
}
BENCHMARK(BM_AppCreate);

// ====== AppModule: app_create + app_destroy cycle ======

static void BM_AppModuleCreateDestroy(benchmark::State& state)
{
    AppModuleCache cache;
    auto m = cache.load("mock_app");
    for (auto _ : state)
    {
        auto app = m.create(R"({"bench":true})");
        benchmark::DoNotOptimize(app);
    }
}
BENCHMARK(BM_AppModuleCreateDestroy);

// ====== AppModule: app_process round-trip ======

static void BM_AppProcess(benchmark::State& state)
{
    AppModuleCache cache;
    auto m = cache.load("mock_app");
    auto app = m.create(R"({"bench":true})");
    for (auto _ : state)
    {
        char* out = m.app_process(app.get(), "ping");
        benchmark::DoNotOptimize(out);
        m.app_free_string(out);
    }
}
BENCHMARK(BM_AppProcess);

// ====== AppManager: subscribe + unsubscribe cycle ======

static void BM_AppManagerSubscribe(benchmark::State& state)
{
    auto& mgr = AppManager::instance();
    AppModuleCache cache;
    mgr.init(&cache);
    for (auto _ : state)
    {
        auto h = mgr.subscribe([](const std::string&, const boost::json::value&) {});
        mgr.unsubscribe(h);
    }
}
BENCHMARK(BM_AppManagerSubscribe);

// ====== AppManager: notify with N subscribers ======

static void BM_AppManagerNotify(benchmark::State& state)
{
    auto& mgr = AppManager::instance();
    AppModuleCache cache;
    mgr.init(&cache);
    int N = state.range(0);
    std::vector<uint64_t> handles;
    for (int i = 0; i < N; ++i)
        handles.push_back(mgr.subscribe([](const std::string&, const boost::json::value&) {}));
    for (auto _ : state)
        mgr.notifyStateChange("bench", boost::json::object{{"x", 1}});
    for (auto h : handles)
        mgr.unsubscribe(h);
}
BENCHMARK(BM_AppManagerNotify)->Arg(1)->Arg(10)->Arg(50)->Arg(200);

// ====== AppManager: openApp with cached module ======

static void BM_AppManagerOpenCheck(benchmark::State& state)
{
    auto& mgr = AppManager::instance();
    AppModuleCache cache;
    mgr.init(&cache);
    cache.load("mock_app");
    for (auto _ : state)
    {
        bool ok = mgr.openApp("mock_app", "{}");
        benchmark::DoNotOptimize(ok);
    }
}
BENCHMARK(BM_AppManagerOpenCheck);

BENCHMARK_MAIN();
