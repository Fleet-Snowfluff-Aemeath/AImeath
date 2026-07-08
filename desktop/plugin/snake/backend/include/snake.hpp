#pragma once

#include <deque>
#include "direction.hpp"

struct Position
{
    int x, y;
    bool operator==(const Position& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Position& o) const { return !(*this == o); }
};

class Snake
{
public:
    Snake(int start_x, int start_y);

    void setDirection(Direction dir);
    Direction direction() const { return m_dir; }

    Position advance();
    void popTail();
    bool hasBodyAt(const Position& p) const;
    bool collidesWithSelf() const;

    const Position& head() const { return m_body.front(); }
    const std::deque<Position>& body() const { return m_body; }

private:
    std::deque<Position> m_body;
    Direction m_dir;
    Direction m_next_dir;
};
