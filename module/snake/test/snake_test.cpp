#include <gtest/gtest.h>
#include "snake.hpp"
#include "board.hpp"
#include "game.hpp"

// ====== Snake ======

TEST(SnakeTest, InitialState)
{
    Snake snake(10, 10);
    EXPECT_EQ(snake.head(), Position({10, 10}));
    EXPECT_EQ(snake.direction(), Direction::RIGHT);
    EXPECT_EQ(snake.body().size(), 3u);
    EXPECT_TRUE(snake.hasBodyAt({10, 10}));
    EXPECT_TRUE(snake.hasBodyAt({9, 10}));
    EXPECT_TRUE(snake.hasBodyAt({8, 10}));
}

TEST(SnakeTest, AdvanceRight)
{
    Snake snake(5, 5);
    Position new_head = snake.advance();
    EXPECT_EQ(new_head, Position({6, 5}));
    EXPECT_EQ(snake.head(), Position({6, 5}));
    EXPECT_EQ(snake.body().size(), 4u);
}

TEST(SnakeTest, AdvanceUp)
{
    Snake snake(5, 5);
    snake.setDirection(Direction::UP);
    Position new_head = snake.advance();
    EXPECT_EQ(new_head, Position({5, 4}));
}

TEST(SnakeTest, AdvanceDown)
{
    Snake snake(5, 5);
    snake.setDirection(Direction::DOWN);
    Position new_head = snake.advance();
    EXPECT_EQ(new_head, Position({5, 6}));
}

TEST(SnakeTest, AdvanceLeft)
{
    Snake snake(5, 5);
    snake.setDirection(Direction::UP);
    snake.advance();
    snake.setDirection(Direction::LEFT);
    Position new_head = snake.advance();
    EXPECT_EQ(new_head, Position({4, 4}));
}

TEST(SnakeTest, PopTail)
{
    Snake snake(5, 5);
    snake.advance();
    EXPECT_EQ(snake.body().size(), 4u);
    snake.popTail();
    EXPECT_EQ(snake.body().size(), 3u);
    EXPECT_EQ(snake.head(), Position({6, 5}));
}

TEST(SnakeTest, CannotReverseDirection)
{
    Snake snake(5, 5);
    snake.setDirection(Direction::LEFT);
    snake.advance();
    EXPECT_EQ(snake.head(), Position({6, 5}));
}

TEST(SnakeTest, DirectionBuffered)
{
    Snake snake(5, 5);
    snake.setDirection(Direction::UP);
    EXPECT_EQ(snake.direction(), Direction::RIGHT);
    snake.advance();
    EXPECT_EQ(snake.direction(), Direction::UP);
}

TEST(SnakeTest, SelfCollision)
{
    Snake snake(5, 5);
    // Eat 3 times to grow
    for (int i = 0; i < 3; ++i)
        snake.advance();
    EXPECT_EQ(snake.body().size(), 6u);
    // Turn down, eat
    snake.setDirection(Direction::DOWN);
    snake.advance();
    // Turn left, eat
    snake.setDirection(Direction::LEFT);
    snake.advance();
    // Turn up — head goes into own body
    snake.setDirection(Direction::UP);
    snake.advance();
    EXPECT_TRUE(snake.collidesWithSelf());
}

TEST(SnakeTest, NoSelfCollisionAfterNormalMove)
{
    Snake snake(5, 5);
    // Straight line with pop — never collides
    for (int i = 0; i < 20; ++i)
    {
        snake.advance();
        snake.popTail();
    }
    EXPECT_FALSE(snake.collidesWithSelf());
}

TEST(SnakeTest, HasBodyAt)
{
    Snake snake(10, 10);
    EXPECT_TRUE(snake.hasBodyAt({10, 10}));
    EXPECT_TRUE(snake.hasBodyAt({9, 10}));
    EXPECT_TRUE(snake.hasBodyAt({8, 10}));
    EXPECT_FALSE(snake.hasBodyAt({7, 10}));
    EXPECT_FALSE(snake.hasBodyAt({10, 9}));
}

TEST(SnakeTest, AdvanceMultipleWithoutPop)
{
    Snake snake(5, 5);
    EXPECT_EQ(snake.advance(), Position({6, 5}));
    EXPECT_EQ(snake.advance(), Position({7, 5}));
    EXPECT_EQ(snake.body().size(), 5u);
    snake.popTail();
    EXPECT_EQ(snake.body().size(), 4u);
    snake.popTail();
    EXPECT_EQ(snake.body().size(), 3u);
    EXPECT_EQ(snake.head(), Position({7, 5}));
}

