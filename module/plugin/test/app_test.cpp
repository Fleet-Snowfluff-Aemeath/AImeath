#include <gtest/gtest.h>
#include "plugin_module.hpp"
#include "plugin_mod.hpp"
#include <string>
#include <cstring>
#include <thread>
#include <vector>

// ---- PluginModule / PluginPtr ----

TEST(PluginModuleTest, DefaultModuleIsInvalid)
{
    PluginModule m;
    EXPECT_FALSE(m);
}

TEST(PluginModuleTest, CreateWithNullDeleterReturnsNull)
{
    PluginModule m;
    PluginPtr p = m.create("test");
    EXPECT_EQ(p.get(), nullptr);
}

TEST(PluginModuleTest, IsAsyncFalseForMock)
{
    PluginModuleCache cache;
    auto m = cache.load("mock_plugin");
    ASSERT_TRUE(m);
    EXPECT_FALSE(m.is_async());
}

// ---- PluginPtr with custom deleter ----

struct TestPlugin { int val; };

TEST(PluginPtrTest, UniquePtrLifetime)
{
    auto* raw = new TestPlugin{42};
    {
        PluginDeleter d;
        d.deleter = [](void* p) { delete static_cast<TestPlugin*>(p); };
        PluginPtr ptr(raw, d);
        EXPECT_EQ(static_cast<TestPlugin*>(ptr.get())->val, 42);
    }
    SUCCEED();
}

TEST(PluginPtrTest, DefaultPluginPtrIsNull)
{
    PluginPtr p;
    EXPECT_EQ(p.get(), nullptr);
}

// ---- PluginModuleCache with mock .so ----

TEST(PluginModuleCacheTest, LoadMock)
{
    PluginModuleCache cache;
    auto m = cache.load("mock_plugin");
    ASSERT_TRUE(m) << "mock_plugin.so must be built first";

    auto plugin = m.create(R"({"cmd":"init"})");
    ASSERT_NE(plugin.get(), nullptr);

    EXPECT_FALSE(m.plugin_is_done(plugin.get()));

    char* out = m.plugin_process(plugin.get(), "hello");
    ASSERT_NE(out, nullptr);
    std::string result(out);
    m.plugin_free_string(out);

    EXPECT_TRUE(result.find("mock") != std::string::npos);
    EXPECT_TRUE(result.find("hello") != std::string::npos);
}

TEST(PluginModuleCacheTest, LoadNonexistent)
{
    PluginModuleCache cache;
    auto m = cache.load("nonexistent_module_xyz");
    EXPECT_FALSE(m);
}

TEST(PluginModuleCacheTest, CacheHit)
{
    PluginModuleCache cache;
    auto m1 = cache.load("mock_plugin");
    ASSERT_TRUE(m1);
    auto m2 = cache.load("mock_plugin");
    EXPECT_TRUE(m2);
}

TEST(PluginModuleCacheTest, Evict)
{
    PluginModuleCache cache;
    auto m = cache.load("mock_plugin");
    ASSERT_TRUE(m);
    cache.evict("mock_plugin");
    auto m2 = cache.load("mock_plugin");
    EXPECT_TRUE(m2);
}

TEST(PluginModuleCacheTest, EvictNonExistent)
{
    PluginModuleCache cache;
    EXPECT_NO_THROW(cache.evict("not_in_cache"));
}

TEST(PluginModuleCacheTest, Clear)
{
    PluginModuleCache cache;
    auto m1 = cache.load("mock_plugin");
    ASSERT_TRUE(m1);
    cache.clear();
    auto m2 = cache.load("mock_plugin");
    EXPECT_TRUE(m2);
}

TEST(PluginModuleCacheTest, ClearEmptyCache)
{
    PluginModuleCache cache;
    EXPECT_NO_THROW(cache.clear());
}

TEST(PluginModuleCacheTest, StressLoadEvict)
{
    PluginModuleCache cache;
    constexpr int N = 50;
    for (int i = 0; i < N; ++i)
    {
        auto m = cache.load("mock_plugin");
        EXPECT_TRUE(m);
        cache.evict("mock_plugin");
    }
}

TEST(PluginModuleCacheTest, ConcurrentLoad)
{
    PluginModuleCache cache;
    std::atomic<int> success{0};
    constexpr int N = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < N; ++i)
    {
        threads.emplace_back([&]() {
            auto m = cache.load("mock_plugin");
            if (m) success.fetch_add(1);
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(success.load(), N);
}


