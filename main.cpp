#include "gamemod.hpp"
#include <iostream>
#include <thread>
#include <chrono>

extern "C" void run_netconn_demo();

enum { DIR_RIGHT = 3 };

static GameModuleCache s_cache;

static void run_snake()
{
    auto m = s_cache.load("snake");
    if (!m) return;
    auto game = m.create(20, 20);
    if (!game) return;
    while (!game->isOver())
    {
        game->tick(DIR_RIGHT);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "=== Snake ===" << std::endl;
    std::cout << "Final Score: " << game->score() << std::endl;
}

static void run_gomoku()
{
    auto m = s_cache.load("gomoku");
    if (!m) return;
    const int SIZE = 15;
    auto game = m.create(SIZE, SIZE);
    if (!game) return;

    int moves[] = {
         3*SIZE+7,   0*SIZE+0,
         4*SIZE+7,   1*SIZE+0,
         5*SIZE+7,   2*SIZE+0,
         6*SIZE+7,   3*SIZE+1,
         7*SIZE+7,
    };

    for (int pos : moves)
    {
        game->tick(pos);
        if (game->isOver()) break;
    }

    int winner = game->score();
    std::cout << "=== Gomoku ===" << std::endl;
    std::cout << "Game Over! ";
    if (winner == 1) std::cout << "Black wins!";
    else if (winner == 2) std::cout << "White wins!";
    else std::cout << "Draw.";
    std::cout << std::endl;
}

static void run_pacman()
{
    auto m = s_cache.load("pacman");
    if (!m) return;
    auto game = m.create(20, 20);
    if (!game) return;

    int steps_in_dir = 5;
    int dirs[] = { 3, 2, 1, 2 };
    for (int phase = 0; phase < 8; ++phase)
    {
        int d = dirs[phase % 4];
        for (int i = 0; i < steps_in_dir; ++i)
        {
            game->tick(d);
            if (game->isOver()) break;
        }
        if (game->isOver()) break;
    }
    while (!game->isOver())
        game->tick(DIR_RIGHT);

    std::cout << "=== Pac-Man ===" << std::endl;
    std::cout << "Final Score: " << game->score() << std::endl;
}

static void run_go()
{
    auto m = s_cache.load("go");
    if (!m) return;
    auto game = m.create(19, 19);
    if (!game) return;

    // Simple demo: B plays 4 corners, W plays center, then both pass
    int blacks[] = { 3*19+3, 3*19+15, 15*19+3, 15*19+15 };
    int whites[] = { 9*19+9, 9*19+8, 9*19+10, 8*19+9, 10*19+9 };
    for (int i = 0; i < 4; ++i) { game->tick(blacks[i]); game->tick(whites[i]); }
    game->tick(whites[4]);
    game->tick(-1); // B pass
    game->tick(-1); // W pass

    std::cout << "=== Go ===" << std::endl;
    std::cout << "Final Score (1=B win, 2=W win): " << game->score() << std::endl;
}

static void run_netconn()
{
    std::cout << "=== NetConn (Core) ===" << std::endl;
    run_netconn_demo();
}

int main()
{
    run_netconn();
    run_pacman();
    run_gomoku();
    run_snake();
    run_go();
    return 0;
}
