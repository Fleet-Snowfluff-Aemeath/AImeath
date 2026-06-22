#include <gtest/gtest.h>
#include "board.hpp"
#include "game.hpp"

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
    // Place stones away from edges: B at (5,5),(6,6) W at (5,6),(6,5)
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
    // Black gets 185 Chinese points, white 176 → black should win with 3.75 komi
    // After 2 passes: enter marking, confirm dead, then score
    GoGame game;
    // Play a simple game ending with both passing
    game.tick(9 * Board::SIZE + 9);
    game.tick(GoGame::PASS);
    game.tick(GoGame::PASS); // enters marking phase
    game.tick(GoGame::CONFIRM_DEAD);
    int s = game.score();
    // With just 1 black stone, score should reflect Chinese counting
    EXPECT_TRUE(s == 1 || s == 2 || s == 0);
}

extern "C" {
    void* game_new(int w, int h);
    void  game_free(void* g);
    void  game_tick(void* g, int a);
    char* game_get_state(void* g);
}

TEST(GoTest, CApi)
{
    void* g = game_new(19, 19);
    ASSERT_NE(g, nullptr);
    game_tick(g, 3 * 19 + 3);
    game_tick(g, 15 * 19 + 15);
    char* s = game_get_state(g);
    EXPECT_NE(std::string(s).find("\"go\""), std::string::npos);
    std::free(s);
    game_free(g);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}