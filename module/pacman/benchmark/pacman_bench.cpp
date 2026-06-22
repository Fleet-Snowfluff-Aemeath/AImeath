#include <benchmark/benchmark.h>
#include "board.hpp"
#include "game.hpp"

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

static void BM_GameTick(benchmark::State& state)
{
    Game game(20, 20);
    int dir = 0;
    for (auto _ : state)
    {
        game.tick(static_cast<Direction>(dir % 4));
        ++dir;
    }
}
BENCHMARK(BM_GameTick);

static void BM_GameTickUntilGameOver(benchmark::State& state)
{
    for (auto _ : state)
    {
        Game game(10, 10);
        while (!game.isOver())
            game.tick(Direction::RIGHT);
        benchmark::DoNotOptimize(game.score());
    }
}
BENCHMARK(BM_GameTickUntilGameOver);

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

BENCHMARK_MAIN();
