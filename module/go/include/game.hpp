#pragma once

#include <game_base.hpp>
#include "board.hpp"
#include <vector>
#include <cstdint>

/// Action: >=0 = place stone (play) or toggle dead mark (marking phase)
/// PASS=-1, RESIGN=-2, CLEAR_DEAD=-3, CONFIRM_DEAD=-4
class GoGame : public Game
{
public:
    static constexpr int PASS        = -1;
    static constexpr int RESIGN      = -2;
    static constexpr int CLEAR_DEAD  = -3;
    static constexpr int CONFIRM_DEAD = -4;

    explicit GoGame(int size = Board::SIZE);
    ~GoGame() override = default;

    void tick(int action) override;
    bool isOver() const override { return m_over; }
    int  score() const override;
    std::string getState() const override;

    const Board& board() const { return m_board; }
    Stone turn() const { return m_turn; }
    int  captures(Stone s) const { return s == Stone::BLACK ? m_capBlack : m_capWhite; }
    double komi() const { return m_komi; }
    bool isMarking() const { return m_marking; }

private:
    Board m_board;
    Stone m_turn{Stone::BLACK};
    int   m_passes{0};
    bool  m_over{false};
    bool  m_marking{false};
    double m_komi{3.75};   // Chinese rule: 黑贴3又3/4子

    int  m_capBlack{0};
    int  m_capWhite{0};

    std::vector<uint64_t> m_history;
    bool m_resigned{false};
};