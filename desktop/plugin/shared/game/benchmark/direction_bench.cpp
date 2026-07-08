#include <benchmark/benchmark.h>
#include "direction.hpp"

static void BM_DirectionIsOpposite(benchmark::State& state)
{
    for (auto _ : state)
        benchmark::DoNotOptimize(isOppositeDir(Direction::UP, Direction::DOWN));
}
BENCHMARK(BM_DirectionIsOpposite);

static void BM_DirectionIsOppositeFalse(benchmark::State& state)
{
    for (auto _ : state)
        benchmark::DoNotOptimize(isOppositeDir(Direction::UP, Direction::LEFT));
}
BENCHMARK(BM_DirectionIsOppositeFalse);

static void BM_DirectionPluginlyDir(benchmark::State& state)
{
    int x = 0, y = 0;
    for (auto _ : state)
    {
        applyDir(Direction::RIGHT, x, y);
        benchmark::DoNotOptimize(x);
        benchmark::DoNotOptimize(y);
    }
}
BENCHMARK(BM_DirectionPluginlyDir);

static void BM_DirectionPluginlyDirAllFour(benchmark::State& state)
{
    int x = 0, y = 0;
    for (auto _ : state)
    {
        applyDir(Direction::UP, x, y);
        applyDir(Direction::RIGHT, x, y);
        applyDir(Direction::DOWN, x, y);
        applyDir(Direction::LEFT, x, y);
        benchmark::DoNotOptimize(x);
        benchmark::DoNotOptimize(y);
    }
}
BENCHMARK(BM_DirectionPluginlyDirAllFour);

BENCHMARK_MAIN();
