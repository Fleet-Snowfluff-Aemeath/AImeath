#include <gtest/gtest.h>
#include "plugin_manager.hpp"
#include "plugin_mod.hpp"
#include "config.hpp"
#include <boost/json.hpp>

class PluginManagerTestFixture : public ::testing::Test
{
protected:
    PluginModuleCache cache;
    PluginManager& mgr = PluginManager::instance();

    void SetUp() override
    {
        mgr.init(&cache);
    }
};

// ---- singleton ----

TEST(PluginManagerTest, SingletonExists)
{
    auto& mgr = PluginManager::instance();
    SUCCEED();
}

// ---- init ----

TEST_F(PluginManagerTestFixture, InitDoesNotCrash)
{
    mgr.init(&cache);
    SUCCEED();
}

TEST_F(PluginManagerTestFixture, InitTwiceDoesNotCrash)
{
    mgr.init(&cache);
    mgr.init(&cache);
    SUCCEED();
}

// ---- openPlugin ----

TEST_F(PluginManagerTestFixture, OpenPluginWithoutInit)
{
    mgr.init(nullptr);
    EXPECT_FALSE(mgr.openPlugin("mock_plugin", "{}"));
    mgr.init(&cache);
}

TEST_F(PluginManagerTestFixture, OpenPluginInvalidModule)
{
    EXPECT_FALSE(mgr.openPlugin("nonexistent_module_xyz", "{}"));
}

TEST_F(PluginManagerTestFixture, OpenPluginValidModule)
{
    EXPECT_TRUE(mgr.openPlugin("mock_plugin", "{}"));
}

// ---- closePlugin ----

TEST_F(PluginManagerTestFixture, ClosePlugin)
{
    EXPECT_TRUE(mgr.closePlugin("mock_plugin"));
}

// ---- getPluginState ----

TEST_F(PluginManagerTestFixture, GetPluginStateWithoutSession)
{
    auto result = mgr.getPluginState("nonexistent_plugin");
    EXPECT_TRUE(result.is_null());
}

// ---- controlPlugin ----

TEST_F(PluginManagerTestFixture, ControlPluginWithoutSession)
{
    auto result = mgr.controlPlugin("nonexistent_plugin", R"({"action":"test"})");
    EXPECT_TRUE(result.is_null());
}

TEST_F(PluginManagerTestFixture, ControlPluginChatWithoutSession)
{
    auto result = mgr.controlPlugin("chat", R"({"action":"test"})");
    EXPECT_TRUE(result.is_null());
}

// ---- listPlugins ----

TEST_F(PluginManagerTestFixture, ListPluginsInitiallyEmpty)
{
    auto arr = mgr.listPlugins();
    EXPECT_TRUE(arr.empty());
}

// ---- subscribe / notify ----

TEST_F(PluginManagerTestFixture, SubscribeAndNotify)
{
    std::string receivedPlugin;
    boost::json::value receivedState;

    auto handle = mgr.subscribe(
        [&](const std::string& pluginName, const boost::json::value& state) {
            receivedPlugin = pluginName;
            receivedState = state;
        });

    boost::json::object state;
    state["over"] = true;
    state["score"] = 100;

    mgr.notifyStateChange("snake", boost::json::value(state));

    EXPECT_EQ(receivedPlugin, "snake");
    EXPECT_TRUE(receivedState.is_object());
    EXPECT_TRUE(receivedState.as_object()["over"].as_bool());

    mgr.unsubscribe(handle);
}

TEST_F(PluginManagerTestFixture, UnsubscribeStopsNotifications)
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

TEST_F(PluginManagerTestFixture, MultipleSubscribers)
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

TEST_F(PluginManagerTestFixture, SubscriberExceptionIsolated)
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

TEST_F(PluginManagerTestFixture, SubscribeReturnsUniqueHandles)
{
    auto h1 = mgr.subscribe([](const std::string&, const boost::json::value&) {});
    auto h2 = mgr.subscribe([](const std::string&, const boost::json::value&) {});
    EXPECT_NE(h1, h2);
    mgr.unsubscribe(h1);
    mgr.unsubscribe(h2);
}

TEST_F(PluginManagerTestFixture, UnsubscribeNonExistent)
{
    EXPECT_NO_THROW(mgr.unsubscribe(0));
    EXPECT_NO_THROW(mgr.unsubscribe(99999));
}

TEST_F(PluginManagerTestFixture, NotifyStateChangeWithNoSubscribers)
{
    EXPECT_NO_THROW(
        mgr.notifyStateChange("test", boost::json::object{{"x", 1}})
    );
}

// ---- window management ----

TEST_F(PluginManagerTestFixture, RegisterWindowAndList)
{
    mgr.registerWindow("win_1", "sess_1", "test_plugin");

    auto windows = mgr.listActiveWindows();
    EXPECT_TRUE(windows.empty());

    mgr.unregisterWindow("win_1");
}

TEST_F(PluginManagerTestFixture, WindowRegistryCrud)
{
    mgr.registerWindow("w1", "s1", "plugin_a");
    mgr.registerWindow("w2", "s2", "plugin_b");

    mgr.unregisterWindow("w1");
    mgr.unregisterWindow("w2");
    SUCCEED();
}

TEST_F(PluginManagerTestFixture, MultipleUnregisterWindow)
{
    mgr.registerWindow("win_x", "sess_x", "test_plugin");
    mgr.unregisterWindow("win_x");
    EXPECT_NO_THROW(mgr.unregisterWindow("win_x"));
}
