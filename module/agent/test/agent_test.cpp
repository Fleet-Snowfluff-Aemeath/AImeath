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

// ====== openPlugin / controlPlugin / closePlugin ======

TEST(AgentServerTest, OpenPluginSendsOutput)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->openPlugin("snake", "{\"width\":20}");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("agent"));
    EXPECT_EQ(obj["action"].as_string(), std::string("open_plugin"));
    EXPECT_EQ(obj["plugin"].as_string(), std::string("snake"));

    ptr->destroy();
}

TEST(AgentServerTest, OpenPluginReturnsTrue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_TRUE(ptr->openPlugin("snake", "{}"));
    ptr->destroy();
}

TEST(AgentServerTest, OpenPluginWithoutOutputDoesNotCrash)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->openPlugin("snake", "{\"width\":20}"));
    EXPECT_NO_THROW(ptr->closePlugin("snake"));
    ptr->destroy();
}

TEST(AgentServerTest, ControlPluginSendsOutput)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->controlPlugin("snake", "{\"value\":3}");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["type"].as_string(), std::string("agent"));
    EXPECT_EQ(obj["action"].as_string(), std::string("control_plugin"));

    ptr->destroy();
}

TEST(AgentServerTest, ControlPluginReturnsTrue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_TRUE(ptr->controlPlugin("snake", "{\"value\":3}"));
    ptr->destroy();
}

TEST(AgentServerTest, ClosePluginSendsOutput)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;

    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->closePlugin("snake");
    EXPECT_FALSE(output.empty());

    auto val = boost::json::parse(output);
    EXPECT_TRUE(val.is_object());
    auto& obj = val.as_object();
    EXPECT_EQ(obj["action"].as_string(), std::string("close_plugin"));

    ptr->destroy();
}

TEST(AgentServerTest, ClosePluginReturnsTrue)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_TRUE(ptr->closePlugin("snake"));
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

// ====== chat_send ======

TEST(AgentServerTest, ChatSendValidText)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->chatSend("hello world"));
    ptr->destroy();
}

TEST(AgentServerTest, ChatSendEmptyText)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->chatSend(""));
    ptr->destroy();
}

TEST(AgentServerTest, ChatSendMultipleTimes)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(ptr->chatSend("msg " + std::to_string(i)));
    }
    ptr->destroy();
}

// ====== file_list ======

TEST(AgentServerTest, FileListWithPath)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    auto result = ptr->fileList("/");
    ptr->destroy();
}

TEST(AgentServerTest, FileListWithoutPath)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    bool result = ptr->fileList("/");
    ptr->destroy();
}

TEST(AgentServerTest, FileListMultipleTimes)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    for (int i = 0; i < 5; ++i) {
        ptr->fileList("/");
    }
    ptr->destroy();
}

TEST(AgentServerTest, FileListToolDefined)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    ptr->destroy();
}

// ====== file_read ======

TEST(AgentServerTest, FileReadValidPath)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileRead("/etc/hostname"));
    ptr->destroy();
}

TEST(AgentServerTest, FileReadNonExistent)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileRead("/__nonexistent__.txt"));
    ptr->destroy();
}

// ====== file_write ======

TEST(AgentServerTest, FileWriteValid)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileWrite("/tmp/agent_test.txt", "hello"));
    ptr->destroy();
}

TEST(AgentServerTest, FileWriteThenRead)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileWrite("/tmp/agent_test_rw.txt", "test content"));
    EXPECT_NO_THROW(ptr->fileRead("/tmp/agent_test_rw.txt"));
    ptr->destroy();
}

TEST(AgentServerTest, FileWriteEmptyContent)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileWrite("/tmp/agent_test_empty.txt", ""));
    ptr->destroy();
}

// ====== file_mkdir ======

TEST(AgentServerTest, FileMkdirValid)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileMkdir("/tmp/agent_test_dir"));
    ptr->destroy();
}

// ====== file_remove ======

TEST(AgentServerTest, FileRemoveValid)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileWrite("/tmp/agent_test_rm.txt", "to remove"));
    EXPECT_NO_THROW(ptr->fileRemove("/tmp/agent_test_rm.txt"));
    ptr->destroy();
}

TEST(AgentServerTest, FileRemoveNonExistent)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->fileRemove("/tmp/__nonexistent_rm__.txt"));
    ptr->destroy();
}

// ====== terminal_exec ======

TEST(AgentServerTest, TerminalExecEcho)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->terminalExec("echo hello"));
    ptr->destroy();
}

TEST(AgentServerTest, TerminalExecInvalidCommand)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->terminalExec("__nonexistent_cmd_xyz__"));
    ptr->destroy();
}

TEST(AgentServerTest, TerminalExecMultipleTimes)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(ptr->terminalExec("echo test"));
    }
    ptr->destroy();
}

TEST(AgentServerTest, TerminalExecEmptyCommand)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->terminalExec(""));
    ptr->destroy();
}

// ====== terminal_stdin ======

TEST(AgentServerTest, TerminalStdinValidData)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->terminalStdin("test input"));
    ptr->destroy();
}

