#include "board.hpp"
#include <cstring>

Board::Board()
{
    std::memset(m_grid, 0, sizeof(m_grid));
}

// Row/col in 0..SIZE1-1 grid space. Returns true if this is a border sentinel cell.
static bool isBorder(int gridRow, int gridCol)
{
    return gridRow == 0 || gridRow == Board::SIZE + 1
        || gridCol == 0 || gridCol == Board::SIZE + 1;
}

Stone Board::at(int row, int col) const
{
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE)
        return Stone::EMPTY;
    return m_grid[toIndex(row + 1, col + 1)];
}

int Board::countLiberties(int idx, Stone color, bool* visited) const
{
    if (visited[idx]) return 0;
    int gr = idx / SIZE1;
    int gc = idx % SIZE1;

    if (m_grid[idx] == Stone::EMPTY)
    {
        visited[idx] = true;
        return isBorder(gr, gc) ? 0 : 1;
    }
    if (m_grid[idx] != color) return 0;
    visited[idx] = true;

    static const int dirs[4] = { -SIZE1, SIZE1, -1, 1 };
    int libs = 0;
    for (int d : dirs)
        libs += countLiberties(idx + d, color, visited);
    return libs;
}

void Board::collectGroup(int idx, Stone color, bool* visited,
                          std::vector<int>& out) const
{
    if (visited[idx]) return;
    if (m_grid[idx] != color) return;
    visited[idx] = true;
    out.push_back(idx);

    static const int dirs[4] = { -SIZE1, SIZE1, -1, 1 };
    for (int d : dirs)
        collectGroup(idx + d, color, visited, out);
}

static void removeStones(Stone* grid, const std::vector<int>& positions)
{
    for (int p : positions)
        grid[p] = Stone::EMPTY;
}

bool Board::place(int row, int col, Stone s, int& captured)
{
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return false;
    int idx = toIndex(row + 1, col + 1);
    if (m_grid[idx] != Stone::EMPTY) return false;

    m_grid[idx] = s;
    captured = 0;
    Stone opp = opponent(s);

    bool visitBuf[SIZE1 * SIZE1];
    static const int dirs[4] = { -SIZE1, SIZE1, -1, 1 };

    for (int d : dirs)
    {
        int ni = idx + d;
        if (m_grid[ni] != opp) continue;
        std::memset(visitBuf, 0, sizeof(visitBuf));
        if (countLiberties(ni, opp, visitBuf) == 0)
        {
            std::vector<int> group;
            std::memset(visitBuf, 0, sizeof(visitBuf));
            collectGroup(ni, opp, visitBuf, group);
            captured += static_cast<int>(group.size());
            removeStones(m_grid, group);
        }
    }

    std::memset(visitBuf, 0, sizeof(visitBuf));
    if (countLiberties(idx, s, visitBuf) == 0 && captured == 0)
    {
        m_grid[idx] = Stone::EMPTY;
        return false;
    }
    return true;
}

bool Board::isLegal(int row, int col, Stone s) const
{
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return false;
    int idx = toIndex(row + 1, col + 1);
    if (m_grid[idx] != Stone::EMPTY) return false;

    // Simulate on a copy
    Stone simGrid[SIZE1 * SIZE1];
    std::memcpy(simGrid, m_grid, sizeof(simGrid));
    simGrid[idx] = s;
    Stone opp = opponent(s);
    static const int dirs[4] = { -SIZE1, SIZE1, -1, 1 };

    int capCount = 0;
    for (int d : dirs)
    {
        int ni = idx + d;
        if (simGrid[ni] != opp) continue;

        // Flood-fill to check liberties of opponent group
        bool vis[SIZE1 * SIZE1] = {};
        std::vector<int> stack;
        stack.push_back(ni);
        vis[ni] = true;
        int libs = 0;
        while (!stack.empty())
        {
            int cur = stack.back(); stack.pop_back();
            for (int dd : dirs)
            {
                int nxt = cur + dd;
                if (simGrid[nxt] == Stone::EMPTY)
                {
                    int gr2 = nxt / SIZE1, gc2 = nxt % SIZE1;
                    if (!isBorder(gr2, gc2) && !vis[nxt])
                    {
                        vis[nxt] = true;
                        ++libs;
                    }
                }
                else if (simGrid[nxt] == opp && !vis[nxt])
                {
                    vis[nxt] = true;
                    stack.push_back(nxt);
                }
            }
        }
        if (libs == 0) return true; // can capture
    }

    // Self-capture check
    {
        bool vis[SIZE1 * SIZE1] = {};
        std::vector<int> stack;
        stack.push_back(idx);
        vis[idx] = true;
        int libs = 0;
        while (!stack.empty())
        {
            int cur = stack.back(); stack.pop_back();
            for (int dd : dirs)
            {
                int nxt = cur + dd;
                if (simGrid[nxt] == Stone::EMPTY)
                {
                    int gr2 = nxt / SIZE1, gc2 = nxt % SIZE1;
                    if (!isBorder(gr2, gc2) && !vis[nxt])
                    {
                        vis[nxt] = true;
                        ++libs;
                    }
                }
                else if (simGrid[nxt] == s && !vis[nxt])
                {
                    vis[nxt] = true;
                    stack.push_back(nxt);
                }
            }
        }
        if (libs == 0) return false;
    }
    return true;
}

