#include <benchmark/benchmark.h>
#include "game_base.hpp"

class BenchGame : public Game
{
    int m_score = 0;
    bool m_over = false;
public:
    void tick(int a) override { m_score += a; }
    bool isOver() const override { return m_over; }
    int score() const override { return m_score; }
    std::string getState() const override { return std::to_string(m_score); }
    void setOver(bool o) { m_over = o; }
};

static void BM_GameVirtualTick(benchmark::State& state)
{
    BenchGame g;
    for (auto _ : state)
        g.tick(1);
}
BENCHMARK(BM_GameVirtualTick);

static void BM_GameVirtualIsOver(benchmark::State& state)
{
    BenchGame g;
    for (auto _ : state)
        benchmark::DoNotOptimize(g.isOver());
}
BENCHMARK(BM_GameVirtualIsOver);

static void BM_GameVirtualScore(benchmark::State& state)
{
    BenchGame g;
    g.tick(5);
    for (auto _ : state)
        benchmark::DoNotOptimize(g.score());
}
BENCHMARK(BM_GameVirtualScore);

static void BM_GameVirtualGetState(benchmark::State& state)
{
    BenchGame g;
    g.tick(5);
    for (auto _ : state)
    {
        auto s = g.getState();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_GameVirtualGetState);

static void BM_GameFullLifecycle(benchmark::State& state)
{
    for (auto _ : state)
    {
        BenchGame g;
        g.tick(1);
        g.tick(2);
        g.tick(3);
        benchmark::DoNotOptimize(g.score());
        benchmark::DoNotOptimize(g.isOver());
        benchmark::DoNotOptimize(g.getState());
    }
}
BENCHMARK(BM_GameFullLifecycle);

BENCHMARK_MAIN();
