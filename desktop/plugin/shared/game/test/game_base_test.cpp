#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include "game_base.hpp"
#include "game_api.hpp"

class TestGame : public Game
{
    int m_score = 0;
    bool m_over = false;
    int m_last = 0;
public:
    TestGame() = default;
    TestGame(int, int) {}
    void tick(int a) override
    {
        if (m_over) return;
        m_score += a;
        m_last = a;
    }
    bool isOver() const override { return m_over; }
    int score() const override { return m_score; }
    std::string getState() const override
    {
        return "{\"score\":" + std::to_string(m_score)
             + ",\"last\":" + std::to_string(m_last)
             + ",\"over\":" + (m_over ? "true" : "false") + "}";
    }
    void setOver(bool o) { m_over = o; }
    int last() const { return m_last; }
};

TEST(GameBaseTest, VirtualDispatch)
{
    TestGame g;
    Game* pg = &g;
    pg->tick(10);
    EXPECT_EQ(pg->score(), 10);
    EXPECT_EQ(pg->isOver(), false);
    const auto& state = pg->getState();
    EXPECT_NE(state.find("\"score\":10"), std::string::npos);
}

TEST(GameBaseTest, TickMultipleAccumulates)
{
    TestGame g;
    g.tick(3);
    EXPECT_EQ(g.score(), 3);
    g.tick(7);
    EXPECT_EQ(g.score(), 10);
    g.tick(5);
    EXPECT_EQ(g.score(), 15);
}

TEST(GameBaseTest, TickIgnoredWhenOver)
{
    TestGame g;
    g.setOver(true);
    g.tick(5);
    EXPECT_EQ(g.score(), 0);
}

TEST(GameBaseTest, IsOverInitiallyFalse)
{
    TestGame g;
    EXPECT_FALSE(g.isOver());
}

TEST(GameBaseTest, ScoreInitiallyZero)
{
    TestGame g;
    EXPECT_EQ(g.score(), 0);
}

// ---- C ABI integration (uses APP_GAME_API_COMMON) ----

#define GAME_CLASS TestGame
APP_GAME_API_COMMON()

extern "C" {
    void* plugin_create(const char*);
    void  plugin_destroy(void*);
    char* plugin_process(void*, const char*);
    void  plugin_free_string(char*);
    int   plugin_is_done(void*);
}

TEST(GameApiTest, CreateAndDestroy)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

TEST(GameApiTest, NewGameAndTick)
{
    void* plugin = plugin_create(nullptr);
    char* s = plugin_process(plugin, R"({"action":"new_game","width":10,"height":10})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"score\""), std::string::npos);
    plugin_free_string(s);

    s = plugin_process(plugin, R"({"action":"tick","value":7})");
    ASSERT_NE(s, nullptr);
    state = std::string(s);
    EXPECT_NE(state.find("\"score\":7"), std::string::npos);
    plugin_free_string(s);

    plugin_destroy(plugin);
}

TEST(GameApiTest, PauseResume)
{
    void* plugin = plugin_create(nullptr);
    plugin_process(plugin, R"({"action":"new_game","width":10,"height":10})");

    char* s = plugin_process(plugin, R"({"action":"pause"})");
    std::string state(s);
    EXPECT_NE(state.find("\"ok\""), std::string::npos);
    plugin_free_string(s);

    s = plugin_process(plugin, R"({"action":"tick","value":5})");
    EXPECT_STREQ(s, "[]");
    plugin_free_string(s);

    s = plugin_process(plugin, R"({"action":"resume"})");
    state = std::string(s);
    EXPECT_NE(state.find("\"ok\""), std::string::npos);
    plugin_free_string(s);

    s = plugin_process(plugin, R"({"action":"tick","value":5})");
    state = std::string(s);
    EXPECT_NE(state.find("\"score\":5"), std::string::npos);
    plugin_free_string(s);

    plugin_destroy(plugin);
}

TEST(GameApiTest, EndGameAndDone)
{
    void* plugin = plugin_create(nullptr);
    plugin_process(plugin, R"({"action":"new_game","width":10,"height":10})");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    char* s = plugin_process(plugin, R"({"action":"end_game"})");
    std::string state(s);
    EXPECT_NE(state.find("\"ok\""), std::string::npos);
    plugin_free_string(s);

    EXPECT_EQ(plugin_is_done(plugin), 1);
    plugin_destroy(plugin);
}

TEST(GameApiTest, UnknownAction)
{
    void* plugin = plugin_create(nullptr);
    char* s = plugin_process(plugin, R"({"action":"__unknown__"})");
    std::string state(s);
    EXPECT_NE(state.find("\"error\""), std::string::npos);
    plugin_free_string(s);
    plugin_destroy(plugin);
}

TEST(GameApiTest, GetState)
{
    void* plugin = plugin_create(nullptr);
    plugin_process(plugin, R"({"action":"new_game","width":10,"height":10})");

    char* s = plugin_process(plugin, R"({"action":"get_state"})");
    std::string state(s);
    EXPECT_NE(state.find("\"score\""), std::string::npos);
    plugin_free_string(s);

    plugin_destroy(plugin);
}

TEST(GameApiTest, TickBeforeNewGameReturnsEmpty)
{
    void* plugin = plugin_create(nullptr);
    char* s = plugin_process(plugin, R"({"action":"tick","value":1})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"error\""), std::string::npos);
    plugin_free_string(s);
    plugin_destroy(plugin);
}

TEST(GameApiTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin_is_done(plugin), 0);
        plugin_destroy(plugin);
    }
}

TEST(GameApiTest, ProcessInvalidJson)
{
    void* plugin = plugin_create(nullptr);
    char* s = plugin_process(plugin, "not json");
    ASSERT_NE(s, nullptr);
    plugin_free_string(s);
    plugin_destroy(plugin);
}
