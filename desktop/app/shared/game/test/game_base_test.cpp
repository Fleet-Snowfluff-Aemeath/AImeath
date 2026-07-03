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

TEST(GameBaseTest, TickIgnoredWhenOver)
{
    TestGame g;
    g.setOver(true);
    g.tick(5);
    EXPECT_EQ(g.score(), 0);
}

// ---- C ABI integration (uses APP_GAME_API_COMMON) ----

#define GAME_CLASS TestGame
APP_GAME_API_COMMON()

extern "C" {
    void* app_create(const char*);
    void  app_destroy(void*);
    char* app_process(void*, const char*);
    void  app_free_string(char*);
    int   app_is_done(void*);
}

TEST(GameApiTest, CreateAndDestroy)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

TEST(GameApiTest, NewGameAndTick)
{
    void* app = app_create(nullptr);
    char* s = app_process(app, R"({"action":"new_game","width":10,"height":10})");
    ASSERT_NE(s, nullptr);
    std::string state(s);
    EXPECT_NE(state.find("\"score\""), std::string::npos);
    app_free_string(s);

    s = app_process(app, R"({"action":"tick","value":7})");
    ASSERT_NE(s, nullptr);
    state = std::string(s);
    EXPECT_NE(state.find("\"score\":7"), std::string::npos);
    app_free_string(s);

    app_destroy(app);
}

TEST(GameApiTest, PauseResume)
{
    void* app = app_create(nullptr);
    app_process(app, R"({"action":"new_game","width":10,"height":10})");

    char* s = app_process(app, R"({"action":"pause"})");
    std::string state(s);
    EXPECT_NE(state.find("\"ok\""), std::string::npos);
    app_free_string(s);

    s = app_process(app, R"({"action":"tick","value":5})");
    EXPECT_STREQ(s, "[]");
    app_free_string(s);

    s = app_process(app, R"({"action":"resume"})");
    state = std::string(s);
    EXPECT_NE(state.find("\"ok\""), std::string::npos);
    app_free_string(s);

    s = app_process(app, R"({"action":"tick","value":5})");
    state = std::string(s);
    EXPECT_NE(state.find("\"score\":5"), std::string::npos);
    app_free_string(s);

    app_destroy(app);
}

TEST(GameApiTest, EndGameAndDone)
{
    void* app = app_create(nullptr);
    app_process(app, R"({"action":"new_game","width":10,"height":10})");
    EXPECT_EQ(app_is_done(app), 0);

    char* s = app_process(app, R"({"action":"end_game"})");
    std::string state(s);
    EXPECT_NE(state.find("\"ok\""), std::string::npos);
    app_free_string(s);

    EXPECT_EQ(app_is_done(app), 1);
    app_destroy(app);
}

TEST(GameApiTest, UnknownAction)
{
    void* app = app_create(nullptr);
    char* s = app_process(app, R"({"action":"__unknown__"})");
    std::string state(s);
    EXPECT_NE(state.find("\"error\""), std::string::npos);
    app_free_string(s);
    app_destroy(app);
}

TEST(GameApiTest, GetState)
{
    void* app = app_create(nullptr);
    app_process(app, R"({"action":"new_game","width":10,"height":10})");

    char* s = app_process(app, R"({"action":"get_state"})");
    std::string state(s);
    EXPECT_NE(state.find("\"score\""), std::string::npos);
    app_free_string(s);

    app_destroy(app);
}
