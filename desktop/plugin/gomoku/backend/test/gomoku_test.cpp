#include <gtest/gtest.h>
#include "board.hpp"
#include "game.hpp"
#include <boost/json.hpp>

// ====== Board ======

TEST(BoardTest, Construction)
{
    Board board(15);
    EXPECT_EQ(board.size(), 15);
    EXPECT_FALSE(board.isFull());
}

TEST(BoardTest, InBounds)
{
    Board board(10);
    EXPECT_TRUE(board.inBounds(0, 0));
    EXPECT_TRUE(board.inBounds(9, 9));
    EXPECT_TRUE(board.inBounds(5, 5));
    EXPECT_FALSE(board.inBounds(-1, 0));
    EXPECT_FALSE(board.inBounds(0, -1));
    EXPECT_FALSE(board.inBounds(10, 0));
    EXPECT_FALSE(board.inBounds(0, 10));
}

TEST(BoardTest, PlaceAndAt)
{
    Board board(10);
    EXPECT_TRUE(board.place(3, 4, Cell::BLACK));
    EXPECT_EQ(board.at(3, 4), Cell::BLACK);
    EXPECT_EQ(board.at(4, 4), Cell::EMPTY);
}

TEST(BoardTest, PlaceOccupied)
{
    Board board(10);
    EXPECT_TRUE(board.place(5, 5, Cell::BLACK));
    EXPECT_FALSE(board.place(5, 5, Cell::WHITE));
    EXPECT_EQ(board.at(5, 5), Cell::BLACK);
}

TEST(BoardTest, PlaceOutOfBounds)
{
    Board board(10);
    EXPECT_FALSE(board.place(-1, 0, Cell::BLACK));
    EXPECT_FALSE(board.place(0, 10, Cell::BLACK));
    EXPECT_FALSE(board.place(10, 0, Cell::BLACK));
}

TEST(BoardTest, IsFull)
{
    Board board(3);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            board.place(r, c, Cell::BLACK);
    EXPECT_TRUE(board.isFull());
}

TEST(BoardTest, IsFullNotFull)
{
    Board board(5);
    EXPECT_FALSE(board.isFull());
    board.place(0, 0, Cell::BLACK);
    EXPECT_FALSE(board.isFull());
}

TEST(BoardTest, EmptyBoardAllEmpty)
{
    Board board(8);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            EXPECT_EQ(board.at(r, c), Cell::EMPTY);
}

TEST(BoardTest, PlaceBothColors)
{
    Board board(10);
    EXPECT_TRUE(board.place(0, 0, Cell::BLACK));
    EXPECT_TRUE(board.place(0, 1, Cell::WHITE));
    EXPECT_EQ(board.at(0, 0), Cell::BLACK);
    EXPECT_EQ(board.at(0, 1), Cell::WHITE);
}

// ====== Game ======

TEST(GomokuGameTest, Construction)
{
    GomokuGame game(15);
    EXPECT_FALSE(game.isOver());
    EXPECT_EQ(game.score(), 0);
    EXPECT_EQ(game.currentPlayer(), 1);
    EXPECT_FALSE(game.hasWinner());
    EXPECT_EQ(game.winner(), 0);
    EXPECT_EQ(game.board().size(), 15);
}

TEST(GomokuGameTest, TickValid)
{
    GomokuGame game(15);
    game.tick(7 * 15 + 7);
    EXPECT_EQ(game.currentPlayer(), 2);
    EXPECT_FALSE(game.isOver());
}

TEST(GomokuGameTest, TickOutOfBounds)
{
    GomokuGame game(15);
    int before = game.currentPlayer();
    game.tick(-1);
    EXPECT_EQ(game.currentPlayer(), before);
    game.tick(15 * 15);
    EXPECT_EQ(game.currentPlayer(), before);
}

TEST(GomokuGameTest, TickOccupied)
{
    GomokuGame game(15);
    game.tick(7 * 15 + 7);
    int after = game.currentPlayer();
    game.tick(7 * 15 + 7);
    EXPECT_EQ(game.currentPlayer(), after);
}

TEST(GomokuGameTest, TurnsAlternate)
{
    GomokuGame game(15);
    EXPECT_EQ(game.currentPlayer(), 1);
    game.tick(0 * 15 + 0);
    EXPECT_EQ(game.currentPlayer(), 2);
    game.tick(1 * 15 + 0);
    EXPECT_EQ(game.currentPlayer(), 1);
}

TEST(GomokuGameTest, HorizontalWin)
{
    GomokuGame game(15);
    int b[] = {7*15+3, 7*15+4, 7*15+5, 7*15+6, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        EXPECT_FALSE(game.isOver());
        game.tick(w[i]);
        EXPECT_FALSE(game.isOver());
    }
    game.tick(b[4]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 1);
}

TEST(GomokuGameTest, VerticalWin)
{
    GomokuGame game(15);
    int b[] = {3*15+7, 4*15+7, 5*15+7, 6*15+7, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 1);
}

TEST(GomokuGameTest, DiagonalWin)
{
    GomokuGame game(15);
    int b[] = {3*15+3, 4*15+4, 5*15+5, 6*15+6, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 1);
}

