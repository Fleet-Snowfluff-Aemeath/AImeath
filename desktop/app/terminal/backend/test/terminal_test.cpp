#include <gtest/gtest.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for Terminal (from terminal.cpp) ----
extern "C" {
void* app_create(const char* config_json);
void  app_destroy(void* p);
char* app_process(void* p, const char* input_json);
void  app_free_string(char* s);
int   app_is_done(void* p);
typedef void (*app_output_fn)(void* userdata, const char* json);
void  app_set_output(void* p, app_output_fn cb, void* userdata);
void  app_on_input(void* p, const char* input_json);
}

static std::string callProcess(void* app, const std::string& json)
{
    char* out = app_process(app, json.c_str());
    std::string result(out ? out : "[]");
    if (out) app_free_string(out);
    return result;
}

static bool resultHasType(const std::string& json, const std::string& type)
{
    return json.find("\"type\":\"" + type + "\"") != std::string::npos;
}

// ====== 基础生命周期 ======

TEST(TerminalTest, CreateAndDestroy)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    app_destroy(app);
}

TEST(TerminalTest, CreateWithNullConfig)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    app_destroy(app);
}

TEST(TerminalTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* app = app_create(nullptr);
        ASSERT_NE(app, nullptr);
        app_destroy(app);
    }
}

// ====== ExecSync Action ======

TEST(TerminalTest, ExecSyncEcho)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"exec_sync","command":"echo hello"})");
    EXPECT_TRUE(resultHasType(result, "output"));
    EXPECT_NE(result.find("hello"), std::string::npos);
    app_destroy(app);
}

TEST(TerminalTest, ExecSyncMissingCommand)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"exec_sync"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(TerminalTest, ExecSyncInvalidCommand)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"exec_sync","command":"__nonexistent_cmd_xyz__"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(TerminalTest, ExecSyncEmptyCommand)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"exec_sync","command":"true"})");
    EXPECT_TRUE(resultHasType(result, "output") || resultHasType(result, "error"));
    app_destroy(app);
}

// ====== 错误处理 ======

TEST(TerminalTest, InvalidJson)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, "not json");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(TerminalTest, MissingActionField)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"cmd":"ls"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(TerminalTest, UnknownAction)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"__unknown__"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(TerminalTest, ExecMissingCmd)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"exec"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(TerminalTest, EmptyStringInput)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, "");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

// ====== Resize ======

TEST(TerminalTest, ResizeWithoutSession)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"resize","rows":30,"cols":100})");
    EXPECT_EQ(result, "[]");
    app_destroy(app);
}

TEST(TerminalTest, ResizeWithDefaults)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"resize"})");
    EXPECT_EQ(result, "[]");
    app_destroy(app);
}

// ====== IsDone ======

TEST(TerminalTest, IsDoneWithoutSession)
{
    void* app = app_create(nullptr);
    EXPECT_EQ(app_is_done(app), 1);
    app_destroy(app);
}

// ====== SetOutput ======

TEST(TerminalTest, SetOutputDoesNotCrash)
{
    void* app = app_create(nullptr);
    std::string captured;
    app_set_output(app, [](void* udata, const char* json) {
        *static_cast<std::string*>(udata) = json;
    }, &captured);
    app_destroy(app);
}

// ====== 压力测试 ======

TEST(TerminalTest, MultipleExecSyncCalls)
{
    void* app = app_create(nullptr);
    for (int i = 0; i < 20; ++i) {
        std::string result = callProcess(app, R"({"action":"exec_sync","command":"echo test"})");
        EXPECT_TRUE(resultHasType(result, "output"));
    }
    app_destroy(app);
}

TEST(TerminalTest, MultipleCreatesAndDestroys)
{
    for (int i = 0; i < 10; ++i) {
        void* app = app_create(nullptr);
        ASSERT_NE(app, nullptr);
        std::string result = callProcess(app, R"({"action":"exec_sync","command":"echo ok"})");
        EXPECT_TRUE(resultHasType(result, "output"));
        app_destroy(app);
    }
}
