#include <benchmark/benchmark.h>
#include "board.hpp"
#include "game.hpp"

// ====== Board ======

static void BM_BoardConstruct(benchmark::State& state)
{
    for (auto _ : state)
    {
        Board board(20, 20);
        benchmark::DoNotOptimize(board.width());
    }
}
BENCHMARK(BM_BoardConstruct);

static void BM_BoardGenerateBeans(benchmark::State& state)
{
    Board board(20, 20);
    for (auto _ : state)
    {
        board.generateBeans(30, 10, 10);
    }
}
BENCHMARK(BM_BoardGenerateBeans);

static void BM_BoardHasBean(benchmark::State& state)
{
    Board board(20, 20);
    board.generateBeans(30, 10, 10);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.hasBean(5, 5));
        benchmark::DoNotOptimize(board.hasBean(10, 10));
    }
}
BENCHMARK(BM_BoardHasBean);

static void BM_BoardRemoveBean(benchmark::State& state)
{
    Board board(20, 20);
    board.generateBeans(30, 10, 10);
    int x = 0, y = 0;
    for (auto _ : state)
    {
        board.removeBean(x % 20, y % 20);
        ++x; if (x == 20) { x = 0; ++y; }
    }
}
BENCHMARK(BM_BoardRemoveBean);

static void BM_BoardIsBeanConsistency(benchmark::State& state)
{
    Board board(100, 100);
    board.generateBeans(500, 50, 50);
    int count = 0;
    for (auto _ : state)
    {
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                if (board.hasBean(x, y)) ++count;
    }
}
BENCHMARK(BM_BoardIsBeanConsistency);

// ====== PacmanGame ======

static void BM_PacmanGameConstruct(benchmark::State& state)
{
    for (auto _ : state)
    {
        PacmanGame game(20, 20);
        benchmark::DoNotOptimize(game.score());
    }
}
BENCHMARK(BM_PacmanGameConstruct);

static void BM_GameTick(benchmark::State& state)
{
    PacmanGame game(20, 20);
    int dir = 0;
    for (auto _ : state)
    {
        game.tick(dir % 4);
        ++dir;
    }
}
BENCHMARK(BM_GameTick);

static void BM_GameTickUntilGameOver(benchmark::State& state)
{
    for (auto _ : state)
    {
        PacmanGame game(10, 10);
        while (!game.isOver())
            game.tick(static_cast<int>(Direction::RIGHT));
        benchmark::DoNotOptimize(game.score());
    }
}
BENCHMARK(BM_GameTickUntilGameOver);

static void BM_GameGetState(benchmark::State& state)
{
    int size = state.range(0);
    PacmanGame game(size, size);
    for (auto _ : state)
    {
        auto s = game.getState();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_GameGetState)->Arg(10)->Arg(30)->Arg(50);

static void BM_GameWinnerQueries(benchmark::State& state)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 5; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(game.isOver());
        benchmark::DoNotOptimize(game.score());
        benchmark::DoNotOptimize(game.playerX());
    }
}
BENCHMARK(BM_GameWinnerQueries);

static void BM_GameEatAllBeans(benchmark::State& state)
{
    for (auto _ : state)
    {
        PacmanGame game(5, 5);
        int cycles = 0;
        while (!game.isOver() && cycles < 300)
        {
            for (int d = 0; d < 4; ++d)
            {
                game.tick(d);
                if (game.isOver()) break;
            }
            ++cycles;
        }
        benchmark::DoNotOptimize(game.score());
    }
}
BENCHMARK(BM_GameEatAllBeans);

BENCHMARK_MAIN();
