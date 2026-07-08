#include "board.hpp"
#include <cstdlib>

Board::Board(int width, int height)
    : m_width(width), m_height(height),
      m_beans(width * height, false), m_bean_count(0)
{
}

bool Board::hasBean(int x, int y) const
{
    return m_beans[y * m_width + x];
}

void Board::removeBean(int x, int y)
{
    int idx = y * m_width + x;
    if (m_beans[idx])
    {
        m_beans[idx] = false;
        --m_bean_count;
    }
}

void Board::generateBeans(int count, int avoid_x, int avoid_y)
{
    int total = m_width * m_height;
    if (count > total - 1)
        count = total - 1;

    m_bean_count = 0;
    std::fill(m_beans.begin(), m_beans.end(), false);

    int placed = 0;
    while (placed < count)
    {
        int x = std::rand() % m_width;
        int y = std::rand() % m_height;
        if (x == avoid_x && y == avoid_y)
            continue;
        int idx = y * m_width + x;
        if (!m_beans[idx])
        {
            m_beans[idx] = true;
            ++placed;
            ++m_bean_count;
        }
    }
}
