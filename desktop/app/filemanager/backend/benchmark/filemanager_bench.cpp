#include <benchmark/benchmark.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for FileManager (from filemgr.cpp) ----
extern "C" {
void* app_create(const char* config_json);
void  app_destroy(void* p);
char* app_process(void* p, const char* input_json);
void  app_free_string(char* s);
}

static std::string callProcess(void* app, const std::string& json)
{
    char* out = app_process(app, json.c_str());
    std::string result(out ? out : "[]");
    if (out) app_free_string(out);
    return result;
}

static void BM_CreateDestroy(benchmark::State& state)
{
    for (auto _ : state) {
        void* app = app_create(nullptr);
        app_destroy(app);
    }
}
BENCHMARK(BM_CreateDestroy);

static void BM_ListRoot(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"list","path":"/"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ListRoot);

static void BM_ListHome(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"list","path":"/home"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ListHome);

static void BM_WriteFile(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"write","path":"/bench.txt","content":"benchmark data"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_WriteFile);

static void BM_ReadNonexistent(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"read","path":"/nonexistent_bench.xyz"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ReadNonexistent);

static void BM_MkdirRemove(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        callProcess(app, R"({"action":"mkdir","path":"/bench_dir"})");
        std::string result = callProcess(app, R"({"action":"remove","path":"/bench_dir"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_MkdirRemove);

BENCHMARK_MAIN();
