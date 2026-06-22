#include <benchmark/benchmark.h>
#include "direction.hpp"
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
};

static void BM_GameVirtualTick(benchmark::State& state)
{
    BenchGame g;
    for (auto _ : state)
        g.tick(1);
}
BENCHMARK(BM_GameVirtualTick);

static void BM_DirectionIsOpposite(benchmark::State& state)
{
    for (auto _ : state)
        benchmark::DoNotOptimize(isOppositeDir(Direction::UP, Direction::DOWN));
}
BENCHMARK(BM_DirectionIsOpposite);

static void BM_DirectionApplyDir(benchmark::State& state)
{
    int x = 0, y = 0;
    for (auto _ : state)
    {
        applyDir(Direction::RIGHT, x, y);
        benchmark::DoNotOptimize(x);
        benchmark::DoNotOptimize(y);
    }
}
BENCHMARK(BM_DirectionApplyDir);

BENCHMARK_MAIN();