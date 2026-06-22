#pragma once

#include <game_base.hpp>
#include <game_api.hpp>
#include "board.hpp"

class GomokuGame : public Game
{
public:
    explicit GomokuGame(int size);
    ~GomokuGame();

    void tick(int pos) override;
    bool isOver() const override { return m_over; }
    int  score() const override;
    std::string getState() const override;

    int  currentPlayer() const;
    const Board& board() const { return m_board; }
    bool hasWinner() const { return m_winner != Cell::EMPTY; }
    int  winner() const { return m_winner == Cell::BLACK ? 1 : m_winner == Cell::WHITE ? 2 : 0; }

private:
    bool checkWin(int row, int col) const;
    int  countDir(int row, int col, int dr, int dc) const;
    std::string renderGrid() const;

    Board m_board;
    Cell  m_cur;
    bool  m_over;
    Cell  m_winner;
};

GAME_API_DECL()
