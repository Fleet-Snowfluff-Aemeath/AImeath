#include <benchmark/benchmark.h>
#include "toolbox.hpp"

static void BM_ToolboxParseUrl(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            parseUrl("http://example.com:8080/path/to/resource"));
    }
}
BENCHMARK(BM_ToolboxParseUrl);

static void BM_ToolboxParseWsUrl(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            parseUrl("ws://chat.example.com:9000/room?id=123"));
    }
}
BENCHMARK(BM_ToolboxParseWsUrl);

BENCHMARK_MAIN();
