#include <benchmark/benchmark.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for FileManager (from filemgr.cpp) ----
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

static void BM_ListRoot(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"list","path":"/"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ListRoot);

static void BM_ListHome(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"list","path":"/home"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ListHome);

static void BM_WriteFile(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"write","path":"/bench.txt","content":"benchmark data"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_WriteFile);

static void BM_ReadNonexistent(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"read","path":"/nonexistent_bench.xyz"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ReadNonexistent);

static void BM_MkdirRemove(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        callProcess(plugin, R"({"action":"mkdir","path":"/bench_dir"})");
        std::string result = callProcess(plugin, R"({"action":"remove","path":"/bench_dir"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_MkdirRemove);

static void BM_ListInvalid(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(plugin, R"({"action":"list","path":"/__nonexistent__"})");
        benchmark::DoNotOptimize(result);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_ListInvalid);

static void BM_WriteReadRemove(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    for (auto _ : state) {
        callProcess(plugin, R"({"action":"write","path":"/bench_rw.txt","content":"data"})");
        std::string read = callProcess(plugin, R"({"action":"read","path":"/bench_rw.txt"})");
        callProcess(plugin, R"({"action":"remove","path":"/bench_rw.txt"})");
        benchmark::DoNotOptimize(read);
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_WriteReadRemove);

BENCHMARK_MAIN();
