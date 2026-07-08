#include <benchmark/benchmark.h>
#include "plugin_mod.hpp"
#include "plugin_manager.hpp"
#include <string>
#include <thread>
#include <vector>
#include <boost/json.hpp>

// ====== PluginModuleCache: load (dlopen) ======

static void BM_PluginModuleLoad(benchmark::State& state)
{
    for (auto _ : state)
    {
        PluginModuleCache cache;
        auto m = cache.load("mock_plugin");
        benchmark::DoNotOptimize(m);
    }
}
BENCHMARK(BM_PluginModuleLoad);

// ====== PluginModuleCache: cache hit ======

static void BM_PluginCacheHit(benchmark::State& state)
{
    PluginModuleCache cache;
    cache.load("mock_plugin");
    for (auto _ : state)
    {
        auto m = cache.load("mock_plugin");
        benchmark::DoNotOptimize(m);
    }
}
BENCHMARK(BM_PluginCacheHit);

// ====== PluginModuleCache: evict ======

static void BM_PluginCacheEvict(benchmark::State& state)
{
    for (auto _ : state)
    {
        PluginModuleCache cache;
        cache.load("mock_plugin");
        cache.evict("mock_plugin");
    }
}
BENCHMARK(BM_PluginCacheEvict);

// ====== PluginModuleCache: clear with N entries ======

static void BM_PluginCacheClear(benchmark::State& state)
{
    int N = state.range(0);
    for (auto _ : state)
    {
        PluginModuleCache cache;
        for (int i = 0; i < N; ++i)
            cache.load("mock_plugin");
        cache.clear();
    }
}
BENCHMARK(BM_PluginCacheClear)->Arg(1)->Arg(10)->Arg(100);

// ====== PluginModuleCache: concurrent load ======

static void BM_PluginCacheConcurrentLoad(benchmark::State& state)
{
    int thread_count = state.range(0);
    for (auto _ : state)
    {
        PluginModuleCache cache;
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t)
        {
            threads.emplace_back([&]() {
                auto m = cache.load("mock_plugin");
                benchmark::DoNotOptimize(m);
            });
        }
        for (auto& t : threads) t.join();
        cache.clear();
    }
}
BENCHMARK(BM_PluginCacheConcurrentLoad)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

// ====== PluginModule: plugin_create ======

static void BM_PluginCreate(benchmark::State& state)
{
    PluginModuleCache cache;
    auto m = cache.load("mock_plugin");
    for (auto _ : state)
    {
        auto plugin = m.create(R"({"bench":true})");
        benchmark::DoNotOptimize(plugin);
    }
}
BENCHMARK(BM_PluginCreate);

// ====== PluginModule: plugin_create + plugin_destroy cycle ======

static void BM_PluginModuleCreateDestroy(benchmark::State& state)
{
    PluginModuleCache cache;
    auto m = cache.load("mock_plugin");
    for (auto _ : state)
    {
        auto plugin = m.create(R"({"bench":true})");
        benchmark::DoNotOptimize(plugin);
    }
}
BENCHMARK(BM_PluginModuleCreateDestroy);

// ====== PluginModule: plugin_process round-trip ======

static void BM_PluginProcess(benchmark::State& state)
{
    PluginModuleCache cache;
    auto m = cache.load("mock_plugin");
    auto plugin = m.create(R"({"bench":true})");
    for (auto _ : state)
    {
        char* out = m.plugin_process(plugin.get(), "ping");
        benchmark::DoNotOptimize(out);
        m.plugin_free_string(out);
    }
}
BENCHMARK(BM_PluginProcess);

// ====== PluginManager: subscribe + unsubscribe cycle ======

static void BM_PluginManagerSubscribe(benchmark::State& state)
{
    auto& mgr = PluginManager::instance();
    PluginModuleCache cache;
    mgr.init(&cache);
    for (auto _ : state)
    {
        auto h = mgr.subscribe([](const std::string&, const boost::json::value&) {});
        mgr.unsubscribe(h);
    }
}
BENCHMARK(BM_PluginManagerSubscribe);

// ====== PluginManager: notify with N subscribers ======

static void BM_PluginManagerNotify(benchmark::State& state)
{
    auto& mgr = PluginManager::instance();
    PluginModuleCache cache;
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
BENCHMARK(BM_PluginManagerNotify)->Arg(1)->Arg(10)->Arg(50)->Arg(200);

// ====== PluginManager: openPlugin with cached module ======

static void BM_PluginManagerOpenCheck(benchmark::State& state)
{
    auto& mgr = PluginManager::instance();
    PluginModuleCache cache;
    mgr.init(&cache);
    cache.load("mock_plugin");
    for (auto _ : state)
    {
        bool ok = mgr.openPlugin("mock_plugin", "{}");
        benchmark::DoNotOptimize(ok);
    }
}
BENCHMARK(BM_PluginManagerOpenCheck);

BENCHMARK_MAIN();
