#include <benchmark/benchmark.h>
#include <string>

#include "chat_server.hpp"

static void nullOutput(void*, const char*) {}

static void BM_PluginCreateDestroy(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* plugin = plugin_create(nullptr);
        plugin_destroy(plugin);
    }
}
BENCHMARK(BM_PluginCreateDestroy);

static void BM_PluginCreateOnly(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* plugin = plugin_create(nullptr);
        benchmark::DoNotOptimize(plugin);
        plugin_destroy(plugin);
    }
}
BENCHMARK(BM_PluginCreateOnly);

static void BM_CommandImage(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    plugin_set_output(plugin, nullOutput, nullptr);
    for (auto _ : state)
    {
        plugin_on_input(plugin, R"({"text":"/图片"})");
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_CommandImage);

static void BM_CommandGame(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    plugin_set_output(plugin, nullOutput, nullptr);
    for (auto _ : state)
    {
        plugin_on_input(plugin, R"({"text":"/游戏 snake"})");
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_CommandGame);

static void BM_CommandVideo(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    plugin_set_output(plugin, nullOutput, nullptr);
    for (auto _ : state)
    {
        plugin_on_input(plugin, R"({"text":"/视频"})");
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_CommandVideo);

static void BM_CommandMixed(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    plugin_set_output(plugin, nullOutput, nullptr);
    for (auto _ : state)
    {
        plugin_on_input(plugin, R"({"text":"/图片"})");
        plugin_on_input(plugin, R"({"text":"/音乐"})");
        plugin_on_input(plugin, R"({"text":"/视频"})");
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_CommandMixed);

static void BM_StopAction(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* plugin = plugin_create(nullptr);
        plugin_set_output(plugin, nullOutput, nullptr);
        plugin_on_input(plugin, R"({"text":"hello"})");
        plugin_on_input(plugin, R"({"action":"stop"})");
        plugin_destroy(plugin);
    }
}
BENCHMARK(BM_StopAction);

static void BM_PollAction(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    plugin_set_output(plugin, nullOutput, nullptr);
    for (auto _ : state)
    {
        plugin_on_input(plugin, R"({"action":"poll"})");
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_PollAction);

static void BM_UnknownCommand(benchmark::State& state)
{
    void* plugin = plugin_create(nullptr);
    plugin_set_output(plugin, nullOutput, nullptr);
    for (auto _ : state)
    {
        plugin_on_input(plugin, R"({"text":"/nosuchcmd"})");
    }
    plugin_destroy(plugin);
}
BENCHMARK(BM_UnknownCommand);

BENCHMARK_MAIN();
