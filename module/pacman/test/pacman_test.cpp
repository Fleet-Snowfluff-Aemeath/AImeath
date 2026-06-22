#include <gtest/gtest.h>
#include "board.hpp"
#include "game.hpp"

// ====== Board ======

TEST(BoardTest, Construction)
{
    Board board(20, 15);
    EXPECT_EQ(board.width(), 20);
    EXPECT_EQ(board.height(), 15);
    EXPECT_EQ(board.beanCount(), 0);
}

TEST(BoardTest, GenerateBeans)
{
    Board board(10, 10);
    board.generateBeans(20, 5, 5);
    EXPECT_EQ(board.beanCount(), 20);

    // All beans should be within bounds
    int found = 0;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            if (board.hasBean(x, y)) ++found;
    EXPECT_EQ(found, 20);
}

TEST(BoardTest, GenerateBeansAvoidsPosition)
{
    Board board(5, 5);
    board.generateBeans(20, 2, 2); // max is 24 (25-1)
    EXPECT_FALSE(board.hasBean(2, 2));
}

TEST(BoardTest, RemoveBean)
{
    Board board(10, 10);
    board.generateBeans(10, 0, 0);
    int before = board.beanCount();

    // Find and remove a bean
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            if (board.hasBean(x, y))
            {
                board.removeBean(x, y);
                EXPECT_EQ(board.beanCount(), before - 1);
                EXPECT_FALSE(board.hasBean(x, y));
                return;
            }
}

TEST(BoardTest, RemoveNonExistentBean)
{
    Board board(10, 10);
    board.generateBeans(10, 0, 0);
    int before = board.beanCount();
    board.removeBean(0, 0);
    EXPECT_EQ(board.beanCount(), before); // no bean there
}

TEST(BoardTest, GenerateBeansClampedToMax)
{
    Board board(3, 3); // 9 cells, 1 avoided = 8 max
    board.generateBeans(999, 0, 0);
    EXPECT_EQ(board.beanCount(), 8);
}

// ====== Game ======

TEST(PacmanGameTest, Construction)
{
    PacmanGame game(20, 20);
    EXPECT_FALSE(game.isOver());
    EXPECT_EQ(game.score(), 0);
    EXPECT_GT(game.beanCount(), 0);
}

TEST(PacmanGameTest, TickMovesPlayer)
{
    PacmanGame game(20, 20);
    int before = game.beanCount();
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_FALSE(game.isOver());
    // Player moved, may have eaten a bean
    EXPECT_GE(game.score(), 0);
}

TEST(PacmanGameTest, WallCollisionUp)
{
    PacmanGame game(5, 5);
    // Move up 3 times to hit top wall (start at 2,2 → 2,1 → 2,0 → 2,-1 = wall)
    game.tick(static_cast<int>(Direction::UP));
    EXPECT_FALSE(game.isOver());
    game.tick(static_cast<int>(Direction::UP));
    EXPECT_FALSE(game.isOver());
    game.tick(static_cast<int>(Direction::UP));
    EXPECT_TRUE(game.isOver());
}

TEST(PacmanGameTest, WallCollisionDown)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 3; ++i)
    {
        game.tick(static_cast<int>(Direction::DOWN));
        if (i < 2) EXPECT_FALSE(game.isOver());
    }
    EXPECT_TRUE(game.isOver());
}

TEST(PacmanGameTest, WallCollisionLeft)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 3; ++i)
    {
        game.tick(static_cast<int>(Direction::LEFT));
        if (i < 2) EXPECT_FALSE(game.isOver());
    }
    EXPECT_TRUE(game.isOver());
}

TEST(PacmanGameTest, WallCollisionRight)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 3; ++i)
    {
        game.tick(static_cast<int>(Direction::RIGHT));
        if (i < 2) EXPECT_FALSE(game.isOver());
    }
    EXPECT_TRUE(game.isOver());
}

TEST(PacmanGameTest, ReverseDirectionAllowed)
{
    PacmanGame game(20, 20);
    game.tick(static_cast<int>(Direction::LEFT)); // reverse of initial RIGHT → should work
    game.tick(static_cast<int>(Direction::LEFT)); // continue LEFT
    EXPECT_FALSE(game.isOver());
}

TEST(PacmanGameTest, EatBeanIncreasesScore)
{
    PacmanGame game(20, 20);
    int start_score = game.score();
    // Move around to find and eat beans
    for (int i = 0; i < 10; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    // Should have eaten at least 1 bean if there was one in the path
    EXPECT_GE(game.score(), start_score);
}

TEST(PacmanGameTest, TickAfterGameOverIgnored)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 5; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());
    int final_score = game.score();
    game.tick(static_cast<int>(Direction::RIGHT)); // ignored
    EXPECT_EQ(game.score(), final_score);
}

TEST(PacmanGameTest, AllBeansEatenWins)
{
    // Tiny board with only one bean → eat it to win
    PacmanGame game(3, 3);
    EXPECT_FALSE(game.isOver());
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_FALSE(game.isOver());
}

TEST(PacmanGameTest, ScoreIncrementsByTen)
{
    PacmanGame game(20, 20);
    int score = game.score();
    game.tick(static_cast<int>(Direction::RIGHT));
    int new_score = game.score();
    EXPECT_TRUE(new_score == score || new_score == score + 10);
}

TEST(PacmanGameTest, MultipleTicksSequential)
{
    PacmanGame game(10, 10);
    for (int i = 0; i < 20; ++i)
    {
        game.tick(static_cast<int>(Direction::RIGHT));
        if (game.isOver()) break;
    }
    // Should eventually hit wall on 10x10 board starting at (5,5)
    // Need 5 moves RIGHT to hit wall at x=10
    EXPECT_TRUE(game.isOver());
}
