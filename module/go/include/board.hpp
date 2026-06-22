#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class Stone : uint8_t { EMPTY = 0, BLACK = 1, WHITE = 2 };

inline Stone opponent(Stone s)
{
    return s == Stone::BLACK ? Stone::WHITE
         : s == Stone::WHITE ? Stone::BLACK
         : Stone::EMPTY;
}

inline char stoneChar(Stone s)
{
    return s == Stone::BLACK ? 'B' : s == Stone::WHITE ? 'W' : '.';
}

/// 19x19 Go board with capture, suicide, ko detection, and dead-stone marking.
class Board
{
public:
    static constexpr int SIZE = 19;

    Board();

    Stone at(int row, int col) const;
    int  size() const { return SIZE; }

    /// Try placing a stone.  Returns true on success.
    /// Sets `captured` to the number of opponent stones captured.
    bool place(int row, int col, Stone s, int& captured);

    /// Whether placing s at (row,col) is legal (not suicide unless captures).
    bool isLegal(int row, int col, Stone s) const;

    /// Toggle dead-stone mark at (row,col). Returns true if a stone exists there.
    bool markDead(int row, int col);

    /// Remove all currently marked dead stones.
    int removeDeadStones();

    /// Clear all dead marks.
    void clearDeadMarks();

    /// Returns true if (row,col) is marked dead.
    bool isMarkedDead(int row, int col) const;

    /// Count territory + living stones for the given color (Chinese scoring).
    int countScore(Stone s) const;

    /// Simple board hash for ko detection.
    uint64_t hash() const;

    std::string renderGrid() const;

private:
    static constexpr int SIZE1 = SIZE + 2;
    int toIndex(int row, int col) const { return row * SIZE1 + col; }

    Stone  m_grid[SIZE1 * SIZE1];
    bool   m_dead[SIZE1 * SIZE1]{};

    int  countLiberties(int idx, Stone color, bool* visited) const;
    void collectGroup(int idx, Stone color, bool* visited,
                      std::vector<int>& out) const;
    int  countTerritory(int idx, Stone territoryOf, bool* visited) const;
};