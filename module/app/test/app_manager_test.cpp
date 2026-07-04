#include <gtest/gtest.h>
#include "app_manager.hpp"
#include "app_mod.hpp"
#include "config.hpp"
#include <boost/json.hpp>

class AppManagerTestFixture : public ::testing::Test
{
protected:
    AppModuleCache cache;
    AppManager& mgr = AppManager::instance();

    void SetUp() override
    {
        mgr.init(&cache);
    }
};

// ---- singleton ----

TEST(AppManagerTest, SingletonExists)
{
    auto& mgr = AppManager::instance();
    SUCCEED();
}

// ---- init ----

TEST_F(AppManagerTestFixture, InitDoesNotCrash)
{
    mgr.init(&cache);
    SUCCEED();
}

TEST_F(AppManagerTestFixture, InitTwiceDoesNotCrash)
{
    mgr.init(&cache);
    mgr.init(&cache);
    SUCCEED();
}

// ---- openApp ----

TEST_F(AppManagerTestFixture, OpenAppWithoutInit)
{
    mgr.init(nullptr);
    EXPECT_FALSE(mgr.openApp("mock_app", "{}"));
    mgr.init(&cache);
}

TEST_F(AppManagerTestFixture, OpenAppInvalidModule)
{
    EXPECT_FALSE(mgr.openApp("nonexistent_module_xyz", "{}"));
}

TEST_F(AppManagerTestFixture, OpenAppValidModule)
{
    EXPECT_TRUE(mgr.openApp("mock_app", "{}"));
}

// ---- closeApp ----

TEST_F(AppManagerTestFixture, CloseApp)
{
    EXPECT_TRUE(mgr.closeApp("mock_app"));
}

// ---- getAppState ----

TEST_F(AppManagerTestFixture, GetAppStateWithoutSession)
{
    auto result = mgr.getAppState("nonexistent_app");
    EXPECT_TRUE(result.is_null());
}

// ---- controlApp ----

TEST_F(AppManagerTestFixture, ControlAppWithoutSession)
{
    auto result = mgr.controlApp("nonexistent_app", R"({"action":"test"})");
    EXPECT_TRUE(result.is_null());
}

TEST_F(AppManagerTestFixture, ControlAppChatWithoutSession)
{
    auto result = mgr.controlApp("chat", R"({"action":"test"})");
    EXPECT_TRUE(result.is_null());
}

// ---- listApps ----

TEST_F(AppManagerTestFixture, ListAppsInitiallyEmpty)
{
    auto arr = mgr.listApps();
    EXPECT_TRUE(arr.empty());
}

// ---- subscribe / notify ----

TEST_F(AppManagerTestFixture, SubscribeAndNotify)
{
    std::string receivedApp;
    boost::json::value receivedState;

    auto handle = mgr.subscribe(
        [&](const std::string& appName, const boost::json::value& state) {
            receivedApp = appName;
            receivedState = state;
        });

    boost::json::object state;
    state["over"] = true;
    state["score"] = 100;

    mgr.notifyStateChange("snake", boost::json::value(state));

    EXPECT_EQ(receivedApp, "snake");
    EXPECT_TRUE(receivedState.is_object());
    EXPECT_TRUE(receivedState.as_object()["over"].as_bool());

    mgr.unsubscribe(handle);
}

TEST_F(AppManagerTestFixture, UnsubscribeStopsNotifications)
{
    int callCount = 0;

    auto handle = mgr.subscribe(
        [&](const std::string&, const boost::json::value&) {
            ++callCount;
        });

    mgr.notifyStateChange("snake", boost::json::object{{"test", true}});
    EXPECT_EQ(callCount, 1);

    mgr.unsubscribe(handle);

    mgr.notifyStateChange("snake", boost::json::object{{"test", true}});
    EXPECT_EQ(callCount, 1);
}

TEST_F(AppManagerTestFixture, MultipleSubscribers)
{
    int count1 = 0, count2 = 0;

    auto h1 = mgr.subscribe([&](const std::string&, const boost::json::value&) { ++count1; });
    auto h2 = mgr.subscribe([&](const std::string&, const boost::json::value&) { ++count2; });

    mgr.notifyStateChange("test", boost::json::object{{"value", 1}});

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);

    mgr.unsubscribe(h1);
    mgr.unsubscribe(h2);
}

TEST_F(AppManagerTestFixture, SubscriberExceptionIsolated)
{
    int goodCount = 0;

    auto badHandle = mgr.subscribe(
        [&](const std::string&, const boost::json::value&) {
            throw std::runtime_error("test error");
        });

    auto goodHandle = mgr.subscribe(
        [&](const std::string&, const boost::json::value&) {
            ++goodCount;
        });

    EXPECT_NO_THROW(
        mgr.notifyStateChange("test", boost::json::object{{"x", 1}})
    );

    EXPECT_EQ(goodCount, 1);

    mgr.unsubscribe(badHandle);
    mgr.unsubscribe(goodHandle);
}

TEST_F(AppManagerTestFixture, SubscribeReturnsUniqueHandles)
{
    auto h1 = mgr.subscribe([](const std::string&, const boost::json::value&) {});
    auto h2 = mgr.subscribe([](const std::string&, const boost::json::value&) {});
    EXPECT_NE(h1, h2);
    mgr.unsubscribe(h1);
    mgr.unsubscribe(h2);
}

TEST_F(AppManagerTestFixture, UnsubscribeNonExistent)
{
    EXPECT_NO_THROW(mgr.unsubscribe(0));
    EXPECT_NO_THROW(mgr.unsubscribe(99999));
}

TEST_F(AppManagerTestFixture, NotifyStateChangeWithNoSubscribers)
{
    EXPECT_NO_THROW(
        mgr.notifyStateChange("test", boost::json::object{{"x", 1}})
    );
}

// ---- window management ----

TEST_F(AppManagerTestFixture, RegisterWindowAndList)
{
    mgr.registerWindow("win_1", "sess_1", "test_app");

    auto windows = mgr.listActiveWindows();
    EXPECT_TRUE(windows.empty());

    mgr.unregisterWindow("win_1");
}

TEST_F(AppManagerTestFixture, WindowRegistryCrud)
{
    mgr.registerWindow("w1", "s1", "app_a");
    mgr.registerWindow("w2", "s2", "app_b");

    mgr.unregisterWindow("w1");
    mgr.unregisterWindow("w2");
    SUCCEED();
}

TEST_F(AppManagerTestFixture, MultipleUnregisterWindow)
{
    mgr.registerWindow("win_x", "sess_x", "test_app");
    mgr.unregisterWindow("win_x");
    EXPECT_NO_THROW(mgr.unregisterWindow("win_x"));
}
