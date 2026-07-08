#include <gtest/gtest.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for Terminal (from terminal.cpp) ----
extern "C" {
void* plugin_create(const char* config_json);
void  plugin_destroy(void* p);
char* plugin_process(void* p, const char* input_json);
void  plugin_free_string(char* s);
int   plugin_is_done(void* p);
typedef void (*plugin_output_fn)(void* userdata, const char* json);
void  plugin_set_output(void* p, plugin_output_fn cb, void* userdata);
void  plugin_on_input(void* p, const char* input_json);
}

static std::string callProcess(void* plugin, const std::string& json)
{
    char* out = plugin_process(plugin, json.c_str());
    std::string result(out ? out : "[]");
    if (out) plugin_free_string(out);
    return result;
}

static bool resultHasType(const std::string& json, const std::string& type)
{
    return json.find("\"type\":\"" + type + "\"") != std::string::npos;
}

// ====== 基础生命周期 ======

TEST(TerminalTest, CreateAndDestroy)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    plugin_destroy(plugin);
}

TEST(TerminalTest, CreateWithNullConfig)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    plugin_destroy(plugin);
}

TEST(TerminalTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        plugin_destroy(plugin);
    }
}

// ====== ExecSync Action ======

TEST(TerminalTest, ExecSyncEcho)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"echo hello"})");
    EXPECT_TRUE(resultHasType(result, "output"));
    EXPECT_NE(result.find("hello"), std::string::npos);
    plugin_destroy(plugin);
}

TEST(TerminalTest, ExecSyncMissingCommand)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"exec_sync"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(TerminalTest, ExecSyncInvalidCommand)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"__nonexistent_cmd_xyz__"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(TerminalTest, ExecSyncEmptyCommand)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"true"})");
    EXPECT_TRUE(resultHasType(result, "output") || resultHasType(result, "error"));
    plugin_destroy(plugin);
}

// ====== 错误处理 ======

TEST(TerminalTest, InvalidJson)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, "not json");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(TerminalTest, MissingActionField)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"cmd":"ls"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(TerminalTest, UnknownAction)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"__unknown__"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(TerminalTest, ExecMissingCmd)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"exec"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(TerminalTest, EmptyStringInput)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, "");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

// ====== Resize ======

TEST(TerminalTest, ResizeWithoutSession)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"resize","rows":30,"cols":100})");
    EXPECT_EQ(result, "[]");
    plugin_destroy(plugin);
}

TEST(TerminalTest, ResizeWithDefaults)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"resize"})");
    EXPECT_EQ(result, "[]");
    plugin_destroy(plugin);
}

// ====== IsDone ======

TEST(TerminalTest, IsDoneWithoutSession)
{
    void* plugin = plugin_create(nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 1);
    plugin_destroy(plugin);
}

// ====== SetOutput ======

TEST(TerminalTest, SetOutputDoesNotCrash)
{
    void* plugin = plugin_create(nullptr);
    std::string captured;
    plugin_set_output(plugin, [](void* udata, const char* json) {
        *static_cast<std::string*>(udata) = json;
    }, &captured);
    plugin_destroy(plugin);
}

// ====== 压力测试 ======

TEST(TerminalTest, MultipleExecSyncCalls)
{
    void* plugin = plugin_create(nullptr);
    for (int i = 0; i < 20; ++i) {
        std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"echo test"})");
        EXPECT_TRUE(resultHasType(result, "output"));
    }
    plugin_destroy(plugin);
}

TEST(TerminalTest, MultipleCreatesAndDestroys)
{
    for (int i = 0; i < 10; ++i) {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        std::string result = callProcess(plugin, R"({"action":"exec_sync","command":"echo ok"})");
        EXPECT_TRUE(resultHasType(result, "output"));
        plugin_destroy(plugin);
    }
}
