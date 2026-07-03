#include <gtest/gtest.h>
#include "board.hpp"
#include "game.hpp"
#include <boost/json.hpp>

TEST(GoTest, StoneOpponent)
{
    EXPECT_EQ(opponent(Stone::BLACK), Stone::WHITE);
    EXPECT_EQ(opponent(Stone::WHITE), Stone::BLACK);
    EXPECT_EQ(opponent(Stone::EMPTY), Stone::EMPTY);
}

TEST(GoTest, BoardEmpty)
{
    Board b;
    for (int r = 0; r < Board::SIZE; ++r)
        for (int c = 0; c < Board::SIZE; ++c)
            EXPECT_EQ(b.at(r, c), Stone::EMPTY);
}

TEST(GoTest, PlaceStone)
{
    Board b;
    int cap = 0;
    EXPECT_TRUE(b.place(3, 3, Stone::BLACK, cap));
    EXPECT_EQ(b.at(3, 3), Stone::BLACK);
    EXPECT_EQ(cap, 0);
}

TEST(GoTest, PlaceOccupied)
{
    Board b;
    int cap = 0;
    b.place(3, 3, Stone::BLACK, cap);
    EXPECT_FALSE(b.place(3, 3, Stone::WHITE, cap));
}

TEST(GoTest, CaptureSingleStone)
{
    Board b;
    int cap = 0;
    b.place(1, 0, Stone::WHITE, cap);
    b.place(0, 0, Stone::BLACK, cap);
    b.place(2, 0, Stone::BLACK, cap);
    b.place(1, 1, Stone::BLACK, cap);
    EXPECT_EQ(b.at(1, 0), Stone::EMPTY);
    EXPECT_EQ(cap, 1);
}

TEST(GoTest, CaptureGroup)
{
    Board b;
    int cap = 0;
    b.place(1, 1, Stone::WHITE, cap);
    b.place(1, 2, Stone::WHITE, cap);
    b.place(0, 1, Stone::BLACK, cap);
    b.place(2, 1, Stone::BLACK, cap);
    b.place(1, 0, Stone::BLACK, cap);
    EXPECT_EQ(b.at(1, 1), Stone::WHITE);
    b.place(1, 3, Stone::BLACK, cap);
    b.place(0, 2, Stone::BLACK, cap);
    b.place(2, 2, Stone::BLACK, cap);
    EXPECT_EQ(b.at(1, 1), Stone::EMPTY);
    EXPECT_EQ(b.at(1, 2), Stone::EMPTY);
}

TEST(GoTest, SuicidePrevented)
{
    Board b;
    int cap = 0;
    b.place(0, 1, Stone::BLACK, cap);
    b.place(1, 0, Stone::BLACK, cap);
    EXPECT_FALSE(b.place(0, 0, Stone::WHITE, cap));
    EXPECT_EQ(b.at(0, 0), Stone::EMPTY);
}

TEST(GoTest, PassAndEnd)
{
    GoGame game;
    EXPECT_FALSE(game.isOver());
    game.tick(GoGame::PASS);
    EXPECT_FALSE(game.isOver());
    EXPECT_EQ(game.turn(), Stone::WHITE);
    game.tick(GoGame::PASS);
    EXPECT_TRUE(game.isOver());
}

TEST(GoTest, Resign)
{
    GoGame game;
    game.tick(GoGame::RESIGN);
    EXPECT_TRUE(game.isOver());
}

TEST(GoTest, GetState)
{
    GoGame game;
    game.tick(9 * Board::SIZE + 9);
    std::string s = game.getState();
    EXPECT_NE(s.find("\"go\""), std::string::npos);
    EXPECT_NE(s.find("\"grid\""), std::string::npos);
}

TEST(GoTest, GetStateStructure)
{
    GoGame game;
    std::string s = game.getState();
    auto val = boost::json::parse(s);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("go"));
    EXPECT_FALSE(obj["grid"].as_string().empty());
    EXPECT_FALSE(obj["over"].as_bool());
}

