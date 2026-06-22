#include <gtest/gtest.h>
#include "game_base.hpp"
#include "direction.hpp"
#include "game_api.hpp"

// ---- Game base class ----

class TestGame : public Game
{
    int m_score = 0;
    bool m_over = false;
    std::string m_state;
public:
    void tick(int a) override
    {
        if (m_over) return;
        m_score += a;
        m_state = "tick:" + std::to_string(a);
    }
    bool isOver() const override { return m_over; }
    int score() const override { return m_score; }
    std::string getState() const override { return m_state; }
    void setOver(bool o) { m_over = o; }
};

TEST(GameBaseTest, VirtualDispatch)
{
    TestGame g;
    Game* pg = &g;
    pg->tick(10);
    EXPECT_EQ(pg->score(), 10);
    EXPECT_EQ(pg->isOver(), false);
    EXPECT_EQ(pg->getState(), "tick:10");
}

TEST(GameBaseTest, TickIgnoredWhenOver)
{
    TestGame g;
    g.setOver(true);
    g.tick(5);
    EXPECT_EQ(g.score(), 0);
}

// ---- Direction helpers ----

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

TEST(DirectionTest, ApplyDir)
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

// ---- GAME_API macros ----

extern "C" {
    void* game_new(int w, int h) { return new TestGame; }
    GAME_API_COMMON()
}

TEST(GameApiTest, ApiDispatch)
{
    void* g = game_new(0, 0);
    ASSERT_NE(g, nullptr);
    game_tick(g, 7);
    EXPECT_EQ(game_get_score(g), 7);
    EXPECT_EQ(game_is_over(g), 0);
    char* s = game_get_state(g);
    EXPECT_STREQ(s, "tick:7");
    std::free(s);
    game_free(g);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}