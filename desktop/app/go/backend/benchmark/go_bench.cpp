#include <benchmark/benchmark.h>
#include "board.hpp"
#include "game.hpp"

static void BM_BoardPlace(benchmark::State& state)
{
    for (auto _ : state) {
        Board b;
        int cap = 0;
        for (int i = 0; i < 20; ++i)
            b.place(i % 19, (i + 5) % 19, i % 2 ? Stone::BLACK : Stone::WHITE, cap);
        benchmark::DoNotOptimize(cap);
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
    for (auto _ : state) {
        GoGame game;
        for (int i = 0; i < 20; ++i)
            game.tick((i % 19) * 19 + ((i + 3) % 19));
    }
}
BENCHMARK(BM_GameTick);

static void BM_CaptureStones(benchmark::State& state)
{
    for (auto _ : state) {
        Board b;
        int cap = 0;
        b.place(0, 0, Stone::BLACK, cap);
        b.place(9, 9, Stone::BLACK, cap);
        for (int i = 0; i < 4; ++i) {
            b.place(0, 1, Stone::WHITE, cap);
            b.place(1, 0, Stone::WHITE, cap);
            b.place(1, 1, Stone::WHITE, cap);
            b.place(0, 1, Stone::EMPTY, cap);
        }
        benchmark::DoNotOptimize(cap);
    }
}
BENCHMARK(BM_CaptureStones);

static void BM_Score(benchmark::State& state)
{
    GoGame game;
    int cap = 0;
    for (int i = 0; i < 40; ++i)
        game.tick((i % 19) * 19 + ((i + 7) % 19));
    for (auto _ : state) {
        int s = game.score();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_Score);

BENCHMARK_MAIN();
