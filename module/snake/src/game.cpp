#include "game.hpp"
#include <boost/json.hpp>

#include <iostream>
#include <sstream>
#include <ctime>

SnakeGame::SnakeGame(int width, int height)
    : m_board(width, height), m_snake(width / 2, height / 2),
      m_game_over(false), m_score(0)
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    m_board.generateFood(m_snake);
}

SnakeGame::~SnakeGame()
{
    std::cout << "\033[?25h" << std::flush;
}

void SnakeGame::tick(int action)
{
    if (m_game_over) return;

    Direction dir = static_cast<Direction>(action);
    m_snake.setDirection(dir);
    update(dir);
    render();
}

void SnakeGame::update(Direction dir)
{
    auto new_head = m_snake.advance();

    if (m_board.isFoodAt(new_head))
    {
        m_score += 10;
        m_board.generateFood(m_snake);
    }
    else
    {
        m_snake.popTail();
    }

    if (m_snake.collidesWithSelf() ||
        new_head.x < 0 || new_head.x >= m_board.width() ||
        new_head.y < 0 || new_head.y >= m_board.height())
    {
        m_game_over = true;
    }
}

void SnakeGame::render()
{
    std::ostringstream out;
    out << "\033[2J\033[H\033[?25l";

    out << '+';
    for (int x = 0; x < m_board.width(); ++x) out << '-';
    out << "+\n";

    for (int y = 0; y < m_board.height(); ++y)
    {
        out << '|';
        for (int x = 0; x < m_board.width(); ++x)
        {
            Position p{x, y};
            if (p == m_snake.head())
                out << 'O';
            else if (m_snake.hasBodyAt(p))
                out << '#';
            else if (m_board.isFoodAt(p))
                out << '$';
            else
                out << ' ';
        }
        out << "|\n";
    }

    out << '+';
    for (int x = 0; x < m_board.width(); ++x) out << '-';
    out << "+\n";

    out << "Score: " << m_score << '\n';

    std::cout << out.str() << std::flush;
}

void SnakeGame::renderGameOver()
{
    std::ostringstream out;
    out << "\033[2J\033[H";
    out << "Game Over!\n";
    out << "Final Score: " << m_score << '\n';
    std::cout << out.str() << std::flush;
}

std::string SnakeGame::renderGrid() const
{
    std::ostringstream out;
    for (int y = 0; y < m_board.height(); ++y)
    {
        for (int x = 0; x < m_board.width(); ++x)
        {
            Position p{x, y};
            if (p == m_snake.head())
                out << 'O';
            else if (m_snake.hasBodyAt(p))
                out << '#';
            else if (m_board.isFoodAt(p))
                out << '$';
            else
                out << ' ';
        }
        out << '\n';
    }
    return out.str();
}

std::string SnakeGame::getState() const
{
    boost::json::object obj;
    obj["type"]  = "snake";
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
    return new SnakeGame(w, h);
}

GAME_API_COMMON()

}
