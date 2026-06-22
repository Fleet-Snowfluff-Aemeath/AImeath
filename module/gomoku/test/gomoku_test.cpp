#include <gtest/gtest.h>
#include "board.hpp"
#include "game.hpp"

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

// ====== Game ======

TEST(GomokuGameTest, Construction)
{
    GomokuGame game(15);
    EXPECT_FALSE(game.isOver());
    EXPECT_EQ(game.score(), 0);
    EXPECT_EQ(game.currentPlayer(), 1);
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
    // Black: (7,3),(7,4),(7,5),(7,6),(7,7) → horizontal 5
    // White: (0,0),(1,0),(2,0),(3,0)
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
    // Black: (3,7),(4,7),(5,7),(6,7),(7,7) → vertical 5
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
    // Black: (3,3),(4,4),(5,5),(6,6),(7,7) → diagonal
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
    // Black: (3,7),(4,6),(5,5),(6,4),(7,3) → anti-diagonal /
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
    // Black: scattered (no 5-in-a-row), White: horizontal 5 at row 5
    int b[] = {0*15+0, 2*15+2, 4*15+4, 6*15+6};
    int w[] = {5*15+3, 5*15+4, 5*15+5, 5*15+6, 5*15+7};
    for (int i = 0; i < 4; ++i)
    {
        game.tick(b[i]);
        game.tick(w[i]);
    }
    // Black's turn, white needs one more
    game.tick(1*15+1);
    EXPECT_FALSE(game.isOver());
    game.tick(w[4]);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 2);
}

TEST(GomokuGameTest, DrawFullBoard)
{
    // 3x3 board = 9 cells, no one gets 5 → draw
    GomokuGame game(3);
    // B: 0,2,5,7   W: 1,3,4,6,8  → all 9 filled, no 5-in-a-row oll on 3x3
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
    // tick after game over should be ignored
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
    // invalid move
    game.tick(7*15+7);
    EXPECT_EQ(game.currentPlayer(), 2);
}

TEST(GomokuGameTest, BlockingWin)
{
    GomokuGame game(15);
    // Black tries horizontal at row 7: (7,4),(7,5),(7,6),(7,7)
    // White blocks: (7,8)
    // Black tries vertical at col 10: (8,10),(9,10),(10,10),(11,10)
    // White blocks: (12,10) — but black still needs 5
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
