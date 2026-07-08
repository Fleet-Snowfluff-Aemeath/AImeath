#include <gtest/gtest.h>
#include "direction.hpp"

TEST(DirectionTest, AllDirectionsDistinct)
{
    EXPECT_NE(static_cast<int>(Direction::UP), static_cast<int>(Direction::DOWN));
    EXPECT_NE(static_cast<int>(Direction::LEFT), static_cast<int>(Direction::RIGHT));
    EXPECT_NE(static_cast<int>(Direction::UP), static_cast<int>(Direction::LEFT));
}

TEST(DirectionTest, IsOppositeDir)
{
    EXPECT_TRUE(isOppositeDir(Direction::UP, Direction::DOWN));
    EXPECT_TRUE(isOppositeDir(Direction::DOWN, Direction::UP));
    EXPECT_TRUE(isOppositeDir(Direction::LEFT, Direction::RIGHT));
    EXPECT_TRUE(isOppositeDir(Direction::RIGHT, Direction::LEFT));
    EXPECT_FALSE(isOppositeDir(Direction::UP, Direction::LEFT));
    EXPECT_FALSE(isOppositeDir(Direction::DOWN, Direction::RIGHT));
    EXPECT_FALSE(isOppositeDir(Direction::UP, Direction::UP));
}

TEST(DirectionTest, IsOppositeDirSelf)
{
    EXPECT_FALSE(isOppositeDir(Direction::UP, Direction::UP));
    EXPECT_FALSE(isOppositeDir(Direction::DOWN, Direction::DOWN));
    EXPECT_FALSE(isOppositeDir(Direction::LEFT, Direction::LEFT));
    EXPECT_FALSE(isOppositeDir(Direction::RIGHT, Direction::RIGHT));
}

TEST(DirectionTest, PluginlyDir)
{
    int x = 5, y = 5;
    applyDir(Direction::UP, x, y);
    EXPECT_EQ(x, 5); EXPECT_EQ(y, 4);
    applyDir(Direction::DOWN, x, y);
    EXPECT_EQ(x, 5); EXPECT_EQ(y, 5);
    applyDir(Direction::LEFT, x, y);
    EXPECT_EQ(x, 4); EXPECT_EQ(y, 5);
    applyDir(Direction::RIGHT, x, y);
    EXPECT_EQ(x, 5); EXPECT_EQ(y, 5);
}

TEST(DirectionTest, PluginlyDirAllFour)
{
    int x = 0, y = 0;
    applyDir(Direction::RIGHT, x, y);
    EXPECT_EQ(x, 1); EXPECT_EQ(y, 0);
    applyDir(Direction::DOWN, x, y);
    EXPECT_EQ(x, 1); EXPECT_EQ(y, 1);
    applyDir(Direction::LEFT, x, y);
    EXPECT_EQ(x, 0); EXPECT_EQ(y, 1);
    applyDir(Direction::UP, x, y);
    EXPECT_EQ(x, 0); EXPECT_EQ(y, 0);
}
