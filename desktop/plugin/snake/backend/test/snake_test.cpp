#include <gtest/gtest.h>
#include "snake.hpp"
#include "board.hpp"
#include "game.hpp"
#include <boost/json.hpp>

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
    for (int i = 0; i < 3; ++i)
        snake.advance();
    EXPECT_EQ(snake.body().size(), 6u);
    snake.setDirection(Direction::DOWN);
    snake.advance();
    snake.setDirection(Direction::LEFT);
    snake.advance();
    snake.setDirection(Direction::UP);
    snake.advance();
    EXPECT_TRUE(snake.collidesWithSelf());
}

TEST(SnakeTest, NoSelfCollisionAfterNormalMove)
{
    Snake snake(5, 5);
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
    {
        Snake snake(10, 10);
        snake.setDirection(Direction::DOWN);
        snake.advance();
        snake.setDirection(Direction::UP);
        snake.advance();
        EXPECT_EQ(snake.head().y, 12);
    }
    {
        Snake snake(10, 10);
        snake.setDirection(Direction::UP);
        snake.advance();
        snake.setDirection(Direction::DOWN);
        snake.advance();
        EXPECT_EQ(snake.head().y, 8);
    }
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
    snake.advance();
    EXPECT_EQ(snake.head(), Position({5, 6}));
}

TEST(SnakeTest, HeadConsistencyAfterComplexPath)
{
    Snake snake(5, 5);
    for (int i = 0; i < 2; ++i) { snake.advance(); snake.popTail(); }
    snake.setDirection(Direction::DOWN);
    for (int i = 0; i < 2; ++i) { snake.advance(); snake.popTail(); }
    snake.setDirection(Direction::LEFT);
    for (int i = 0; i < 2; ++i) { snake.advance(); snake.popTail(); }
    snake.setDirection(Direction::UP);
    snake.advance();
    snake.popTail();
    EXPECT_EQ(snake.head(), Position({5, 6}));
    EXPECT_EQ(snake.body().size(), 3u);
}

TEST(SnakeTest, SelfCollisionGrowThenUTurn)
{
    Snake snake(5, 5);
    snake.advance();
    snake.setDirection(Direction::DOWN);
    snake.advance();
    snake.setDirection(Direction::LEFT);
    snake.advance();
    snake.setDirection(Direction::UP);
    snake.advance();
    EXPECT_TRUE(snake.collidesWithSelf());
}

TEST(SnakeTest, AdvanceManyTimesWithoutPop)
{
    Snake snake(5, 5);
    for (int i = 0; i < 10; ++i)
        snake.advance();
    EXPECT_EQ(snake.body().size(), 13u);
    EXPECT_EQ(snake.head(), Position({15, 5}));
}

TEST(SnakeTest, SnakeAtOrigin)
{
    Snake snake(0, 0);
    EXPECT_EQ(snake.head(), Position({0, 0}));
    EXPECT_EQ(snake.body().size(), 3u);
    snake.advance();
    EXPECT_EQ(snake.head(), Position({1, 0}));
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

TEST(BoardTest, FoodDefaultCenter)
{
    Board board(30, 20);
    EXPECT_EQ(board.food().x, 15);
    EXPECT_EQ(board.food().y, 10);
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
            return;
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
    Snake snake(1, 1);
    Board board(4, 4);
    for (int i = 0; i < 10; ++i)
        snake.advance();
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
    SnakeGame game(20, 20);
    for (int i = 0; i < 9; ++i)
    {
        game.tick(static_cast<int>(Direction::RIGHT));
        EXPECT_FALSE(game.isOver());
    }
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());
}

TEST(SnakeGameTest, TickGameOverTopWall)
{
    SnakeGame game(20, 20);
    game.tick(static_cast<int>(Direction::UP));
    for (int i = 0; i < 9; ++i)
    {
        game.tick(static_cast<int>(Direction::UP));
        EXPECT_FALSE(game.isOver());
    }
    game.tick(static_cast<int>(Direction::UP));
    EXPECT_TRUE(game.isOver());
}

