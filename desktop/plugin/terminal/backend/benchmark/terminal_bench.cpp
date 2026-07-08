#include <benchmark/benchmark.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for Terminal (from terminal.cpp) ----
extern "C" {
void* plugin_create(const char* config_json);
void  plugin_destroy(void* p);
char* plugin_process(void* p, const char* input_json);
void  plugin_free_string(char* s);
}

static std::string callProcess(void* plugin, const std::string& json)
{
    char* out = plugin_process(plugin, json.c_str());
    std::string result(out ? out : "[]");
    if (out) plugin_free_string(out);
    return result;
}

static void BM_CreateDestroy(benchmark::State& state)
{
    for (auto _ : state) {
        void* plugin = plugin_create(nullptr);
        plugin_destroy(plugin);
    }
}
BENCHMARK(BM_CreateDestroy);

static void BM_CreateOnly(benchmark::State& state)
{
    for (auto _ : state) {
        void* plugin = plugin_create(nullptr);
        benchmark::DoNotOptimize(plugin);
        plugin_destroy(plugin);
    }
}
BENCHMARK(BM_CreateOnly);

static void BM_ExecSyncEcho(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"echo hello"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ExecSyncEcho);

static void BM_ExecSyncPwd(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"pwd"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ExecSyncPwd);

static void BM_ExecSyncListFiles(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"ls /"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ExecSyncListFiles);

static void BM_InvalidAction(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"__unknown__"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_InvalidAction);

static void BM_ResizeAction(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"resize","rows":30,"cols":100})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ResizeAction);

static void BM_MissingAction(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"cmd":"ls"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_MissingAction);

BENCHMARK_MAIN();
