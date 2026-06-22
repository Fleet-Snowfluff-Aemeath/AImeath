#include <benchmark/benchmark.h>
#include "board.hpp"
#include "game.hpp"

static void BM_BoardPlace(benchmark::State& state)
{
    Board board(15);
    int idx = 0;
    for (auto _ : state)
    {
        int r = (idx / 15) % 15;
        int c = idx % 15;
        board.place(r, c, Cell::BLACK);
        ++idx;
    }
}
BENCHMARK(BM_BoardPlace);

static void BM_BoardAt(benchmark::State& state)
{
    Board board(15);
    board.place(7, 7, Cell::BLACK);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.at(7, 7));
        benchmark::DoNotOptimize(board.at(0, 0));
    }
}
BENCHMARK(BM_BoardAt);

static void BM_BoardIsFull(benchmark::State& state)
{
    Board board(15);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.isFull());
    }
}
BENCHMARK(BM_BoardIsFull);

static void BM_GameTick(benchmark::State& state)
{
    Game game(15);
    int pos = 0;
    for (auto _ : state)
    {
        int r = (pos / 15) % 15;
        int c = pos % 15;
        game.tick(r * 15 + c);
        ++pos;
    }
}
BENCHMARK(BM_GameTick);

static void BM_WinDetectionHorizontal(benchmark::State& state)
{
    for (auto _ : state)
    {
        Game game(15);
        int b[] = {7*15+3, 7*15+4, 7*15+5, 7*15+6, 7*15+7};
        int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
        for (int i = 0; i < 4; ++i)
        {
            game.tick(b[i]);
            game.tick(w[i]);
        }
        game.tick(b[4]);
        benchmark::DoNotOptimize(game.isOver());
    }
}
BENCHMARK(BM_WinDetectionHorizontal);

static void BM_WinDetectionDiagonal(benchmark::State& state)
{
    for (auto _ : state)
    {
        Game game(15);
        int b[] = {3*15+3, 4*15+4, 5*15+5, 6*15+6, 7*15+7};
        int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
        for (int i = 0; i < 4; ++i)
        {
            game.tick(b[i]);
            game.tick(w[i]);
        }
        game.tick(b[4]);
        benchmark::DoNotOptimize(game.isOver());
    }
}
BENCHMARK(BM_WinDetectionDiagonal);

BENCHMARK_MAIN();