TEST(GoTest, GameTickInvalid)
{
    GoGame game;
    game.tick(500);
    EXPECT_EQ(game.turn(), Stone::BLACK);
    game.tick(9 * Board::SIZE + 9);
    EXPECT_EQ(game.turn(), Stone::WHITE);
    game.tick(9 * Board::SIZE + 9);
    EXPECT_EQ(game.turn(), Stone::WHITE);
}

TEST(GoTest, GameTickCorner)
{
    GoGame game;
    game.tick(0); // (0,0) top-left corner
    EXPECT_EQ(game.turn(), Stone::WHITE);
    EXPECT_EQ(game.board().at(0, 0), Stone::BLACK);
}

TEST(GoTest, GameTickFarCorner)
{
    GoGame game;
    game.tick((Board::SIZE - 1) * Board::SIZE + (Board::SIZE - 1)); // bottom-right
    EXPECT_EQ(game.turn(), Stone::WHITE);
    EXPECT_EQ(game.board().at(Board::SIZE - 1, Board::SIZE - 1), Stone::BLACK);
}

// ---- Dead stone marking ----

TEST(GoTest, MarkDeadStone)
{
    GoGame game;
    game.tick(3 * Board::SIZE + 3);
    game.tick(15 * Board::SIZE + 15);
    game.tick(GoGame::PASS);
    game.tick(GoGame::PASS);
    EXPECT_TRUE(game.isOver());
    EXPECT_TRUE(game.isMarking());
    game.tick(3 * Board::SIZE + 3);
    EXPECT_TRUE(game.board().isMarkedDead(3, 3));
    game.tick(GoGame::CLEAR_DEAD);
    EXPECT_FALSE(game.board().isMarkedDead(3, 3));
    game.tick(3 * Board::SIZE + 3);
    game.tick(GoGame::CONFIRM_DEAD);
    EXPECT_FALSE(game.isMarking());
    EXPECT_EQ(game.board().at(3, 3), Stone::EMPTY);
}

// ---- Seki (双活) ----

TEST(GoTest, SekiCountsBothStones)
{
    Board b;
    int cap = 0;
    b.place(5, 5, Stone::BLACK, cap);
    b.place(5, 6, Stone::WHITE, cap);
    b.place(6, 5, Stone::WHITE, cap);
    b.place(6, 6, Stone::BLACK, cap);
    int bs = b.countScore(Stone::BLACK);
    int ws = b.countScore(Stone::WHITE);
    EXPECT_GE(bs, 2);
    EXPECT_GE(ws, 2);
}

// ---- Scoring ----

TEST(GoTest, ChineseScoringKomi)
{
    GoGame game;
    game.tick(9 * Board::SIZE + 9);
    game.tick(GoGame::PASS);
    game.tick(GoGame::PASS);
    game.tick(GoGame::CONFIRM_DEAD);
    int s = game.score();
    EXPECT_TRUE(s == 1 || s == 2 || s == 0);
}

// ---- C API ----

extern "C" {
    void* app_create(const char* config_json);
    void  app_destroy(void* p);
    char* app_process(void* p, const char* input_json);
    void  app_free_string(char* s);
    int   app_is_done(void* p);
}

TEST(GoTest, CApi)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);

    char* s = app_process(app, R"({"action":"new_game","width":19,"height":19})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"go\""), std::string::npos);
    app_free_string(s);

    s = app_process(app, R"({"action":"tick","value":66})");
    ASSERT_NE(s, nullptr);
    state = std::string(s);
    EXPECT_NE(state.find("\"grid\""), std::string::npos);
    app_free_string(s);

    app_destroy(app);
}

TEST(GoTest, CApiMultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* app = app_create(nullptr);
        ASSERT_NE(app, nullptr);
        EXPECT_EQ(app_is_done(app), 0);
        app_destroy(app);
    }
}

TEST(GoTest, CApiProcessInvalidJson)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    char* s = app_process(app, "not json");
    ASSERT_NE(s, nullptr);
    app_free_string(s);
    app_destroy(app);
}

TEST(GoTest, CApiMultipleTicks)
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