TEST(GomokuGameTest, AntiDiagonalWin)
{
    GomokuGame game(15);
    int b[] = {3*15+7, 4*15+6, 5*15+5, 6*15+4, 7*15+3};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 1);
}

TEST(GomokuGameTest, WhiteWins)
{
    GomokuGame game(15);
    int b[] = {0*15+0, 2*15+2, 4*15+4, 6*15+6};
    int w[] = {5*15+3, 5*15+4, 5*15+5, 5*15+6, 5*15+7};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(1*15+1);
    EXPECT_FALSE(game.isOver());
    game.tick(w[4]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 2);
}

TEST(GomokuGameTest, DrawFullBoard)
{
    GomokuGame game(3);
    int moves[] = {0, 1, 2, 3, 5, 4, 7, 6, 8};
    for (int i = 0; i < 8; ++i)
    {
        game.tick(moves[i]);
        EXPECT_FALSE(game.isOver());
    }
    game.tick(moves[8]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 0);
}

TEST(GomokuGameTest, GameOverStopsMoves)
{
    GomokuGame game(15);
    int b[] = {7*15+3, 7*15+4, 7*15+5, 7*15+6, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    EXPECT_TRUE(game.isOver());
    game.tick(4*15+4);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 1);
}

TEST(GomokuGameTest, ScorePreservedAfterGameOver)
{
    GomokuGame game(15);
    int b[] = {7*15+3, 7*15+4, 7*15+5, 7*15+6, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    int final = game.score();
    game.tick(5*15+5);
    EXPECT_EQ(game.score(), final);
}

TEST(GomokuGameTest, InvalidMoveDoesNotChangePlayer)
{
    GomokuGame game(15);
    game.tick(7*15+7);
    EXPECT_EQ(game.currentPlayer(), 2);
    game.tick(7*15+7);
    EXPECT_EQ(game.currentPlayer(), 2);
}

TEST(GomokuGameTest, BlockingWin)
{
    GomokuGame game(15);
    int b1[] = {7*15+4, 7*15+5, 7*15+6, 7*15+7};
    int w1[] = {0*15+0, 1*15+0, 2*15+0, 7*15+8};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b1[i]);
        game.tick(w1[i]);
    }
    EXPECT_FALSE(game.isOver());

    int b2[] = {8*15+10, 9*15+10, 10*15+10, 11*15+10};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b2[i]);
        game.tick(13*15 + i);
    }
    EXPECT_FALSE(game.isOver());
    game.tick(12*15+10);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 1);
}

TEST(GomokuGameTest, FirstMoveCorner)
{
    GomokuGame game(15);
    game.tick(0); // top-left corner row=0,col=0
    EXPECT_EQ(game.currentPlayer(), 2);
    EXPECT_FALSE(game.isOver());
}

TEST(GomokuGameTest, GetStateStructure)
{
    GomokuGame game(15);
    std::string state = game.getState();
    auto val = boost::json::parse(state);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("gomoku"));
    EXPECT_EQ(obj["s"].as_int64(), 15);
    EXPECT_EQ(obj["cur"].as_int64(), 1);
    EXPECT_EQ(obj["score"].as_int64(), 0);
    EXPECT_EQ(obj["over"].as_bool(), false);
    EXPECT_EQ(obj["winner"].as_int64(), 0);
    EXPECT_TRUE(obj.contains("grid"));
}

TEST(GomokuGameTest, GetStateAfterWin)
{
    GomokuGame game(15);
    int b[] = {7*15+3, 7*15+4, 7*15+5, 7*15+6, 7*15+7};
    int w[] = {0*15+0, 1*15+0, 2*15+0, 3*15+0};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    game.tick(b[4]);
    EXPECT_TRUE(game.isOver());

    std::string state = game.getState();
    auto val = boost::json::parse(state);
    EXPECT_TRUE(val.as_object()["over"].as_bool());
    EXPECT_EQ(val.as_object()["winner"].as_int64(), 1);
    EXPECT_EQ(val.as_object()["score"].as_int64(), 1);
    EXPECT_TRUE(game.hasWinner());
    EXPECT_EQ(game.winner(), 1);
}

// ====== C API ======

extern "C" {
    void* plugin_create(const char* config_json);
    void  plugin_destroy(void* p);
    char* plugin_process(void* p, const char* input_json);
    void  plugin_free_string(char* s);
    int   plugin_is_done(void* p);
}

TEST(GomokuGameTest, CApi)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);

    char* s = plugin_process(plugin, R"({"action":"new_game","width":15,"height":15})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"gomoku\""), std::string::npos);
    plugin_free_string(s);

    s = plugin_process(plugin, R"({"action":"tick","value":119})");
    ASSERT_NE(s, nullptr);
    state = std::string(s);
    EXPECT_NE(state.find("\"grid\""), std::string::npos);
    plugin_free_string(s);

    plugin_destroy(plugin);
}

TEST(GomokuGameTest, CApiMultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin_is_done(plugin), 0);
        plugin_destroy(plugin);
    }
}

TEST(GomokuGameTest, CApiProcessInvalidJson)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    char* s = plugin_process(plugin, "not json");
    ASSERT_NE(s, nullptr);
    plugin_free_string(s);
    plugin_destroy(plugin);
}

TEST(GomokuGameTest, CApiMultipleTicks)
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
