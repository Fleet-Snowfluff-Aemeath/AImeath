#pragma once

#include <vector>

class Board
{
public:
    Board(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }

    bool hasBean(int x, int y) const;
    void removeBean(int x, int y);
    int  beanCount() const { return m_bean_count; }
    void generateBeans(int count, int avoid_x, int avoid_y);

private:
    int m_width, m_height;
    std::vector<bool> m_beans;
    int m_bean_count;
};
