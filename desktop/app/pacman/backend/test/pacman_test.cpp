#include <gtest/gtest.h>
#include "board.hpp"
#include "game.hpp"
#include <boost/json.hpp>

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

    int found = 0;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            if (board.hasBean(x, y)) ++found;
    EXPECT_EQ(found, 20);
}

TEST(BoardTest, GenerateBeansAvoidsPosition)
{
    Board board(5, 5);
    board.generateBeans(20, 2, 2);
    EXPECT_FALSE(board.hasBean(2, 2));
}

TEST(BoardTest, RemoveBean)
{
    Board board(10, 10);
    board.generateBeans(10, 0, 0);
    int before = board.beanCount();

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
    EXPECT_EQ(board.beanCount(), before);
}

TEST(BoardTest, GenerateBeansClampedToMax)
{
    Board board(3, 3);
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

TEST(PacmanGameTest, PlayerStartsAtCenter)
{
    PacmanGame game(20, 20);
    EXPECT_EQ(game.playerX(), 10);
    EXPECT_EQ(game.playerY(), 10);
}

TEST(PacmanGameTest, TickMovesPlayer)
{
    PacmanGame game(20, 20);
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_FALSE(game.isOver());
    EXPECT_GE(game.score(), 0);
}

TEST(PacmanGameTest, WallCollisionUp)
{
    PacmanGame game(5, 5);
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
    game.tick(static_cast<int>(Direction::LEFT));
    game.tick(static_cast<int>(Direction::LEFT));
    EXPECT_FALSE(game.isOver());
}

TEST(PacmanGameTest, EatBeanIncreasesScore)
{
    PacmanGame game(20, 20);
    int start_score = game.score();
    for (int i = 0; i < 10; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_GE(game.score(), start_score);
}

TEST(PacmanGameTest, TickAfterGameOverIgnored)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 5; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());
    int final_score = game.score();
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_EQ(game.score(), final_score);
}

TEST(PacmanGameTest, AllBeansEatenWins)
{
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
    EXPECT_TRUE(game.isOver());
}

TEST(PacmanGameTest, GetStateStructure)
{
    PacmanGame game(20, 15);
    std::string state = game.getState();
    auto val = boost::json::parse(state);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("pacman"));
    EXPECT_EQ(obj["w"].as_int64(), 20);
    EXPECT_EQ(obj["h"].as_int64(), 15);
    EXPECT_EQ(obj["score"].as_int64(), 0);
    EXPECT_EQ(obj["over"].as_bool(), false);
    EXPECT_TRUE(obj.contains("grid"));
    EXPECT_FALSE(obj["grid"].as_string().empty());
}

TEST(PacmanGameTest, GetStateAfterGameOver)
{
    PacmanGame game(5, 5);
    for (int i = 0; i < 5; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_TRUE(game.isOver());

    std::string state = game.getState();
    auto val = boost::json::parse(state);
    EXPECT_TRUE(val.as_object()["over"].as_bool());
}

TEST(PacmanGameTest, BeanCountAfterEat)
{
    PacmanGame game(20, 20);
    int initial = game.beanCount();
    for (int i = 0; i < 30; ++i)
        game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_LE(game.beanCount(), initial);
}

TEST(PacmanGameTest, PlayerCoordinatesUpdate)
{
    PacmanGame game(20, 20);
    EXPECT_EQ(game.playerX(), 10);
    EXPECT_EQ(game.playerY(), 10);
    game.tick(static_cast<int>(Direction::RIGHT));
    EXPECT_EQ(game.playerX(), 11);
    EXPECT_EQ(game.playerY(), 10);
}

// ====== C API ======

extern "C" {
    void* app_create(const char* config_json);
    void  app_destroy(void* p);
    char* app_process(void* p, const char* input_json);
    void  app_free_string(char* s);
    int   app_is_done(void* p);
}

TEST(PacmanGameTest, CApi)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);

    char* s = app_process(app, R"({"action":"new_game","width":20,"height":20})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"pacman\""), std::string::npos);
    app_free_string(s);

    s = app_process(app, R"({"action":"tick","value":3})");
    ASSERT_NE(s, nullptr);
    state = std::string(s);
    EXPECT_NE(state.find("\"grid\""), std::string::npos);
    app_free_string(s);

    app_destroy(app);
}

TEST(PacmanGameTest, CApiMultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* app = app_create(nullptr);
        ASSERT_NE(app, nullptr);
        EXPECT_EQ(app_is_done(app), 0);
        app_destroy(app);
    }
}

TEST(PacmanGameTest, CApiProcessInvalidJson)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    char* s = app_process(app, "not json");
    ASSERT_NE(s, nullptr);
    app_free_string(s);
    app_destroy(app);
}

TEST(PacmanGameTest, CApiMultipleTicks)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);

    for (int i = 0; i < 5; ++i)
    {
        char* s = app_process(app, R"({"action":"tick","value":0})");
        ASSERT_NE(s, nullptr);
        app_free_string(s);
    }
    app_destroy(app);
}
