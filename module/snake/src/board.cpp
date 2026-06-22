#include "board.hpp"
#include <cstdlib>

Board::Board(int width, int height)
    : m_width(width), m_height(height), m_food{width / 2, height / 2}
{
}

void Board::generateFood(const Snake& snake)
{
    do
    {
        m_food.x = std::rand() % m_width;
        m_food.y = std::rand() % m_height;
    } while (snake.hasBodyAt(m_food));
}