TEST(SnakeTest, MultiplePopTail)
{
    Snake snake(10, 10);
    for (int i = 0; i < 5; ++i)
    {
        snake.advance();
        snake.popTail();
    }
    EXPECT_EQ(snake.body().size(), 3u);
    EXPECT_EQ(snake.head(), Position({15, 10}));
}

TEST(SnakeTest, PopTailThenAdvance)
{
    Snake snake(5, 5);
    snake.advance();
    EXPECT_EQ(snake.body().size(), 4u);
    snake.popTail();
    EXPECT_EQ(snake.body().size(), 3u);
    snake.advance();
    EXPECT_EQ(snake.body().size(), 4u);
    EXPECT_EQ(snake.head(), Position({7, 5}));
}

TEST(SnakeTest, AllReverseDirectionsBlocked)
{
    // Start RIGHT, turn DOWN → then UP should be blocked
    {
        Snake snake(10, 10);
        snake.setDirection(Direction::DOWN);
        snake.advance();
        snake.setDirection(Direction::UP);
        snake.advance();
        EXPECT_EQ(snake.head().y, 12);
    }
    // Start RIGHT, turn UP → then DOWN should be blocked
    {
        Snake snake(10, 10);
        snake.setDirection(Direction::UP);
        snake.advance();
        snake.setDirection(Direction::DOWN);
        snake.advance();
        EXPECT_EQ(snake.head().y, 8);
    }
    // Start RIGHT, turn UP, then LEFT → then RIGHT should be blocked
    {
        Snake snake(10, 10);
        snake.setDirection(Direction::UP);
        snake.advance();
        snake.setDirection(Direction::LEFT);
        snake.advance();
        snake.setDirection(Direction::RIGHT);
        snake.advance();
        EXPECT_EQ(snake.head().x, 8);
    }
    // Start RIGHT, turn UP, then RIGHT → then LEFT should be blocked
    {
        Snake snake(10, 10);
        snake.setDirection(Direction::UP);
        snake.advance();
        snake.setDirection(Direction::RIGHT);
        snake.advance();
        snake.setDirection(Direction::LEFT);
        snake.advance();
        EXPECT_EQ(snake.head().x, 12);
    }
}

TEST(SnakeTest, ConsecutiveDirectionChanges)
{
    Snake snake(5, 5);
    snake.setDirection(Direction::UP);
    snake.setDirection(Direction::LEFT);
    snake.setDirection(Direction::DOWN);
    // Only last one (DOWN) applies
    snake.advance();
    EXPECT_EQ(snake.head(), Position({5, 6}));
}

TEST(SnakeTest, HeadConsistencyAfterComplexPath)
{
    Snake snake(5, 5);
    // R, R, D, D, L, L, U — traces a Π shape
    for (int i = 0; i < 2; ++i) { snake.advance(); snake.popTail(); }
    snake.setDirection(Direction::DOWN);
    for (int i = 0; i < 2; ++i) { snake.advance(); snake.popTail(); }
    snake.setDirection(Direction::LEFT);
    for (int i = 0; i < 2; ++i) { snake.advance(); snake.popTail(); }
    snake.setDirection(Direction::UP);
    snake.advance();
    snake.popTail();
    // After Π: head at (5,6), body size 3
    EXPECT_EQ(snake.head(), Position({5, 6}));
    EXPECT_EQ(snake.body().size(), 3u);
}

TEST(SnakeTest, SelfCollisionGrowThenUTurn)
{
    Snake snake(5, 5);
    // Grow to length 4
    snake.advance();
    // Turn
    snake.setDirection(Direction::DOWN);
    snake.advance();
    snake.setDirection(Direction::LEFT);
    snake.advance();
    snake.setDirection(Direction::UP);
    snake.advance();
    EXPECT_TRUE(snake.collidesWithSelf());
}

// ====== Board ======

TEST(BoardTest, FoodWithinBounds)
{
    Board board(20, 15);
    EXPECT_GE(board.food().x, 0);
    EXPECT_LT(board.food().x, 20);
    EXPECT_GE(board.food().y, 0);
    EXPECT_LT(board.food().y, 15);
}

TEST(BoardTest, boardDimensions)
{
    Board board(30, 25);
    EXPECT_EQ(board.width(), 30);
    EXPECT_EQ(board.height(), 25);
}

