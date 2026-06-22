#include <benchmark/benchmark.h>
#include "board.hpp"
#include "game.hpp"

static void BM_BoardPlace(benchmark::State& state)
{
    Board b;
    int cap = 0;
    int r = 0;
    for (auto _ : state)
    {
        b.place(r % Board::SIZE, (r + 5) % Board::SIZE, Stone::BLACK, cap);
        ++r;
    }
}
BENCHMARK(BM_BoardPlace);

static void BM_BoardHash(benchmark::State& state)
{
    Board b;
    int cap = 0;
    for (int i = 0; i < 50; ++i)
        b.place(i % 19, (i + 7) % 19, i % 2 ? Stone::BLACK : Stone::WHITE, cap);
    for (auto _ : state)
        benchmark::DoNotOptimize(b.hash());
}
BENCHMARK(BM_BoardHash);

static void BM_GameTick(benchmark::State& state)
{
    GoGame game;
    int r = 0;
    for (auto _ : state)
    {
        game.tick((r % 19) * 19 + ((r + 3) % 19));
        ++r;
    }
}
BENCHMARK(BM_GameTick);

BENCHMARK_MAIN();