uint64_t Board::hash() const
{
    uint64_t h = 14695981039346656037ULL;
    for (int r = 0; r < SIZE; ++r)
    {
        for (int c = 0; c < SIZE; ++c)
        {
            int v = static_cast<int>(m_grid[toIndex(r + 1, c + 1)]);
            h ^= static_cast<uint64_t>(v);
            h *= 1099511628211ULL;
            h ^= static_cast<uint64_t>(r * SIZE + c);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

int Board::countTerritory(int idx, Stone territoryOf, bool* visited) const
{
    if (visited[idx]) return 0;
    int gr = idx / SIZE1, gc = idx % SIZE1;
    if (isBorder(gr, gc)) { visited[idx] = true; return 0; }

    if (m_grid[idx] == territoryOf)
    {
        visited[idx] = true;
        return 1;
    }
    // If we hit opponent stone, this empty area is not territory
    if (m_grid[idx] != Stone::EMPTY) { visited[idx] = true; return -9999; }
    visited[idx] = true;

    static const int dirs[4] = { -SIZE1, SIZE1, -1, 1 };
    int total = 0;
    for (int d : dirs)
    {
        int t = countTerritory(idx + d, territoryOf, visited);
        if (t < 0) return t;
        total += t;
    }
    return total;
}

bool Board::markDead(int row, int col)
{
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return false;
    int idx = toIndex(row + 1, col + 1);
    if (m_grid[idx] == Stone::EMPTY) return false;
    m_dead[idx] = !m_dead[idx];
    return true;
}

int Board::removeDeadStones()
{
    int count = 0;
    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
        {
            int idx = toIndex(r + 1, c + 1);
            if (m_dead[idx] && m_grid[idx] != Stone::EMPTY)
            {
                m_grid[idx] = Stone::EMPTY;
                m_dead[idx] = false;
                ++count;
            }
        }
    return count;
}

void Board::clearDeadMarks()
{
    std::memset(m_dead, 0, sizeof(m_dead));
}

bool Board::isMarkedDead(int row, int col) const
{
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return false;
    return m_dead[toIndex(row + 1, col + 1)];
}

int Board::countScore(Stone s) const
{
    bool visited[SIZE1 * SIZE1] = {};
    int score = 0;

    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            if (at(r, c) == s) ++score;

    for (int r = 0; r < SIZE; ++r)
    {
        for (int c = 0; c < SIZE; ++c)
        {
            if (at(r, c) != Stone::EMPTY) continue;
            int idx = toIndex(r + 1, c + 1);
            if (visited[idx]) continue;

            bool visitCopy[SIZE1 * SIZE1];
            std::memcpy(visitCopy, visited, sizeof(visited));
            int t = countTerritory(idx, s, visitCopy);
            if (t > 0)
            {
                score += t;
                std::memcpy(visited, visitCopy, sizeof(visited));
            }
        }
    }
    return score;
}

std::string Board::renderGrid() const
{
    std::string out;
    out.reserve(SIZE * (SIZE + 1));
    for (int r = 0; r < SIZE; ++r)
    {
        for (int c = 0; c < SIZE; ++c)
            out += stoneChar(at(r, c));
        out += '\n';
    }
    return out;
}