#include <benchmark/benchmark.h>
#include "board.hpp"
#include "game.hpp"

// ====== Board ======

static void BM_BoardConstruct(benchmark::State& state)
{
    for (auto _ : state)
    {
        Board board(15);
        benchmark::DoNotOptimize(board.size());
    }
}
BENCHMARK(BM_BoardConstruct);

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

static void BM_BoardPlaceOccupied(benchmark::State& state)
{
    Board board(15);
    board.place(7, 7, Cell::BLACK);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.place(7, 7, Cell::WHITE));
    }
}
BENCHMARK(BM_BoardPlaceOccupied);

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

static void BM_BoardAtLarge(benchmark::State& state)
{
    int size = state.range(0);
    Board board(size);
    board.place(size / 2, size / 2, Cell::BLACK);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.at(size / 2, size / 2));
        benchmark::DoNotOptimize(board.at(0, 0));
    }
}
BENCHMARK(BM_BoardAtLarge)->Arg(15)->Arg(50)->Arg(100);

static void BM_BoardIsFull(benchmark::State& state)
{
    Board board(15);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.isFull());
    }
}
BENCHMARK(BM_BoardIsFull);

static void BM_BoardIsFullPopulated(benchmark::State& state)
{
    int size = state.range(0);
    Board board(size);
    for (int r = 0; r < size; ++r)
        for (int c = 0; c < size; ++c)
            board.place(r, c, Cell::BLACK);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(board.isFull());
    }
}
BENCHMARK(BM_BoardIsFullPopulated)->Arg(10)->Arg(30)->Arg(50);

// ====== Game ======

static void BM_GameConstruct(benchmark::State& state)
{
    for (auto _ : state)
    {
        GomokuGame game(15);
        benchmark::DoNotOptimize(game.currentPlayer());
    }
}
BENCHMARK(BM_GameConstruct);

static void BM_GameTick(benchmark::State& state)
{
    GomokuGame game(15);
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

static void BM_GameCurrentPlayer(benchmark::State& state)
{
    GomokuGame game(15);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(game.currentPlayer());
    }
}
BENCHMARK(BM_GameCurrentPlayer);

static void BM_GameGetState(benchmark::State& state)
{
    int size = state.range(0);
    GomokuGame game(size);
    for (auto _ : state)
    {
        auto s = game.getState();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_GameGetState)->Arg(10)->Arg(30)->Arg(50);

static void BM_WinDetectionHorizontal(benchmark::State& state)
{
    for (auto _ : state)
    {
        GomokuGame game(15);
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

static void BM_WinDetectionVertical(benchmark::State& state)
{
    for (auto _ : state)
    {
        GomokuGame game(15);
        int b[] = {3*15+7, 4*15+7, 5*15+7, 6*15+7, 7*15+7};
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
BENCHMARK(BM_WinDetectionVertical);

static void BM_WinDetectionDiagonal(benchmark::State& state)
{
    for (auto _ : state)
    {
        GomokuGame game(15);
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

static void BM_WinDetectionAntiDiagonal(benchmark::State& state)
{
    for (auto _ : state)
    {
        GomokuGame game(15);
        int b[] = {3*15+7, 4*15+6, 5*15+5, 6*15+4, 7*15+3};
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
BENCHMARK(BM_WinDetectionAntiDiagonal);

static void BM_GameDrawFullBoard(benchmark::State& state)
{
    for (auto _ : state)
    {
        GomokuGame game(3);
        int moves[] = {0, 1, 2, 3, 5, 4, 7, 6, 8};
        for (int i = 0; i < 9; ++i)
            game.tick(moves[i]);
        benchmark::DoNotOptimize(game.isOver());
    }
}
BENCHMARK(BM_GameDrawFullBoard);

static void BM_GameWinner(benchmark::State& state)
{
    GomokuGame game(15);
    int b[] = {7*15+3, 7*15+4, 7*15+5, 7*15+6, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(game.winner());
        benchmark::DoNotOptimize(game.hasWinner());
    }
}
BENCHMARK(BM_GameWinner);

BENCHMARK_MAIN();