TEST(SnakeGameTest, TickGameOverLeftWall)
{
    SnakeGame game(20, 20);
    game.tick(static_cast<int>(Direction::UP));
    game.tick(static_cast<int>(Direction::LEFT));
    for (int i = 0; i < 9; ++i)
    {
        game.tick(static_cast<int>(Direction::LEFT));
        EXPECT_FALSE(game.isOver());
    }
    game.tick(static_cast<int>(Direction::LEFT));
    EXPECT_TRUE(game.isOver());
}

TEST(SnakeGameTest, TickGameOverBottomWall)
{
    SnakeGame game(20, 20);
    game.tick(static_cast<int>(Direction::DOWN));
    for (int i = 0; i < 8; ++i)
    {
        game.tick(static_cast<int>(Direction::DOWN));
        EXPECT_FALSE(game.isOver());
    }
    game.tick(static_cast<int>(Direction::DOWN));
    EXPECT_TRUE(game.isOver());
}

TEST(SnakeGameTest, TickOnGameOverNoCrash)
{
    SnakeGame game(20, 20);
    for (int i = 0; i < 12; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());
    int final_score = game.score();
    game.tick(static_cast<int>(Direction::RIGHT));
    game.tick(static_cast<int>(Direction::DOWN));
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), final_score);
}

TEST(SnakeGameTest, TickReverseDirection)
{
    SnakeGame game(20, 20);
    game.tick(static_cast<int>(Direction::LEFT));
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
    EXPECT_FALSE(game.isOver());
}

TEST(SnakeGameTest, TickCumulativeScore)
{
    SnakeGame game(20, 20);
    EXPECT_EQ(game.score(), 0);
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_GE(game.score(), 0);
}

TEST(SnakeGameTest, ScorePreservedAfterGameOver)
{
    SnakeGame game(10, 10);
    for (int i = 0; i < 10; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    int final_score = game.score();
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_EQ(game.score(), final_score);
}

TEST(SnakeGameTest, GetStateStructure)
{
    SnakeGame game(20, 15);
    std::string state = game.getState();
    auto val = boost::json::parse(state);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("snake"));
    EXPECT_EQ(obj["w"].as_int64(), 20);
    EXPECT_EQ(obj["h"].as_int64(), 15);
    EXPECT_EQ(obj["score"].as_int64(), 0);
    EXPECT_EQ(obj["over"].as_bool(), false);
    EXPECT_TRUE(obj.contains("grid"));
    EXPECT_FALSE(obj["grid"].as_string().empty());
}

TEST(SnakeGameTest, GetStateAfterGameOver)
{
    SnakeGame game(10, 10);
    for (int i = 0; i < 10; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());

    std::string state = game.getState();
    auto val = boost::json::parse(state);
    EXPECT_TRUE(val.as_object()["over"].as_bool());
}

// ====== C API ======

extern "C" {
    void* plugin_create(const char* config_json);
    void  plugin_destroy(void* p);
    char* plugin_process(void* p, const char* input_json);
    void  plugin_free_string(char* s);
    int   plugin_is_done(void* p);
}

TEST(SnakeGameTest, CApi)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);

    char* s = plugin_process(plugin, R"({"action":"new_game","width":20,"height":20})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"snake\""), std::string::npos);
    plugin_free_string(s);

    s = plugin_process(plugin, R"({"action":"tick","value":0})");
    ASSERT_NE(s, nullptr);
    state = std::string(s);
    EXPECT_NE(state.find("\"grid\""), std::string::npos);
    plugin_free_string(s);

    plugin_destroy(plugin);
}

TEST(SnakeGameTest, CApiMultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin_is_done(plugin), 0);
        plugin_destroy(plugin);
    }
}

TEST(SnakeGameTest, CApiProcessInvalidJson)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    char* s = plugin_process(plugin, "not json");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    plugin_free_string(s);
    plugin_destroy(plugin);
}

TEST(SnakeGameTest, CApiMultipleTicks)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);

    for (int i = 0; i < 5; ++i)
    {
        char* s = plugin_process(plugin, R"({"action":"tick","value":0})");
        ASSERT_NE(s, nullptr);
        plugin_free_string(s);
    }
    plugin_destroy(plugin);
}
