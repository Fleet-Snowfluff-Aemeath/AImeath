#include "game.hpp"
#include <boost/json.hpp>

GomokuGame::GomokuGame(int size)
    : m_board(size), m_cur(Cell::BLACK), m_over(false), m_winner(Cell::EMPTY)
{
}

GomokuGame::~GomokuGame() = default;

int GomokuGame::currentPlayer() const
{
    return m_cur == Cell::BLACK ? 1 : 2;
}

int GomokuGame::score() const
{
    if (m_winner == Cell::BLACK) return 1;
    if (m_winner == Cell::WHITE) return 2;
    return 0;
}

void GomokuGame::tick(int pos)
{
    if (m_over) return;

    int size = m_board.size();
    int row = pos / size;
    int col = pos % size;

    if (!m_board.place(row, col, m_cur))
        return;

    if (checkWin(row, col))
    {
        m_winner = m_cur;
        m_over = true;
        return;
    }

    if (m_board.isFull())
    {
        m_over = true;
        return;
    }

    m_cur = (m_cur == Cell::BLACK) ? Cell::WHITE : Cell::BLACK;
}

bool GomokuGame::checkWin(int row, int col) const
{
    static const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (auto [dr, dc] : dirs)
    {
        int cnt = 1 + countDir(row, col, dr, dc) + countDir(row, col, -dr, -dc);
        if (cnt >= 5) return true;
    }
    return false;
}

int GomokuGame::countDir(int row, int col, int dr, int dc) const
{
    Cell cell = m_board.at(row, col);
    int cnt = 0;
    int r = row + dr;
    int c = col + dc;
    while (m_board.inBounds(r, c) && m_board.at(r, c) == cell)
    {
        ++cnt;
        r += dr;
        c += dc;
    }
    return cnt;
}

std::string GomokuGame::renderGrid() const
{
    int s = m_board.size();
    std::string out;
    out.reserve(s * (s + 1));
    for (int r = 0; r < s; ++r)
    {
        for (int c = 0; c < s; ++c)
        {
            Cell cell = m_board.at(r, c);
            out += (cell == Cell::EMPTY) ? '.' : (cell == Cell::BLACK) ? 'B' : 'W';
        }
        out += '\n';
    }
    return out;
}

std::string GomokuGame::getState() const
{
    boost::json::object obj;
    obj["type"]   = "gomoku";
    obj["s"]      = m_board.size();
    obj["grid"]   = renderGrid();
    obj["cur"]    = currentPlayer();
    obj["score"]  = score();
    obj["over"]   = m_over;
    obj["winner"] = winner();
    return boost::json::serialize(obj);
}

extern "C"
{

void* game_new(int w, int h)
{
    (void)h;
    return new GomokuGame(w);
}

GAME_API_COMMON()

}
