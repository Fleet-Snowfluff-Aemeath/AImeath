#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <mutex>
#include <boost/json.hpp>

#include "chat_server.hpp"

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
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

TEST(ChatServerTest, CreateWithNullConfig)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

TEST(ChatServerTest, CreateWithEmptyConfig)
{
    void* plugin = plugin_create("{}");
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

// ====== 命令处理 ======

TEST(ChatServerTest, CommandDoesNotSetDone)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, R"({"text":"/图片"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_on_input(plugin, R"({"text":"/视频"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_on_input(plugin, R"({"text":"/音乐"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_destroy(plugin);
}

TEST(ChatServerTest, CommandProducesEmbedOutput)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, R"({"text":"/图片"})");
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        ASSERT_EQ(capture.events.size(), 1u);
        EXPECT_NE(capture.events[0].find("embed"), std::string::npos);
    }

    plugin_destroy(plugin);
}

TEST(ChatServerTest, GameCommandProducesEmbed)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, R"({"text":"/游戏 snake"})");
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        ASSERT_EQ(capture.events.size(), 1u);
        EXPECT_NE(capture.events[0].find("game"), std::string::npos);
    }

    plugin_destroy(plugin);
}

TEST(ChatServerTest, UnknownCommandProducesTextEmbed)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, R"({"text":"/nosuchcmd"})");
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        ASSERT_EQ(capture.events.size(), 1u);
        EXPECT_NE(capture.events[0].find("未知命令"), std::string::npos);
    }

    plugin_destroy(plugin);
}

// ====== 多轮对话压力 ======

TEST(ChatServerTest, MultiRoundTextWithoutApiKeyDoesNotSetDone)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    const int ROUNDS = 15;
    for (int i = 0; i < ROUNDS; ++i)
    {
        plugin_on_input(plugin, R"({"text":"/图片"})");
        EXPECT_EQ(plugin_is_done(plugin), 0);
    }

    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        EXPECT_GE(capture.events.size(), (size_t)ROUNDS);
    }

    plugin_destroy(plugin);
}

TEST(ChatServerTest, MultiRoundMixedCommandsAndText)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    for (int i = 0; i < 10; ++i)
    {
        plugin_on_input(plugin, R"({"text":"/图片"})");
        EXPECT_EQ(plugin_is_done(plugin), 0);

        std::string msg = R"({"text":"msg )" + std::to_string(i) + R"("})";
        plugin_on_input(plugin, msg.c_str());
        EXPECT_EQ(plugin_is_done(plugin), 0);
    }

    plugin_destroy(plugin);
}

TEST(ChatServerTest, DestroyAfterMultipleRounds)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    for (int i = 0; i < 20; ++i)
    {
        std::string msg = R"({"text":"msg )" + std::to_string(i) + R"("})";
        plugin_on_input(plugin, msg.c_str());
    }

    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

TEST(ChatServerTest, StressMultiRound)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    const int ROUNDS = 50;
    for (int i = 0; i < ROUNDS; ++i)
    {
        std::string msg = R"({"text":"stress test round )" + std::to_string(i) + R"("})";
        plugin_on_input(plugin, msg.c_str());
        ASSERT_EQ(plugin_is_done(plugin), 0) << "done became true at round " << i;
    }

    plugin_destroy(plugin);
}

// ====== Stop 动作 ======

TEST(ChatServerTest, StopActionEmitsStreamEnd)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, R"({"action":"stop"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    bool has_stream_end = false;
    {
        std::lock_guard<std::mutex> lock(capture.mtx);
        for (auto& e : capture.events) {
            auto v = boost::json::parse(e);
            if (v.is_object() && v.as_object().contains("type") &&
                v.as_object()["type"].as_string() == "stream_end")
                has_stream_end = true;
        }
    }
    EXPECT_TRUE(has_stream_end);

    plugin_destroy(plugin);
}

// ====== Poll 动作 ======

TEST(ChatServerTest, PollActionDoesNotCrash)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, R"({"action":"poll"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_destroy(plugin);
}

// ====== 工具调用无缓存 ======

