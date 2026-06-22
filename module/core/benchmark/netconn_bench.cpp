#include <benchmark/benchmark.h>
#include "wsutil.hpp"

static void BM_NetConnParseUrl(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            parseUrl("http://example.com:8080/path/to/resource"));
    }
}
BENCHMARK(BM_NetConnParseUrl);

static void BM_NetConnParseWsUrl(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            parseUrl("ws://chat.example.com:9000/room?id=123"));
    }
}
BENCHMARK(BM_NetConnParseWsUrl);

BENCHMARK_MAIN();
