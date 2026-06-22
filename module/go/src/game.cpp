#include "game.hpp"
#include <game_api.hpp>
#include <cstdint>
#include <boost/json.hpp>

GoGame::GoGame(int size)
    : m_board()
{
    (void)size;
}

void GoGame::tick(int action)
{
    // Marking phase (after game ends, before score confirmation)
    if (m_marking)
    {
        if (action == CLEAR_DEAD)
        {
            m_board.clearDeadMarks();
            return;
        }
        if (action == CONFIRM_DEAD)
        {
            m_board.removeDeadStones();
            m_marking = false;
            return;
        }
        int row = action / Board::SIZE;
        int col = action % Board::SIZE;
        m_board.markDead(row, col);
        return;
    }

    if (m_over) return;

    if (action == RESIGN)
    {
        m_over = true;
        m_resigned = true;
        return;
    }

    if (action == PASS)
    {
        ++m_passes;
        if (m_passes >= 2)
        {
            m_over = true;
            m_marking = true;   // enter dead-stone marking phase
        }
        else
            m_turn = opponent(m_turn);
        return;
    }

    // Playing phase: place a stone
    int row = action / Board::SIZE;
    int col = action % Board::SIZE;

    if (row < 0 || row >= Board::SIZE || col < 0 || col >= Board::SIZE)
        return;

    if (!m_board.isLegal(row, col, m_turn))
        return;

    Board copy = m_board;
    int captured = 0;
    if (!copy.place(row, col, m_turn, captured))
        return;

    uint64_t newHash = copy.hash();
    for (uint64_t h : m_history)
        if (h == newHash) return;

    m_board.place(row, col, m_turn, captured);
    if (m_turn == Stone::BLACK)
        m_capBlack += captured;
    else
        m_capWhite += captured;

    m_history.push_back(m_board.hash());
    m_passes = 0;
    m_turn = opponent(m_turn);
}

int GoGame::score() const
{
    int blackScore = m_board.countScore(Stone::BLACK);
    int whiteScore = m_board.countScore(Stone::WHITE);
    double result = static_cast<double>(blackScore - whiteScore) - 2.0 * m_komi;
    if (result > 0) return 1;
    if (result < 0) return 2;
    return 0;
}

std::string GoGame::getState() const
{
    boost::json::object obj;
    obj["type"]    = "go";
    obj["s"]       = Board::SIZE;
    obj["grid"]    = m_board.renderGrid();
    obj["turn"]    = m_turn == Stone::BLACK ? "B" : "W";
    obj["over"]    = m_over;
    obj["marking"] = m_marking;
    obj["score"]   = m_marking ? 0 : score();
    obj["passes"]  = m_passes;
    obj["capsB"]   = m_capBlack;
    obj["capsW"]   = m_capWhite;
    obj["komi"]    = m_komi;

    // Dead stone mask for frontend rendering
    std::string deadMask;
    deadMask.reserve(Board::SIZE * Board::SIZE);
    for (int r = 0; r < Board::SIZE; ++r)
        for (int c = 0; c < Board::SIZE; ++c)
            deadMask += m_board.isMarkedDead(r, c) ? '1' : '0';
    obj["deadMask"] = deadMask;

    return boost::json::serialize(obj);
}

// ---- C API ----
extern "C"
{

void* game_new(int w, int h)
{
    (void)h;
    return new GoGame(w);
}

GAME_API_COMMON()

} // extern "C"
