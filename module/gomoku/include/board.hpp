#pragma once

#include <vector>

enum class Cell { EMPTY, BLACK, WHITE };

class Board
{
public:
    explicit Board(int size);
    Board(const Board&) = delete;
    Board& operator=(const Board&) = delete;

    int size() const { return m_size; }
    bool inBounds(int row, int col) const;
    Cell at(int row, int col) const;
    bool place(int row, int col, Cell cell);
    bool isFull() const;

private:
    int m_size;
    std::vector<Cell> m_cells;
};
