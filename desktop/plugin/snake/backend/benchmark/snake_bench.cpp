#include <benchmark/benchmark.h>
#include "snake.hpp"
#include "board.hpp"
#include "game.hpp"

// ====== Snake ======

static void BM_SnakeAdvance(benchmark::State& state)
{
    Snake snake(10, 10);
    for (auto _ : state)
    {
        snake.advance();
        snake.popTail();
    }
}
BENCHMARK(BM_SnakeAdvance);

static void BM_SnakeAdvanceGrowing(benchmark::State& state)
{
    for (auto _ : state)
    {
        Snake snake(10, 10);
        snake.advance();
        snake.advance();
        snake.advance();
        benchmark::DoNotOptimize(snake.body().size());
    }
}
BENCHMARK(BM_SnakeAdvanceGrowing);

static void BM_SnakeConstruct(benchmark::State& state)
{
    for (auto _ : state)
    {
        Snake snake(10, 10);
        benchmark::DoNotOptimize(snake.head());
    }
}
BENCHMARK(BM_SnakeConstruct);

static void BM_SnakeCollisionCheck(benchmark::State& state)
{
    Snake snake(10, 10);
    for (int i = 0; i < 50; ++i)
    {
        snake.advance();
        snake.popTail();
    }
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(snake.collidesWithSelf());
    }
}
BENCHMARK(BM_SnakeCollisionCheck);

static void BM_SnakeHasBodyAt(benchmark::State& state)
{
    Snake snake(10, 10);
    for (int i = 0; i < 50; ++i)
    {
        snake.advance();
        snake.popTail();
    }
    Position test_pos{15, 10};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(snake.hasBodyAt(test_pos));
    }
}
BENCHMARK(BM_SnakeHasBodyAt);

static void BM_SnakeHasBodyAtLarge(benchmark::State& state)
{
    int N = state.range(0);
    Snake snake(0, 0);
    for (int i = 0; i < N; ++i)
    {
        snake.advance();
        snake.popTail();
    }
    Position test_pos{N / 2, 0};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(snake.hasBodyAt(test_pos));
    }
}
BENCHMARK(BM_SnakeHasBodyAtLarge)->Arg(10)->Arg(100)->Arg(500);

static void BM_SnakeSetDirection(benchmark::State& state)
{
    Snake snake(10, 10);
    for (auto _ : state)
    {
        snake.setDirection(Direction::UP);
        snake.setDirection(Direction::RIGHT);
        snake.setDirection(Direction::DOWN);
        snake.setDirection(Direction::LEFT);
    }
}
BENCHMARK(BM_SnakeSetDirection);

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

static void BM_BoardIsFoodAt(benchmark::State& state)
{
    Board board(20, 20);
    Position p = board.food();
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.isFoodAt(p));
    }
}
BENCHMARK(BM_BoardIsFoodAt);

static void BM_BoardGenerateFood(benchmark::State& state)
{
    Snake snake(10, 10);
    Board board(50, 50);
    for (auto _ : state)
    {
        board.generateFood(snake);
    }
}
BENCHMARK(BM_BoardGenerateFood);

static void BM_BoardGenerateFoodSmall(benchmark::State& state)
{
    Snake snake(5, 5);
    Board board(5, 5);
    for (auto _ : state)
    {
        board.generateFood(snake);
    }
}
BENCHMARK(BM_BoardGenerateFoodSmall);

// ====== SnakeGame ======

static void BM_SnakeGameConstructDestroy(benchmark::State& state)
{
    for (auto _ : state)
    {
        SnakeGame game(20, 20);
        benchmark::DoNotOptimize(game.score());
    }
}
BENCHMARK(BM_SnakeGameConstructDestroy);

static void BM_SnakeGameTick(benchmark::State& state)
{
    SnakeGame game(20, 20);
    for (auto _ : state)
    {
        game.tick(static_cast<int>(Direction::RIGHT));
    }
}
BENCHMARK(BM_SnakeGameTick);

static void BM_SnakeGameGetState(benchmark::State& state)
{
    int size = state.range(0);
    SnakeGame game(size, size);
    for (auto _ : state)
    {
        auto s = game.getState();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_SnakeGameGetState)->Arg(10)->Arg(50)->Arg(100);

static void BM_SnakeGameRenderGrid(benchmark::State& state)
{
    int size = state.range(0);
    SnakeGame game(size, size);
    for (auto _ : state)
    {
        auto grid = game.getState();
        benchmark::DoNotOptimize(grid);
    }
}
BENCHMARK(BM_SnakeGameRenderGrid)->Arg(10)->Arg(50)->Arg(100);

static void BM_SnakeGameSelfCollision(benchmark::State& state)
{
    for (auto _ : state)
    {
        SnakeGame game(20, 20);
        game.tick(static_cast<int>(Direction::RIGHT));
        game.tick(static_cast<int>(Direction::RIGHT));
        game.tick(static_cast<int>(Direction::DOWN));
        game.tick(static_cast<int>(Direction::DOWN));
        game.tick(static_cast<int>(Direction::LEFT));
        game.tick(static_cast<int>(Direction::LEFT));
        game.tick(static_cast<int>(Direction::UP));
        benchmark::DoNotOptimize(game.isOver());
    }
}
BENCHMARK(BM_SnakeGameSelfCollision);

BENCHMARK_MAIN();