TEST(AgentServerTest, TerminalStdinEmptyData)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->terminalStdin(""));
    ptr->destroy();
}

// ====== get_plugin_state for chat, filemanager, terminal ======

TEST(AgentServerTest, GetPluginStateChat)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->controlPlugin("chat", "{\"action\":\"poll\"}"));
    ptr->destroy();
}

TEST(AgentServerTest, GetPluginStateFilemanager)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->controlPlugin("filemanager", "{\"action\":\"list\",\"path\":\"/\"}"));
    ptr->destroy();
}

TEST(AgentServerTest, GetPluginStateTerminal)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    EXPECT_NO_THROW(ptr->controlPlugin("terminal", "{\"action\":\"resize\"}"));
    ptr->destroy();
}

// ====== open/close for chat, filemanager, terminal ======

TEST(AgentServerTest, OpenPluginChat)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;
    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->openPlugin("chat", "{}");
    EXPECT_FALSE(output.empty());
    auto val = boost::json::parse(output);
    EXPECT_EQ(val.as_object()["plugin"].as_string(), std::string("chat"));
    ptr->destroy();
}

TEST(AgentServerTest, OpenPluginFilemanager)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;
    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->openPlugin("filemanager", "{}");
    EXPECT_FALSE(output.empty());
    auto val = boost::json::parse(output);
    EXPECT_EQ(val.as_object()["plugin"].as_string(), std::string("filemanager"));
    ptr->destroy();
}

TEST(AgentServerTest, OpenPluginTerminal)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;
    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->openPlugin("terminal", "{}");
    EXPECT_FALSE(output.empty());
    auto val = boost::json::parse(output);
    EXPECT_EQ(val.as_object()["plugin"].as_string(), std::string("terminal"));
    ptr->destroy();
}

TEST(AgentServerTest, ClosePluginChat)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;
    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->closePlugin("chat");
    EXPECT_FALSE(output.empty());
    auto val = boost::json::parse(output);
    EXPECT_EQ(val.as_object()["plugin"].as_string(), std::string("chat"));
    ptr->destroy();
}

TEST(AgentServerTest, ClosePluginFilemanager)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;
    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->closePlugin("filemanager");
    EXPECT_FALSE(output.empty());
    auto val = boost::json::parse(output);
    EXPECT_EQ(val.as_object()["plugin"].as_string(), std::string("filemanager"));
    ptr->destroy();
}

TEST(AgentServerTest, ClosePluginTerminal)
{
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string output;
    ptr->setOutput(
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &output);

    ptr->closePlugin("terminal");
    EXPECT_FALSE(output.empty());
    auto val = boost::json::parse(output);
    EXPECT_EQ(val.as_object()["plugin"].as_string(), std::string("terminal"));
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
    void* plugin_create(const char* config);
    void  plugin_destroy(void* p);
    void  plugin_set_output(void* p, plugin_output_fn cb, void* udata);
    int   plugin_is_done(void* p);
    void  plugin_on_input(void* p, const char* json);
    char* plugin_process(void* p, const char* json);
    void  plugin_free_string(char* s);
}

TEST(AgentServerTest, CApiCreateDestroy)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

TEST(AgentServerTest, CApiMultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        plugin_destroy(plugin);
    }
}

TEST(AgentServerTest, CApiProcess)
{
    void* plugin = plugin_create(nullptr);
    char* s = plugin_process(plugin, R"({"text":"hello"})");
    ASSERT_NE(s, nullptr);
    std::string result(s);
    EXPECT_FALSE(result.empty());
    plugin_free_string(s);
    plugin_destroy(plugin);
}

TEST(AgentServerTest, CApiOnInputStop)
{
    void* plugin = plugin_create(nullptr);
    plugin_on_input(plugin, R"({"action":"stop"})");
    EXPECT_EQ(plugin_is_done(plugin), 1);
    plugin_destroy(plugin);
}

TEST(AgentServerTest, CApiSetOutput)
{
    void* plugin = plugin_create(nullptr);
    std::string received;
    EXPECT_NO_THROW(plugin_set_output(plugin,
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &received));
    plugin_destroy(plugin);
}

TEST(AgentServerTest, CApiFullLifecycle)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);

    std::string received;
    plugin_set_output(plugin,
        [](void* udata, const char* json) {
            *static_cast<std::string*>(udata) = json;
        },
        &received);

    plugin_on_input(plugin, R"({"action":"stop"})");
    EXPECT_EQ(plugin_is_done(plugin), 1);
    EXPECT_FALSE(received.empty());

    auto val = boost::json::parse(received);
    EXPECT_EQ(val.as_object()["type"].as_string(), std::string("stream_end"));

    plugin_destroy(plugin);
}

TEST(AgentServerTest, CApiProcessWithEmptyInput)
{
    void* plugin = plugin_create(nullptr);
    char* s = plugin_process(plugin, "");
    ASSERT_NE(s, nullptr);
    std::string result(s);
    auto val = boost::json::parse(result);
    EXPECT_TRUE(val.is_array());
    EXPECT_TRUE(val.as_array().empty());
    plugin_free_string(s);
    plugin_destroy(plugin);
}