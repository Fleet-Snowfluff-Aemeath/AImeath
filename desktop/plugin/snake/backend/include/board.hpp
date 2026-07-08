#pragma once

#include "snake.hpp"

class Board
{
public:
    Board(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }

    void generateFood(const Snake& snake);
    bool isFoodAt(const Position& p) const { return p == m_food; }
    const Position& food() const { return m_food; }

private:
    int m_width, m_height;
    Position m_food;
};
