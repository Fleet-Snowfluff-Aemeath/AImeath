#include <benchmark/benchmark.h>
#include <string>

// ---- C ABI for ChatApp (from chat_server.cpp) ----
extern "C" {
void* app_create(const char* config_json);
void  app_destroy(void* p);
void  app_on_input(void* p, const char* input_json);
int   app_is_done(void* p);
typedef void (*app_output_fn)(void* userdata, const char* json);
void  app_set_output(void* p, app_output_fn cb, void* userdata);
}

static void nullOutput(void*, const char*) {}

static void BM_AppCreateDestroy(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* app = app_create(nullptr);
        app_destroy(app);
    }
}
BENCHMARK(BM_AppCreateDestroy);

static void BM_CommandImage(benchmark::State& state)
{
    void* app = app_create(nullptr);
    app_set_output(app, nullOutput, nullptr);
    for (auto _ : state)
    {
        app_on_input(app, R"({"text":"/图片"})");
    }
    app_destroy(app);
}
BENCHMARK(BM_CommandImage);

static void BM_CommandGame(benchmark::State& state)
{
    void* app = app_create(nullptr);
    app_set_output(app, nullOutput, nullptr);
    for (auto _ : state)
    {
        app_on_input(app, R"({"text":"/游戏 snake"})");
    }
    app_destroy(app);
}
BENCHMARK(BM_CommandGame);

static void BM_CommandVideo(benchmark::State& state)
{
    void* app = app_create(nullptr);
    app_set_output(app, nullOutput, nullptr);
    for (auto _ : state)
    {
        app_on_input(app, R"({"text":"/视频"})");
    }
    app_destroy(app);
}
BENCHMARK(BM_CommandVideo);

static void BM_CommandMixed(benchmark::State& state)
{
    void* app = app_create(nullptr);
    app_set_output(app, nullOutput, nullptr);
    for (auto _ : state)
    {
        app_on_input(app, R"({"text":"/图片"})");
        app_on_input(app, R"({"text":"/音乐"})");
        app_on_input(app, R"({"text":"/视频"})");
    }
    app_destroy(app);
}
BENCHMARK(BM_CommandMixed);

BENCHMARK_MAIN();
