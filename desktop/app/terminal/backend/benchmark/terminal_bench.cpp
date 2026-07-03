#include <benchmark/benchmark.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for Terminal (from terminal.cpp) ----
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

static void BM_CreateOnly(benchmark::State& state)
{
    for (auto _ : state) {
        void* app = app_create(nullptr);
        benchmark::DoNotOptimize(app);
        app_destroy(app);
    }
}
BENCHMARK(BM_CreateOnly);

static void BM_ExecSyncEcho(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"exec_sync","command":"echo hello"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ExecSyncEcho);

static void BM_ExecSyncPwd(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"exec_sync","command":"pwd"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ExecSyncPwd);

static void BM_ExecSyncListFiles(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"exec_sync","command":"ls /"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ExecSyncListFiles);

static void BM_InvalidAction(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"__unknown__"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_InvalidAction);

static void BM_ResizeAction(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"action":"resize","rows":30,"cols":100})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_ResizeAction);

static void BM_MissingAction(benchmark::State& state)
{
    void* app = app_create(nullptr);
    for (auto _ : state) {
        std::string result = callProcess(app, R"({"cmd":"ls"})");
        benchmark::DoNotOptimize(result);
    }
    app_destroy(app);
}
BENCHMARK(BM_MissingAction);

BENCHMARK_MAIN();
