#include <gtest/gtest.h>
#include "chat.hpp"
#include <vector>
#include <string>
#include <mutex>

// ---- C ABI for ChatApp (from chat_server.cpp) ----
extern "C" {
void* app_create(const char* config_json);
void  app_destroy(void* p);
int   app_is_done(void* p);
typedef void (*app_output_fn)(void* userdata, const char* json);
void  app_set_output(void* p, app_output_fn cb, void* userdata);
void  app_on_input(void* p, const char* input_json);
void  app_set_io_context(void* p, void* io_context);
}

static std::string meow(const std::string& text)
{
    return text + "\xe5\x96\xb5";
}

static char* meow_c(const char* text)
{
    std::string s(text);
    s += "\xe5\x96\xb5";
    char* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (buf)
        std::memcpy(buf, s.data(), s.size() + 1);
    return buf;
}

TEST(ChatTest, ProcessAppendsMeow)
{
    Chat chat(meow);
    EXPECT_EQ(chat.process(""), "\xe5\x96\xb5");
    EXPECT_EQ(chat.process("hello"), "hello\xe5\x96\xb5");
    EXPECT_EQ(chat.process("你好"), "你好\xe5\x96\xb5");
}

TEST(ChatTest, ProcessMultipleCalls)
{
    Chat chat(meow);
    EXPECT_EQ(chat.process("a"), "a\xe5\x96\xb5");
    EXPECT_EQ(chat.process("b"), "b\xe5\x96\xb5");
    EXPECT_EQ(chat.process("c"), "c\xe5\x96\xb5");
}

TEST(ChatTest, ProcessWithChineseInput)
{
    Chat chat(meow);
    EXPECT_EQ(chat.process("喵"), "喵\xe5\x96\xb5");
    EXPECT_EQ(chat.process("今天天气不错"), "今天天气不错\xe5\x96\xb5");
}

TEST(ChatTest, CapiNewFree)
{
    void* chat = chat_new(meow_c);
    ASSERT_NE(chat, nullptr);
    chat_free(chat);
}

TEST(ChatTest, CapiProcess)
{
    void* chat = chat_new(meow_c);
    ASSERT_NE(chat, nullptr);

    char* r1 = chat_process(chat, "");
    EXPECT_STREQ(r1, "\xe5\x96\xb5");
    std::free(r1);

    char* r2 = chat_process(chat, "hello");
    EXPECT_STREQ(r2, "hello\xe5\x96\xb5");
    std::free(r2);

    char* r3 = chat_process(chat, "你好");
    EXPECT_STREQ(r3, "你好\xe5\x96\xb5");
    std::free(r3);

    chat_free(chat);
}

TEST(ChatTest, CapiMultipleInstances)
{
    void* c1 = chat_new(meow_c);
    void* c2 = chat_new(meow_c);
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    char* r1 = chat_process(c1, "foo");
    char* r2 = chat_process(c2, "bar");
    EXPECT_STREQ(r1, "foo\xe5\x96\xb5");
    EXPECT_STREQ(r2, "bar\xe5\x96\xb5");
    std::free(r1);
    std::free(r2);

    chat_free(c1);
    chat_free(c2);
}

// ============================================================
//  ChatServer Multi-Round Tests (regression: done flag bug)
// ============================================================

struct CaptureOutput
{
    std::mutex mtx;
    std::vector<std::string> events;

    void push(const char* json)
    {
        std::lock_guard<std::mutex> lock(mtx);
        events.push_back(std::string(json));
    }
};

static void capture_callback(void* userdata, const char* json)
{
    static_cast<CaptureOutput*>(userdata)->push(json);
}

TEST(ChatServerTest, InitialStateNotDone)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

TEST(ChatServerTest, CommandDoesNotSetDone)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    app_on_input(app, R"({"text":"/图片"})");
    EXPECT_EQ(app_is_done(app), 0);

    app_on_input(app, R"({"text":"/视频"})");
    EXPECT_EQ(app_is_done(app), 0);

    app_on_input(app, R"({"text":"/音乐"})");
    EXPECT_EQ(app_is_done(app), 0);

    app_destroy(app);
}

// Regression: after multiple text messages, app_is_done must remain 0.
// Uses synchronous commands to avoid depending on DeepSeek API key presence.
TEST(ChatServerTest, MultiRoundTextWithoutApiKeyDoesNotSetDone)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    // Use commands (starting with /) — they are synchronous and don't need API key.
    // Text messages without API key go through push_output synchronously too.
    const int ROUNDS = 15;
    for (int i = 0; i < ROUNDS; ++i)
    {
        app_on_input(app, R"({"text":"/图片"})");
        EXPECT_EQ(app_is_done(app), 0);
    }

    // Verify output was produced for each round
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        EXPECT_GE(capture.events.size(), (size_t)ROUNDS);
    }

    app_destroy(app);
}

// Regression: multi-round with commands interspersed still doesn't set done
TEST(ChatServerTest, MultiRoundMixedCommandsAndText)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    for (int i = 0; i < 10; ++i)
    {
        app_on_input(app, R"({"text":"/图片"})");
        EXPECT_EQ(app_is_done(app), 0);

        std::string msg = R"({"text":"msg )" + std::to_string(i) + R"("})";
        app_on_input(app, msg.c_str());
        EXPECT_EQ(app_is_done(app), 0);
    }

    app_destroy(app);
}

TEST(ChatServerTest, DestroyAfterMultipleRounds)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    for (int i = 0; i < 20; ++i)
    {
        std::string msg = R"({"text":"msg )" + std::to_string(i) + R"("})";
        app_on_input(app, msg.c_str());
    }

    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

// History growth: verify that sending many messages doesn't crash or leak
TEST(ChatServerTest, StressMultiRound)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    const int ROUNDS = 50;
    for (int i = 0; i < ROUNDS; ++i)
    {
        std::string msg = R"({"text":"stress test round )" + std::to_string(i) + R"("})";
        app_on_input(app, msg.c_str());
        ASSERT_EQ(app_is_done(app), 0) << "done became true at round " << i;
    }

    app_destroy(app);
}