TEST(ChatServerTest, ToolCallWithoutCacheGraceful)
{
    void* plugin = plugin_create("{}");
    ASSERT_NE(plugin, nullptr);
    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);
    plugin_on_input(plugin, R"({"text":"/图片"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

// ====== 多实例独立 ======

TEST(ChatServerTest, MultipleInstancesIndependent)
{
    void* plugin1 = plugin_create(nullptr);
    void* plugin2 = plugin_create(nullptr);
    ASSERT_NE(plugin1, nullptr);
    ASSERT_NE(plugin2, nullptr);
    EXPECT_NE(plugin1, plugin2);
    EXPECT_EQ(plugin_is_done(plugin1), 0);
    EXPECT_EQ(plugin_is_done(plugin2), 0);

    CaptureOutput cap1, cap2;
    plugin_set_output(plugin1, capture_callback, &cap1);
    plugin_set_output(plugin2, capture_callback, &cap2);

    plugin_on_input(plugin1, R"({"text":"/图片"})");
    plugin_on_input(plugin2, R"({"text":"/音乐"})");

    {
        std::lock_guard<std::mutex> lock1(cap1.mtx);
        EXPECT_EQ(cap1.events.size(), 1u);
    }
    {
        std::lock_guard<std::mutex> lock2(cap2.mtx);
        EXPECT_EQ(cap2.events.size(), 1u);
    }

    plugin_destroy(plugin1);
    plugin_destroy(plugin2);
}

TEST(ChatServerTest, DestroyOneDoesNotAffectOther)
{
    void* plugin1 = plugin_create(nullptr);
    void* plugin2 = plugin_create(nullptr);
    ASSERT_NE(plugin1, nullptr);
    ASSERT_NE(plugin2, nullptr);

    plugin_destroy(plugin1);
    EXPECT_EQ(plugin_is_done(plugin2), 0);
    plugin_destroy(plugin2);
}

TEST(ChatServerTest, MultipleInstancesStress)
{
    void* plugin1 = plugin_create(nullptr);
    void* plugin2 = plugin_create(nullptr);
    void* plugin3 = plugin_create(nullptr);
    ASSERT_NE(plugin1, nullptr);
    ASSERT_NE(plugin2, nullptr);
    ASSERT_NE(plugin3, nullptr);

    CaptureOutput cap1, cap2, cap3;
    plugin_set_output(plugin1, capture_callback, &cap1);
    plugin_set_output(plugin2, capture_callback, &cap2);
    plugin_set_output(plugin3, capture_callback, &cap3);

    plugin_on_input(plugin1, R"({"text":"/图片"})");
    plugin_on_input(plugin2, R"({"text":"/音乐"})");
    plugin_on_input(plugin3, R"({"text":"/视频"})");

    {
        std::lock_guard<std::mutex> lock(cap1.mtx);
        EXPECT_EQ(cap1.events.size(), 1u);
    }
    {
        std::lock_guard<std::mutex> lock(cap2.mtx);
        EXPECT_EQ(cap2.events.size(), 1u);
    }
    {
        std::lock_guard<std::mutex> lock(cap3.mtx);
        EXPECT_EQ(cap3.events.size(), 1u);
    }

    plugin_destroy(plugin1);
    plugin_destroy(plugin2);
    plugin_destroy(plugin3);
}

// ====== 连续 stop ======

TEST(ChatServerTest, MultipleStopDoesNotCrash)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    for (int i = 0; i < 5; ++i)
        plugin_on_input(plugin, R"({"action":"stop"})");

    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_destroy(plugin);
}

// ====== Empty / invalid input ======

TEST(ChatServerTest, EmptyInputDoesNotCrash)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, "{}");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_destroy(plugin);
}

TEST(ChatServerTest, InvalidJsonDoesNotCrash)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);

    CaptureOutput capture;
    plugin_set_output(plugin, capture_callback, &capture);

    plugin_on_input(plugin, "not json");
    EXPECT_EQ(plugin_is_done(plugin), 0);

    plugin_destroy(plugin);
}