TEST(BoardTest, FoodChangesOnGenerate)
{
    Snake snake(10, 10);
    Board board(20, 20);
    Position first = board.food();
    for (int i = 0; i < 10; ++i)
    {
        board.generateFood(snake);
        if (board.food().x != first.x || board.food().y != first.y)
            return;  // position changed at least once
    }
    ADD_FAILURE() << "food position never changed after 10 regenerate";
}

TEST(BoardTest, FoodNotOnSnake)
{
    Snake snake(10, 10);
    Board board(5, 5);
    for (int i = 0; i < 100; ++i)
    {
        board.generateFood(snake);
        EXPECT_FALSE(snake.hasBodyAt(board.food()));
    }
}

TEST(BoardTest, FoodNotOnSnakeLargeBoard)
{
    Snake snake(50, 50);
    Board board(100, 100);
    for (int i = 0; i < 200; ++i)
    {
        board.generateFood(snake);
        EXPECT_FALSE(snake.hasBodyAt(board.food()));
    }
}

TEST(BoardTest, FoodNotOnSnakeFullBody)
{
    // Snake fills most of a small board
    Snake snake(1, 1);
    Board board(4, 4);
    for (int i = 0; i < 10; ++i)
        snake.advance();
    // Now snake is long, food must avoid all body segments
    for (int i = 0; i < 50; ++i)
    {
        board.generateFood(snake);
        EXPECT_FALSE(snake.hasBodyAt(board.food()));
    }
}

TEST(BoardTest, IsFoodAt)
{
    Board board(10, 10);
    Position f = board.food();
    EXPECT_TRUE(board.isFoodAt(f));
    EXPECT_FALSE(board.isFoodAt({f.x + 1, f.y}));
}

// ====== Game ======

TEST(SnakeGameTest, ConstructAndDestroy)
{
    SnakeGame game(10, 10);
}

TEST(SnakeGameTest, InitialState)
{
    SnakeGame game(20, 20);
    EXPECT_FALSE(game.isOver());
    EXPECT_EQ(game.score(), 0);
}

TEST(SnakeGameTest, TickMovesSnake)
{
    SnakeGame game(20, 20);
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_FALSE(game.isOver());
}

TEST(SnakeGameTest, TickGameOverWall)
{
    // Snake starts at (10,10) going RIGHT, board 20x20.
    // After 10 ticks to the right, head reaches x=20 → out of bounds.
    SnakeGame game(20, 20);
    for (int i = 0; i < 9; ++i)
    {
        game.tick(static_cast<int>(Direction::RIGHT));
        EXPECT_FALSE(game.isOver());
    }
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());
}

TEST(SnakeGameTest, TickOnGameOverNoCrash)
{
    SnakeGame game(20, 20);
    for (int i = 0; i < 12; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());
    // Additional ticks after game over should not crash
    game.tick(static_cast<int>(Direction::RIGHT));
    game.tick(static_cast<int>(Direction::DOWN));
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 0);
}

TEST(SnakeGameTest, TickReverseDirection)
{
    // Reverse direction from tick is silently ignored
    SnakeGame game(20, 20);
    game.tick(static_cast<int>(Direction::LEFT));   // LEFT is reverse of initial RIGHT → ignored
    EXPECT_FALSE(game.isOver());
}

TEST(SnakeGameTest, MultipleTicks)
{
    SnakeGame game(20, 20);
    for (int i = 0; i < 5; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_FALSE(game.isOver());
    game.tick(static_cast<int>(Direction::DOWN));
    for (int i = 0; i < 5; ++i)
        game.tick(static_cast<int>(Direction::DOWN));
    // Snake went right 5, down 6 → should not hit wall yet
    EXPECT_FALSE(game.isOver());
}

TEST(SnakeGameTest, TickCumulativeScore)
{
    // Score only increases when eating food (random position).
    // We can't control food position but we can verify tick doesn't corrupt score.
    SnakeGame game(20, 20);
    EXPECT_EQ(game.score(), 0);
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_GE(game.score(), 0);
}

TEST(SnakeGameTest, ScorePreservedAfterGameOver)
{
    SnakeGame game(10, 10);
    // Snake 10x10, starts at (5,5) going RIGHT → hits wall at x=10 after ~5 ticks
    for (int i = 0; i < 10; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    int final_score = game.score();
    game.tick(static_cast<int>(Direction::RIGHT));  // tick after game over
    EXPECT_EQ(game.score(), final_score);
}
