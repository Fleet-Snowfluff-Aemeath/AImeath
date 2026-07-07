#include <gtest/gtest.h>
#include "app_module.hpp"
#include "app_mod.hpp"
#include <string>
#include <cstring>
#include <thread>
#include <vector>

// ---- AppModule / AppPtr ----

TEST(AppModuleTest, DefaultModuleIsInvalid)
{
    AppModule m;
    EXPECT_FALSE(m);
}

TEST(AppModuleTest, CreateWithNullDeleterReturnsNull)
{
    AppModule m;
    AppPtr p = m.create("test");
    EXPECT_EQ(p.get(), nullptr);
}

TEST(AppModuleTest, IsAsyncFalseForMock)
{
    AppModuleCache cache;
    auto m = cache.load("mock_app");
    ASSERT_TRUE(m);
    EXPECT_FALSE(m.is_async());
}

// ---- AppPtr with custom deleter ----

struct TestApp { int val; };

TEST(AppPtrTest, UniquePtrLifetime)
{
    auto* raw = new TestApp{42};
    {
        AppDeleter d;
        d.deleter = [](void* p) { delete static_cast<TestApp*>(p); };
        AppPtr ptr(raw, d);
        EXPECT_EQ(static_cast<TestApp*>(ptr.get())->val, 42);
    }
    SUCCEED();
}

TEST(AppPtrTest, DefaultAppPtrIsNull)
{
    AppPtr p;
    EXPECT_EQ(p.get(), nullptr);
}

// ---- AppModuleCache with mock .so ----

TEST(AppModuleCacheTest, LoadMock)
{
    AppModuleCache cache;
    auto m = cache.load("mock_app");
    ASSERT_TRUE(m) << "mock_app.so must be built first";

    auto app = m.create(R"({"cmd":"init"})");
    ASSERT_NE(app.get(), nullptr);

    EXPECT_FALSE(m.app_is_done(app.get()));

    char* out = m.app_process(app.get(), "hello");
    ASSERT_NE(out, nullptr);
    std::string result(out);
    m.app_free_string(out);

    EXPECT_TRUE(result.find("mock") != std::string::npos);
    EXPECT_TRUE(result.find("hello") != std::string::npos);
}

TEST(AppModuleCacheTest, LoadNonexistent)
{
    AppModuleCache cache;
    auto m = cache.load("nonexistent_module_xyz");
    EXPECT_FALSE(m);
}

TEST(AppModuleCacheTest, CacheHit)
{
    AppModuleCache cache;
    auto m1 = cache.load("mock_app");
    ASSERT_TRUE(m1);
    auto m2 = cache.load("mock_app");
    EXPECT_TRUE(m2);
}

TEST(AppModuleCacheTest, Evict)
{
    AppModuleCache cache;
    auto m = cache.load("mock_app");
    ASSERT_TRUE(m);
    cache.evict("mock_app");
    auto m2 = cache.load("mock_app");
    EXPECT_TRUE(m2);
}

TEST(AppModuleCacheTest, EvictNonExistent)
{
    AppModuleCache cache;
    EXPECT_NO_THROW(cache.evict("not_in_cache"));
}

TEST(AppModuleCacheTest, Clear)
{
    AppModuleCache cache;
    auto m1 = cache.load("mock_app");
    ASSERT_TRUE(m1);
    cache.clear();
    auto m2 = cache.load("mock_app");
    EXPECT_TRUE(m2);
}

TEST(AppModuleCacheTest, ClearEmptyCache)
{
    AppModuleCache cache;
    EXPECT_NO_THROW(cache.clear());
}

TEST(AppModuleCacheTest, StressLoadEvict)
{
    AppModuleCache cache;
    constexpr int N = 50;
    for (int i = 0; i < N; ++i)
    {
        auto m = cache.load("mock_app");
        EXPECT_TRUE(m);
        cache.evict("mock_app");
    }
}

TEST(AppModuleCacheTest, ConcurrentLoad)
{
    AppModuleCache cache;
    std::atomic<int> success{0};
    constexpr int N = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < N; ++i)
    {
        threads.emplace_back([&]() {
            auto m = cache.load("mock_app");
            if (m) success.fetch_add(1);
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(success.load(), N);
}


