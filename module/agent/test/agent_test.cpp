#include <gtest/gtest.h>
#include "agent_server.hpp"
#include <boost/json.hpp>

// ====== lifecycle ======

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

TEST(AgentServerTest, IsDoneAfterDestroy)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    ptr->destroy();
    EXPECT_TRUE(ptr->isDone());
}

TEST(AgentServerTest, MultipleStopDoesNotCrash)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->stop());
    EXPECT_NO_THROW(ptr->stop());
    ptr->destroy();
}

TEST(AgentServerTest, MultipleDestroyDoesNotCrash)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->destroy());
    EXPECT_NO_THROW(ptr->destroy());
}

// ====== setOutput / setIoContext ======

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

TEST(AgentServerTest, SetIoContextDoesNotCrash)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->setIoContext(nullptr));
    int dummy = 0;
    EXPECT_NO_THROW(ptr->setIoContext(&dummy));
    ptr->destroy();
}

// ====== openApp / controlApp / closeApp ======

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

TEST(AgentServerTest, OpenAppReturnsTrue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_TRUE(ptr->openApp("snake", "{}"));
    ptr->destroy();
}

TEST(AgentServerTest, OpenAppWithoutOutputDoesNotCrash)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->openApp("snake", "{\"width\":20}"));
    EXPECT_NO_THROW(ptr->closeApp("snake"));
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

TEST(AgentServerTest, ControlAppReturnsTrue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_TRUE(ptr->controlApp("snake", "{\"value\":3}"));
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

TEST(AgentServerTest, CloseAppReturnsTrue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_TRUE(ptr->closeApp("snake"));
    ptr->destroy();
}

// ====== onInput ======

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

TEST(AgentServerTest, OnInputWithInvalidJson)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->onInput("not json"));
    EXPECT_NO_THROW(ptr->onInput(""));
    ptr->destroy();
}

TEST(AgentServerTest, OnInputWithEmptyObject)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->onInput("{}"));
    ptr->destroy();
}

// ====== process ======

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

TEST(AgentServerTest, ProcessWithEmptyString)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process("");
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    EXPECT_TRUE(val.as_array().empty());
    ptr->destroy();
}

TEST(AgentServerTest, ProcessWithInvalidJson)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process("not json");
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    EXPECT_TRUE(val.as_array().empty());
    ptr->destroy();
}

TEST(AgentServerTest, ProcessWithEmptyObject)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string result = ptr->process("{}");
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    ptr->destroy();
}

// ====== multi-instance ======

TEST(AgentServerTest, MultipleAgentsIndependent)
{
    auto agent1 = std::make_shared<agent::AgentServer>();
    auto agent2 = std::make_shared<agent::AgentServer>();

    EXPECT_FALSE(agent1->isDone());
    EXPECT_FALSE(agent2->isDone());

    agent1->stop();
    EXPECT_TRUE(agent1->isDone());
    EXPECT_FALSE(agent2->isDone());

    agent2->stop();
    EXPECT_TRUE(agent2->isDone());

    agent1->destroy();
    agent2->destroy();
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

TEST(AgentServerTest, CApiMultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* app = app_create(nullptr);
        ASSERT_NE(app, nullptr);
        app_destroy(app);
    }
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

TEST(AgentServerTest, CApiSetOutput)
{
    void* app = app_create(nullptr);
    std::string received;
    EXPECT_NO_THROW(app_set_output(app,
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &received));
    app_destroy(app);
}

TEST(AgentServerTest, CApiFullLifecycle)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);

    std::string received;
    app_set_output(app,
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &received);

    app_on_input(app, R"({"action":"stop"})");
    EXPECT_EQ(app_is_done(app), 1);
    EXPECT_FALSE(received.empty());

    auto val = boost::json::parse(received);
    EXPECT_EQ(val.as_object()["type"].as_string(), std::string("stream_end"));

    app_destroy(app);
}

TEST(AgentServerTest, CApiProcessWithEmptyInput)
{
    void* app = app_create(nullptr);
    char* s = app_process(app, "");
    ASSERT_NE(s, nullptr);
    std::string result(s);
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    EXPECT_TRUE(val.as_array().empty());
    app_free_string(s);
    app_destroy(app);
}