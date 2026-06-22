#pragma once

#include <game_base.hpp>
#include <game_api.hpp>
#include "snake.hpp"
#include "board.hpp"

class SnakeGame : public Game
{
public:
    SnakeGame(int width, int height);
    ~SnakeGame();

    void tick(int action) override;
    bool isOver() const override { return m_game_over; }
    int  score() const override { return m_score; }
    std::string getState() const override;

    const Board& board() const { return m_board; }
    const Snake& snake() const { return m_snake; }

private:
    void update(Direction dir);
    void render();
    void renderGameOver();
    std::string renderGrid() const;

    Board m_board;
    Snake m_snake;
    bool m_game_over;
    int m_score;
};

GAME_API_DECL()
