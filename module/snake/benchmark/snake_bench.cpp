#include <benchmark/benchmark.h>
#include "snake.hpp"
#include "board.hpp"

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

static void BM_SnakeCollisionCheck(benchmark::State& state)
{
    Snake snake(10, 10);
    // Make the snake long
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

BENCHMARK_MAIN();
