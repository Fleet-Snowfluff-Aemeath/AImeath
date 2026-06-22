#include "board.hpp"

Board::Board(int size)
    : m_size(size), m_cells(size * size, Cell::EMPTY)
{
}

bool Board::inBounds(int row, int col) const
{
    return row >= 0 && row < m_size && col >= 0 && col < m_size;
}

Cell Board::at(int row, int col) const
{
    return m_cells[row * m_size + col];
}

bool Board::place(int row, int col, Cell cell)
{
    if (!inBounds(row, col)) return false;
    auto& c = m_cells[row * m_size + col];
    if (c != Cell::EMPTY) return false;
    c = cell;
    return true;
}

bool Board::isFull() const
{
    for (auto c : m_cells)
        if (c == Cell::EMPTY) return false;
    return true;
}
