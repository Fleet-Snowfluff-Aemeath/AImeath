#include <gtest/gtest.h>
#include "app_manager.hpp"
#include "config.hpp"
#include <boost/json.hpp>

TEST(AppManagerTest, SingletonExists)
{
    auto& mgr = AppManager::instance();
    SUCCEED();
}

TEST(AppManagerTest, ListAppsInitiallyEmpty)
{
    auto arr = AppManager::instance().listApps();
    EXPECT_TRUE(arr.empty());
}

TEST(AppManagerTest, SubscribeAndNotify)
{
    auto& mgr = AppManager::instance();
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

TEST(AppManagerTest, UnsubscribeStopsNotifications)
{
    auto& mgr = AppManager::instance();
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

TEST(AppManagerTest, MultipleSubscribers)
{
    auto& mgr = AppManager::instance();
    int count1 = 0, count2 = 0;

    auto h1 = mgr.subscribe([&](const std::string&, const boost::json::value&) { ++count1; });
    auto h2 = mgr.subscribe([&](const std::string&, const boost::json::value&) { ++count2; });

    mgr.notifyStateChange("test", boost::json::object{{"value", 1}});

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);

    mgr.unsubscribe(h1);
    mgr.unsubscribe(h2);
}

TEST(AppManagerTest, SubscriberExceptionIsolated)
{
    auto& mgr = AppManager::instance();
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
