#include <gtest/gtest.h>
#include <string>
#include <cstdlib>
#include <cstring>

// ---- C ABI for FileManager (from filemgr.cpp) ----
extern "C" {
void* plugin_create(const char* config_json);
void  plugin_destroy(void* p);
char* plugin_process(void* p, const char* input_json);
void  plugin_free_string(char* s);
int   plugin_is_done(void* p);
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

TEST(FileManagerTest, CreateAndDestroy)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

TEST(FileManagerTest, CreateWithNullConfig)
{
    void* plugin = plugin_create(nullptr);
    ASSERT_NE(plugin, nullptr);
    plugin_destroy(plugin);
}

TEST(FileManagerTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 5; ++i)
    {
        void* plugin = plugin_create(nullptr);
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin_is_done(plugin), 0);
        plugin_destroy(plugin);
    }
}

// ====== List Action ======

TEST(FileManagerTest, ListRootDirectory)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"list","path":"/"})");
    EXPECT_TRUE(resultHasType(result, "listing") || resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, ListHomeDirectory)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"list","path":"/home"})");
    EXPECT_TRUE(resultHasType(result, "listing") || resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, ListInvalidPath)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"list","path":"/__nonexistent_xyz__"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, ListMultipleTimes)
{
    void* plugin = plugin_create(nullptr);
    for (int i = 0; i < 5; ++i) {
        std::string result = callProcess(plugin, R"({"action":"list","path":"/"})");
        EXPECT_TRUE(resultHasType(result, "listing") || resultHasType(result, "error"));
    }
    plugin_destroy(plugin);
}

// ====== Read Action ======

TEST(FileManagerTest, ReadNonexistentFile)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"read","path":"/__nonexistent__.txt"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

// ====== Missing Action ======

TEST(FileManagerTest, MissingActionField)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"path":"/"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, UnknownAction)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"__unknown__","path":"/"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

// ====== Invalid JSON ======

TEST(FileManagerTest, InvalidJson)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, "not json");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, EmptyArrayInput)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, "[]");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, EmptyStringInput)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, "");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

// ====== Mkdir / Write / Remove ======

TEST(FileManagerTest, MkdirAction)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"mkdir","path":"/test_mkdir"})");
    EXPECT_TRUE(resultHasType(result, "ok") || resultHasType(result, "error"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, WriteAction)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"write","path":"/test.txt","content":"hello"})");
    EXPECT_TRUE(resultHasType(result, "ok"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, RemoveNonexistentFile)
{
    void* plugin = plugin_create(nullptr);
    std::string result = callProcess(plugin, R"({"action":"remove","path":"/__nonexistent__.txt"})");
    EXPECT_TRUE(resultHasType(result, "error"));
    plugin_destroy(plugin);
}

// ====== IsDone Always False ======

TEST(FileManagerTest, IsDoneAlwaysFalse)
{
    void* plugin = plugin_create(nullptr);
    EXPECT_EQ(plugin_is_done(plugin), 0);
    callProcess(plugin, R"({"action":"list","path":"/"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);
    callProcess(plugin, R"({"action":"read","path":"/nonexistent"})");
    EXPECT_EQ(plugin_is_done(plugin), 0);
    plugin_destroy(plugin);
}

// ====== 压力测试 ======

TEST(FileManagerTest, StressListManyTimes)
{
    void* plugin = plugin_create(nullptr);
    for (int i = 0; i < 100; ++i) {
        std::string result = callProcess(plugin, R"({"action":"list","path":"/"})");
        ASSERT_FALSE(result.empty());
    }
    plugin_destroy(plugin);
}

TEST(FileManagerTest, MultipleInstancesIndependent)
{
    void* plugin1 = plugin_create(nullptr);
    void* plugin2 = plugin_create(nullptr);
    ASSERT_NE(plugin1, nullptr);
    ASSERT_NE(plugin2, nullptr);
    EXPECT_NE(plugin1, plugin2);

    std::string r1 = callProcess(plugin1, R"({"action":"list","path":"/"})");
    std::string r2 = callProcess(plugin2, R"({"action":"list","path":"/"})");
    EXPECT_FALSE(r1.empty());
    EXPECT_FALSE(r2.empty());

    plugin_destroy(plugin1);
    plugin_destroy(plugin2);
}

TEST(FileManagerTest, WriteThenRead)
{
    void* plugin = plugin_create(nullptr);
    std::string w = callProcess(plugin, R"({"action":"write","path":"/test_rw.txt","content":"test content"})");
    EXPECT_TRUE(resultHasType(w, "ok"));
    std::string r = callProcess(plugin, R"({"action":"read","path":"/test_rw.txt"})");
    EXPECT_TRUE(resultHasType(r, "file"));
    plugin_destroy(plugin);
}

TEST(FileManagerTest, WriteThenRemove)
{
    void* plugin = plugin_create(nullptr);
    EXPECT_TRUE(resultHasType(callProcess(plugin, R"({"action":"write","path":"/test_rm.txt","content":"x"})"), "ok"));
    EXPECT_TRUE(resultHasType(callProcess(plugin, R"({"action":"remove","path":"/test_rm.txt"})"), "ok"));
    plugin_destroy(plugin);
}
