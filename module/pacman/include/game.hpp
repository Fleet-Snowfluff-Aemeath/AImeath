#pragma once

#include <game_base.hpp>
#include <game_api.hpp>
#include "board.hpp"
#include "direction.hpp"

class PacmanGame : public Game
{
public:
    PacmanGame(int width, int height);
    ~PacmanGame();

    void tick(int action) override;
    bool isOver() const override { return m_game_over; }
    int  score() const override { return m_score; }
    std::string getState() const override;

    int  beanCount() const;
    const Board& board() const { return m_board; }
    int playerX() const { return m_player_x; }
    int playerY() const { return m_player_y; }

private:
    void tickDir(Direction dir);
    std::string renderGrid() const;

    Board m_board;
    int   m_player_x, m_player_y;
    Direction m_dir;
    Direction m_next_dir;
    int   m_score;
    bool  m_game_over;
};

GAME_API_DECL()
