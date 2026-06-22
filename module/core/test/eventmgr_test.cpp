#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include "eventmgr.hpp"

TEST(EventManagerTest, SubscribeAndFireSync)
{
    EventManager mgr;
    std::atomic<int> count{0};
    mgr.subscribe(100, [&](const Event&) { count.fetch_add(1); });
    mgr.fire({100});
    EXPECT_EQ(count.load(), 1);
}

TEST(EventManagerTest, MultipleSubscribers)
{
    EventManager mgr;
    std::atomic<int> count{0};
    mgr.subscribe(100, [&](const Event&) { count.fetch_add(1); });
    mgr.subscribe(100, [&](const Event&) { count.fetch_add(1); });
    mgr.fire({100});
    EXPECT_EQ(count.load(), 2);
}

TEST(EventManagerTest, UnsubscribePreventsFire)
{
    EventManager mgr;
    std::atomic<int> count{0};
    auto h = mgr.subscribe(100, [&](const Event&) { count.fetch_add(1); });
    mgr.unsubscribe(h);
    mgr.fire({100});
    EXPECT_EQ(count.load(), 0);
}

TEST(EventManagerTest, AsyncFire)
{
    EventManager mgr;
    std::atomic<int> count{0};
    mgr.subscribe(100, [&](const Event&) {
        count.fetch_add(1);
    });
    ThreadPool pool(2);
    mgr.fireAsync({100}, pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(count.load(), 1);
}

TEST(EventManagerTest, PriorityOrder)
{
    EventManager mgr;
    std::vector<int> order;
    mgr.subscribe(100, [&](const Event&) { order.push_back(1); }, 0);
    mgr.subscribe(100, [&](const Event&) { order.push_back(2); }, 10);
    mgr.subscribe(100, [&](const Event&) { order.push_back(3); }, 5);
    mgr.fire({100});
    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(order[2], 1);
}

TEST(EventManagerTest, SubscriberCount)
{
    EventManager mgr;
    EXPECT_EQ(mgr.subscriberCount(100), 0);
    mgr.subscribe(100, [](const Event&) {});
    mgr.subscribe(100, [](const Event&) {});
    EXPECT_EQ(mgr.subscriberCount(100), 2);
}

TEST(EventManagerTest, FireWithNoSubscribers)
{
    EventManager mgr;
    EXPECT_NO_THROW(mgr.fire({999}));
}

TEST(EventManagerTest, DifferentEventTypesDontInterfere)
{
    EventManager mgr;
    std::atomic<int> a{0}, b{0};
    mgr.subscribe(1, [&](const Event&) { a.fetch_add(1); });
    mgr.subscribe(2, [&](const Event&) { b.fetch_add(1); });
    mgr.fire({1});
    EXPECT_EQ(a.load(), 1);
    EXPECT_EQ(b.load(), 0);
    mgr.fire({2});
    EXPECT_EQ(a.load(), 1);
    EXPECT_EQ(b.load(), 1);
}

TEST(EventManagerTest, UnsubscribeNonExistent)
{
    EventManager mgr;
    EXPECT_NO_THROW(mgr.unsubscribe(EventManager::Handle{}));
}

TEST(EventManagerTest, SubscribeDuringFire)
{
    EventManager mgr;
    std::atomic<int> count{0};
    mgr.subscribe(100, [&](const Event& e) {
        mgr.subscribe(200, [&](const Event&) { count.fetch_add(1); });
    });
    mgr.fire({100});
    mgr.fire({200});
    EXPECT_EQ(count.load(), 1);
}

TEST(EventManagerTest, UnsubscribeDuringFire)
{
    EventManager mgr;
    EventManager::Handle h;
    std::atomic<int> count{0};
    h = mgr.subscribe(100, [&](const Event&) {
        mgr.unsubscribe(h);
    });
    mgr.subscribe(100, [&](const Event&) { count.fetch_add(1); });
    mgr.fire({100});
    EXPECT_EQ(count.load(), 1);
    mgr.fire({100});
    EXPECT_EQ(count.load(), 2);
}

TEST(EventManagerTest, NestedFire)
{
    EventManager mgr;
    std::vector<int> order;
    mgr.subscribe(100, [&](const Event&) {
        order.push_back(1);
        mgr.fire({200});
        order.push_back(3);
    });
    mgr.subscribe(200, [&](const Event&) { order.push_back(2); });
    mgr.fire({100});
    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(EventManagerTest, CustomData)
{
    EventManager mgr;
    int value = 0;
    mgr.subscribe(100, [&](const Event& e) {
        if (e.data.has_value()) value = std::any_cast<int>(e.data);
    });
    mgr.fire({100, 42});
    EXPECT_EQ(value, 42);
}

TEST(EventManagerTest, SubscriberCountAfterUnsubscribe)
{
    EventManager mgr;
    auto h = mgr.subscribe(100, [](const Event&) {});
    mgr.subscribe(100, [](const Event&) {});
    EXPECT_EQ(mgr.subscriberCount(100), 2);
    mgr.unsubscribe(h);
    EXPECT_EQ(mgr.subscriberCount(100), 1);
}

TEST(EventManagerTest, FireExceptionIsolates)
{
    EventManager mgr;
    std::vector<int> order;
    mgr.subscribe(100, [&](const Event&) {
        order.push_back(1);
        throw std::runtime_error("callback error");
    });
    mgr.subscribe(100, [&](const Event&) {
        order.push_back(2);
    });
    EXPECT_NO_THROW(mgr.fire({100}));
    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST(EventManagerTest, ManySubscribers)
{
    EventManager mgr;
    std::atomic<int> count{0};
    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
        mgr.subscribe(100, [&](const Event&) { count.fetch_add(1, std::memory_order_relaxed); });
    mgr.fire({100});
    EXPECT_EQ(count.load(), N);
}

TEST(EventManagerTest, ManyEvents)
{
    EventManager mgr;
    std::atomic<int> count{0};
    mgr.subscribe(100, [&](const Event&) { count.fetch_add(1, std::memory_order_relaxed); });
    constexpr int N = 1000;
    for (int i = 0; i < N; ++i)
        mgr.fire({100});
    EXPECT_EQ(count.load(), N);
}

TEST(EventManagerTest, ConcurrentFireAsync)
{
    EventManager mgr;
    std::atomic<int> count{0};
    mgr.subscribe(100, [&](const Event&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });
    ThreadPool pool(4);
    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
        mgr.fireAsync({100}, pool);
    pool.wait_all();
    EXPECT_EQ(count.load(), N);
}
