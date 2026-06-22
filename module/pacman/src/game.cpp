#include "game.hpp"
#include <boost/json.hpp>
#include <ctime>
#include <sstream>

static const int BEAN_COUNT = 30;

PacmanGame::PacmanGame(int width, int height)
    : m_board(width, height),
      m_player_x(width / 2), m_player_y(height / 2),
      m_dir(Direction::RIGHT), m_next_dir(Direction::RIGHT),
      m_score(0), m_game_over(false)
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    m_board.generateBeans(BEAN_COUNT, m_player_x, m_player_y);
}

PacmanGame::~PacmanGame() = default;

int PacmanGame::beanCount() const
{
    return m_board.beanCount();
}

void PacmanGame::tick(int action)
{
    tickDir(static_cast<Direction>(action));
}

void PacmanGame::tickDir(Direction dir)
{
    if (m_game_over) return;

    m_dir = dir;
    m_next_dir = dir;

    int nx = m_player_x;
    int ny = m_player_y;

    applyDir(m_dir, nx, ny);

    // Wall collision → game over
    if (nx < 0 || nx >= m_board.width() ||
        ny < 0 || ny >= m_board.height())
    {
        m_game_over = true;
        return;
    }

    m_player_x = nx;
    m_player_y = ny;

    // Eat bean if present
    if (m_board.hasBean(nx, ny))
    {
        m_board.removeBean(nx, ny);
        m_score += 10;
    }

    // Win condition: all beans eaten
    if (m_board.beanCount() == 0)
        m_game_over = true;
}

std::string PacmanGame::renderGrid() const
{
    std::string out;
    out.reserve(m_board.width() * (m_board.height() + 1));
    for (int y = 0; y < m_board.height(); ++y)
    {
        for (int x = 0; x < m_board.width(); ++x)
        {
            if (x == m_player_x && y == m_player_y)
                out += '@';
            else if (m_board.hasBean(x, y))
                out += '*';
            else
                out += ' ';
        }
        out += '\n';
    }
    return out;
}

std::string PacmanGame::getState() const
{
    boost::json::object obj;
    obj["type"]  = "pacman";
    obj["w"]     = m_board.width();
    obj["h"]     = m_board.height();
    obj["grid"]  = renderGrid();
    obj["score"] = m_score;
    obj["over"]  = m_game_over;
    return boost::json::serialize(obj);
}

extern "C"
{

void* game_new(int w, int h)
{
    return new PacmanGame(w, h);
}

GAME_API_COMMON()

}
