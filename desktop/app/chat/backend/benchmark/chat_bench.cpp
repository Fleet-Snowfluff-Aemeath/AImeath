#include <benchmark/benchmark.h>
#include <string>

#include "chat_server.hpp"

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

static void BM_AppCreateOnly(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* app = app_create(nullptr);
        benchmark::DoNotOptimize(app);
        app_destroy(app);
    }
}
BENCHMARK(BM_AppCreateOnly);

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

static void BM_StopAction(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* app = app_create(nullptr);
        app_set_output(app, nullOutput, nullptr);
        app_on_input(app, R"({"text":"hello"})");
        app_on_input(app, R"({"action":"stop"})");
        app_destroy(app);
    }
}
BENCHMARK(BM_StopAction);

static void BM_PollAction(benchmark::State& state)
{
    void* app = app_create(nullptr);
    app_set_output(app, nullOutput, nullptr);
    for (auto _ : state)
    {
        app_on_input(app, R"({"action":"poll"})");
    }
    app_destroy(app);
}
BENCHMARK(BM_PollAction);

static void BM_UnknownCommand(benchmark::State& state)
{
    void* app = app_create(nullptr);
    app_set_output(app, nullOutput, nullptr);
    for (auto _ : state)
    {
        app_on_input(app, R"({"text":"/nosuchcmd"})");
    }
    app_destroy(app);
}
BENCHMARK(BM_UnknownCommand);

BENCHMARK_MAIN();
