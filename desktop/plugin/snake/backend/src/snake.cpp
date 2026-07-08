#include "snake.hpp"

Snake::Snake(int start_x, int start_y)
    : m_dir(Direction::RIGHT), m_next_dir(Direction::RIGHT)
{
    m_body.push_front({start_x, start_y});
    m_body.push_back({start_x - 1, start_y});
    m_body.push_back({start_x - 2, start_y});
}

void Snake::setDirection(Direction dir)
{
    if (isOppositeDir(m_dir, dir))
        return;
    m_next_dir = dir;
}

Position Snake::advance()
{
    m_dir = m_next_dir;
    Position new_head = m_body.front();
    applyDir(m_dir, new_head.x, new_head.y);
    m_body.push_front(new_head);
    return new_head;
}

void Snake::popTail()
{
    m_body.pop_back();
}

bool Snake::hasBodyAt(const Position& p) const
{
    for (const auto& seg : m_body)
        if (seg == p) return true;
    return false;
}

bool Snake::collidesWithSelf() const
{
    const auto& h = m_body.front();
    for (std::size_t i = 1; i < m_body.size(); ++i)
        if (m_body[i] == h) return true;
    return false;
}
