#include <gtest/gtest.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for FileManager (from filemgr.cpp) ----
extern "C" {
void* app_create(const char* config_json);
void  app_destroy(void* p);
char* app_process(void* p, const char* input_json);
void  app_free_string(char* s);
int   app_is_done(void* p);
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

TEST(FileManagerTest, CreateAndDestroy)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

TEST(FileManagerTest, CreateWithNullConfig)
{
    void* app = app_create(nullptr);
    ASSERT_NE(app, nullptr);
    app_destroy(app);
}

TEST(FileManagerTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* app = app_create(nullptr);
        ASSERT_NE(app, nullptr);
        EXPECT_EQ(app_is_done(app), 0);
        app_destroy(app);
    }
}

// ====== List Action ======

TEST(FileManagerTest, ListRootDirectory)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"list","path":"/"})");
    EXPECT_TRUE(resultHasType(result, "listing") || resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, ListHomeDirectory)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"list","path":"/home"})");
    EXPECT_TRUE(resultHasType(result, "listing") || resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, ListInvalidPath)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"list","path":"/__nonexistent_xyz__"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, ListMultipleTimes)
{
    void* app = app_create(nullptr);
    for (int i = 0; i < 5; ++i) {
        std::string result = callProcess(app, R"({"action":"list","path":"/"})");
        EXPECT_TRUE(resultHasType(result, "listing") || resultHasType(result, "error"));
    }
    app_destroy(app);
}

// ====== Read Action ======

TEST(FileManagerTest, ReadNonexistentFile)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"read","path":"/__nonexistent__.txt"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

// ====== Missing Action ======

TEST(FileManagerTest, MissingActionField)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"path":"/"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, UnknownAction)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"__unknown__","path":"/"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

// ====== Invalid JSON ======

TEST(FileManagerTest, InvalidJson)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, "not json");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, EmptyArrayInput)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, "[]");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, EmptyStringInput)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, "");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

// ====== Mkdir / Write / Remove ======

TEST(FileManagerTest, MkdirAction)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"mkdir","path":"/test_mkdir"})");
    EXPECT_TRUE(resultHasType(result, "ok") || resultHasType(result, "error"));
    app_destroy(app);
}

TEST(FileManagerTest, WriteAction)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"write","path":"/test.txt","content":"hello"})");
    EXPECT_TRUE(resultHasType(result, "ok"));
    app_destroy(app);
}

TEST(FileManagerTest, RemoveNonexistentFile)
{
    void* app = app_create(nullptr);
    std::string result = callProcess(app, R"({"action":"remove","path":"/__nonexistent__.txt"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    app_destroy(app);
}

// ====== IsDone Always False ======

TEST(FileManagerTest, IsDoneAlwaysFalse)
{
    void* app = app_create(nullptr);
    EXPECT_EQ(app_is_done(app), 0);
    callProcess(app, R"({"action":"list","path":"/"})");
    EXPECT_EQ(app_is_done(app), 0);
    callProcess(app, R"({"action":"read","path":"/nonexistent"})");
    EXPECT_EQ(app_is_done(app), 0);
    app_destroy(app);
}

// ====== 压力测试 ======

TEST(FileManagerTest, StressListManyTimes)
{
    void* app = app_create(nullptr);
    for (int i = 0; i < 100; ++i) {
        std::string result = callProcess(app, R"({"action":"list","path":"/"})");
        ASSERT_FALSE(result.empty());
    }
    app_destroy(app);
}

TEST(FileManagerTest, MultipleInstancesIndependent)
{
    void* app1 = app_create(nullptr);
    void* app2 = app_create(nullptr);
    ASSERT_NE(app1, nullptr);
    ASSERT_NE(app2, nullptr);
    EXPECT_NE(app1, app2);

    std::string r1 = callProcess(app1, R"({"action":"list","path":"/"})");
    std::string r2 = callProcess(app2, R"({"action":"list","path":"/"})");
    EXPECT_FALSE(r1.empty());
    EXPECT_FALSE(r2.empty());

    app_destroy(app1);
    app_destroy(app2);
}

TEST(FileManagerTest, WriteThenRead)
{
    void* app = app_create(nullptr);
    std::string w = callProcess(app, R"({"action":"write","path":"/test_rw.txt","content":"test content"})");
    EXPECT_TRUE(resultHasType(w, "ok"));
    std::string r = callProcess(app, R"({"action":"read","path":"/test_rw.txt"})");
    EXPECT_TRUE(resultHasType(r, "file"));
    app_destroy(app);
}

TEST(FileManagerTest, WriteThenRemove)
{
    void* app = app_create(nullptr);
    EXPECT_TRUE(resultHasType(callProcess(app, R"({"action":"write","path":"/test_rm.txt","content":"x"})"), "ok"));
    EXPECT_TRUE(resultHasType(callProcess(app, R"({"action":"remove","path":"/test_rm.txt"})"), "ok"));
    app_destroy(app);
}
