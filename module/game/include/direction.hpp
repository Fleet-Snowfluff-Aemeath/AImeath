#pragma once

/// Shared direction type for grid-based games (snake, pacman).
enum class Direction { UP, DOWN, LEFT, RIGHT };

/// Returns true when `next` is the exact opposite of `cur` (e.g. UP vs DOWN).
inline bool isOppositeDir(Direction cur, Direction next)
{
    return (cur == Direction::UP    && next == Direction::DOWN)  ||
           (cur == Direction::DOWN  && next == Direction::UP)    ||
           (cur == Direction::LEFT  && next == Direction::RIGHT) ||
           (cur == Direction::RIGHT && next == Direction::LEFT);
}

/// Apply `dir` to `(x, y)` in-place.  UP=-y, DOWN=+y, LEFT=-x, RIGHT=+x.
inline void applyDir(Direction dir, int& x, int& y)
{
    switch (dir)
    {
        case Direction::UP:    --y; break;
        case Direction::DOWN:  ++y; break;
        case Direction::LEFT:  --x; break;
        case Direction::RIGHT: ++x; break;
    }
}