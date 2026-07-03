#include <gtest/gtest.h>
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
void  app_set_io_context(void* p, void* io_context);
void  app_on_input(void* p, const char* input_json);
int   app_queue_size(void* p);
int   app_streaming(void* p);
void  app_test_set_streaming(void* p, int val);
void  app_test_drain_queue(void* p);
}

struct CaptureOutput
{
    std::mutex mtx;
    std::vector<std::string> events;

    void push(const char* json)
    {
        std::lock_guard<std::mutex> lock(mtx);
        events.push_back(std::string(json));
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx);
        events.clear();
    }
};

static void capture_callback(void* userdata, const char* json)
{
    static_cast<CaptureOutput*>(userdata)->push(json);
}

// ====== 基础生命周期 ======

TEST(ChatServerTest, InitialStateNotDone)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

TEST(ChatServerTest, CreateWithNullConfig)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

TEST(ChatServerTest, CreateWithEmptyConfig)
{
    void* app = app_create("{}");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

// ====== 命令处理 ======

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

TEST(ChatServerTest, CommandProducesEmbedOutput)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    app_on_input(app, R"({"text":"/图片"})");
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        ASSERT_EQ(capture.events.size(), 1u);
        EXPECT_NE(capture.events[0].find("embed"), std::string::npos);
    }

    app_destroy(app);
}

TEST(ChatServerTest, GameCommandProducesEmbed)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    app_on_input(app, R"({"text":"/游戏 snake"})");
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        ASSERT_EQ(capture.events.size(), 1u);
        EXPECT_NE(capture.events[0].find("game"), std::string::npos);
    }

    app_destroy(app);
}

TEST(ChatServerTest, UnknownCommandProducesTextEmbed)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    app_on_input(app, R"({"text":"/nosuchcmd"})");
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        ASSERT_EQ(capture.events.size(), 1u);
        EXPECT_NE(capture.events[0].find("未知命令"), std::string::npos);
    }

    app_destroy(app);
}

// ====== 多轮对话压力 ======

TEST(ChatServerTest, MultiRoundTextWithoutApiKeyDoesNotSetDone)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    const int ROUNDS = 15;
    for (int i = 0; i < ROUNDS; ++i)
    {
        app_on_input(app, R"({"text":"/图片"})");
        EXPECT_EQ(app_is_done(app), 0);
    }

    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        EXPECT_GE(capture.events.size(), (size_t)ROUNDS);
    }

    app_destroy(app);
}

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

// ====== 消息队列 ======

TEST(ChatServerTest, TextWhileStreamingQueues)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    // First text starts streaming
    app_on_input(app, R"({"text":"first msg"})");
    EXPECT_EQ(app_streaming(app), 1);
    EXPECT_EQ(app_queue_size(app), 0);

    // Send more messages while streaming — should queue
    app_on_input(app, R"({"text":"second msg"})");
    app_on_input(app, R"({"text":"third msg"})");
    EXPECT_EQ(app_queue_size(app), 2);

    app_destroy(app);
}

// ====== Stop 动作 ======

TEST(ChatServerTest, StopActionClearsStreaming)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    // Start streaming with a text message
    app_on_input(app, R"({"text":"hello"})");
    EXPECT_EQ(app_streaming(app), 1);

    // Send stop
    app_on_input(app, R"({"action":"stop"})");
    EXPECT_EQ(app_streaming(app), 0);
    EXPECT_EQ(app_queue_size(app), 0);

    app_destroy(app);
}

// ====== Poll 动作 ======

TEST(ChatServerTest, PollActionDoesNotCrash)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    // Poll before any messages — should not crash
    app_on_input(app, R"({"action":"poll"})");
    EXPECT_EQ(app_is_done(app), 0);

    app_destroy(app);
}

// ====== 流状态测试 ======

TEST(ChatServerTest, SetStreamingAndDrainQueue)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    // Initially not streaming
    EXPECT_EQ(app_streaming(app), 0);
    EXPECT_EQ(app_queue_size(app), 0);

    // Force into streaming state
    app_test_set_streaming(app, 1);
    EXPECT_EQ(app_streaming(app), 1);

    // Queue a message
    std::string queued = R"({"text":"queued msg"})";
    app_on_input(app, queued.c_str());
    EXPECT_GE(app_queue_size(app), 1);

    // Drain queue — this will process the queued message and may start a new stream
    app_test_drain_queue(app);

    // Queue should be empty after drain
    EXPECT_EQ(app_queue_size(app), 0);

    app_destroy(app);
}

// ====== 工具调用无缓存 ======

TEST(ChatServerTest, ToolCallWithoutCacheGraceful)
{
    void* app = app_create("{}");
    ASSERT_NE(app, nullptr);
    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);
    app_on_input(app, R"({"text":"/图片"})");
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

// ====== 多实例独立 ======

TEST(ChatServerTest, MultipleInstancesIndependent)
{
    void* app1 = app_create(nullptr);
    void* app2 = app_create(nullptr);
    ASSERT_NE(app1, nullptr);
    ASSERT_NE(app2, nullptr);
    EXPECT_NE(app1, app2);
    EXPECT_EQ(app_is_done(app1), 0);
    EXPECT_EQ(app_is_done(app2), 0);

    CaptureOutput cap1, cap2;
    app_set_output(app1, capture_callback, &cap1);
    app_set_output(app2, capture_callback, &cap2);

    app_on_input(app1, R"({"text":"/图片"})");
    app_on_input(app2, R"({"text":"/音乐"})");

    {
        std::lock_guard<std::mutex> lock1(cap1.mtx);
        EXPECT_EQ(cap1.events.size(), 1u);
    }
    {
        std::lock_guard<std::mutex> lock2(cap2.mtx);
        EXPECT_EQ(cap2.events.size(), 1u);
    }

    app_destroy(app1);
    app_destroy(app2);
}

TEST(ChatServerTest, DestroyOneDoesNotAffectOther)
{
    void* app1 = app_create(nullptr);
    void* app2 = app_create(nullptr);
    ASSERT_NE(app1, nullptr);
    ASSERT_NE(app2, nullptr);

    app_destroy(app1);
    EXPECT_EQ(app_is_done(app2), 0);
    app_destroy(app2);
}

// ====== 连续 stop ======

TEST(ChatServerTest, MultipleStopDoesNotCrash)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);

    CaptureOutput capture;
    app_set_output(app, capture_callback, &capture);

    app_on_input(app, R"({"text":"hello"})");
    EXPECT_EQ(app_streaming(app), 1);

    // Send stop multiple times — should not crash
    for (int i = 0; i < 5; ++i)
        app_on_input(app, R"({"action":"stop"})");

    EXPECT_EQ(app_streaming(app), 0);
    EXPECT_EQ(app_is_done(app), 0);

    app_destroy(app);
}
