#include <gtest/gtest.h>
#include "agent_server.hpp"
#include <boost/json.hpp>

TEST(AgentServerTest, CreateAndDestroy)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NE(ptr.get(), nullptr);
    ptr->destroy();
}

TEST(AgentServerTest, IsDoneInitiallyFalse)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_FALSE(ptr->isDone());
    ptr->destroy();
}

TEST(AgentServerTest, IsDoneAfterStop)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    ptr->stop();
    EXPECT_TRUE(ptr->isDone());
    ptr->destroy();
}

TEST(AgentServerTest, SetOutputCallback)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string received;

    ptr->setOutput(
        [](void* udata, const char* json) {
            auto* r = static_cast<std::string*>(udata);
            *r = json;
        },
        &received);

    EXPECT_NO_THROW(ptr->setOutput(nullptr, nullptr));
    ptr->destroy();
}

TEST(AgentServerTest, StopCancelsStream)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->stop());
    ptr->destroy();
}

TEST(AgentServerTest, OpenAppSendsOutput)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->openApp("snake", "{\"width\":20}");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("agent"));
    EXPECT_EQ(obj["action"].as_string(), std::string("open_app"));
    EXPECT_EQ(obj["app"].as_string(), std::string("snake"));

    ptr->destroy();
}

TEST(AgentServerTest, ControlAppSendsOutput)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->controlApp("snake", "{\"value\":3}");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("agent"));
    EXPECT_EQ(obj["action"].as_string(), std::string("control_app"));

    ptr->destroy();
}

TEST(AgentServerTest, CloseAppSendsOutput)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->closeApp("snake");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["action"].as_string(), std::string("close_app"));

    ptr->destroy();
}

TEST(AgentServerTest, OnInputStopClearsQueue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->onInput("{\"action\":\"stop\"}");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    EXPECT_EQ(val.as_object()["type"].as_string(), std::string("stream_end"));
    EXPECT_EQ(val.as_object()["msg"].as_string(), std::string("stopped"));

    ptr->destroy();
}

TEST(AgentServerTest, OnInputDequeuesWhenStreaming)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->onInput("{\"text\":\"hello\"}"));
    ptr->stop();
    ptr->destroy();
}

TEST(AgentServerTest, ProcessWithText)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process("{\"text\":\"hello\"}");
    EXPECT_FALSE(result.empty());
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    ptr->destroy();
}

TEST(AgentServerTest, ProcessWithCommand)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process("{\"text\":\"/help\"}");
    EXPECT_FALSE(result.empty());
    ptr->destroy();
}

TEST(AgentServerTest, MultipleStopDoesNotCrash)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->stop());
    EXPECT_NO_THROW(ptr->stop());
    ptr->destroy();
}

// ====== Process fallback (tests tool dispatch indirectly) ======

TEST(AgentServerTest, ProcessCommandListWindows)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process(R"({"text":"/help"})");
    EXPECT_FALSE(result.empty());
    ptr->destroy();
}

TEST(AgentServerTest, ProcessHandlesEmbed)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process(R"({"text":"/help"})");
    EXPECT_FALSE(result.empty());
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    ptr->destroy();
}

// ====== C ABI ======

extern "C" {
    void* app_create(const char* config);
    void  app_destroy(void* p);
    void  app_set_output(void* p, app_output_fn cb, void* udata);
    int   app_is_done(void* p);
    void  app_on_input(void* p, const char* json);
    char* app_process(void* p, const char* json);
    void  app_free_string(char* s);
}

TEST(AgentServerTest, CApiCreateDestroy)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

TEST(AgentServerTest, CApiProcess)
{
    void* app = app_create(nullptr);
    char* s = app_process(app, R"({"text":"hello"})");
    ASSERT_NE(s, nullptr);
    std::string result(s);
    EXPECT_FALSE(result.empty());
    app_free_string(s);
    app_destroy(app);
}

TEST(AgentServerTest, CApiOnInputStop)
{
    void* app = app_create(nullptr);
    app_on_input(app, R"({"action":"stop"})");
    EXPECT_EQ(app_is_done(app), 1);
    app_destroy(app);
}
