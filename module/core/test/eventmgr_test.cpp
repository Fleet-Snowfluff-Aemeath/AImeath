#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include "eventmgr.hpp"
#include "threadmgr.hpp"

// ---- typed events ----
struct E1 {};
struct E2 {};
struct EA {};
struct EB {};
struct ENone {};
struct EData { int value; };

// ====== Subscribe & Fire ======

TEST(EventBusTest, SubscribeAndFireSync)
{
    EventBus bus;
    std::atomic<int> count{0};
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    bus.fire(E1{});
    EXPECT_EQ(count.load(), 1);
}

TEST(EventBusTest, MultipleSubscribers)
{
    EventBus bus;
    std::atomic<int> count{0};
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    bus.fire(E1{});
    EXPECT_EQ(count.load(), 2);
}

TEST(EventBusTest, UnsubscribePreventsFire)
{
    EventBus bus;
    std::atomic<int> count{0};
    auto sub = bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    sub.disconnect();
    bus.fire(E1{});
    EXPECT_EQ(count.load(), 0);
}

TEST(EventBusTest, AsyncFire)
{
    ThreadPool pool(2);
    EventBus bus(threadPoolExecutor(pool));
    std::atomic<int> count{0};
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    bus.fireAsync(E1{});
    pool.wait_all();
    EXPECT_EQ(count.load(), 1);
}

TEST(EventBusTest, PriorityOrder)
{
    EventBus bus;
    std::vector<int> order;
    bus.subscribe<E1>([&](const E1&) { order.push_back(1); }, 0);
    bus.subscribe<E1>([&](const E1&) { order.push_back(2); }, 10);
    bus.subscribe<E1>([&](const E1&) { order.push_back(3); }, 5);
    bus.fire(E1{});
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(order[2], 1);
}

TEST(EventBusTest, SubscriberCount)
{
    EventBus bus;
    EXPECT_EQ(bus.subscriberCount<E1>(), 0u);
    bus.subscribe<E1>([](const E1&) {});
    bus.subscribe<E1>([](const E1&) {});
    EXPECT_EQ(bus.subscriberCount<E1>(), 2u);
}

TEST(EventBusTest, FireWithNoSubscribers)
{
    EventBus bus;
    EXPECT_NO_THROW(bus.fire(ENone{}));
}

TEST(EventBusTest, DifferentEventTypesDontInterfere)
{
    EventBus bus;
    std::atomic<int> a{0}, b{0};
    bus.subscribe<EA>([&](const EA&) { a.fetch_add(1); });
    bus.subscribe<EB>([&](const EB&) { b.fetch_add(1); });
    bus.fire(EA{});
    EXPECT_EQ(a.load(), 1);
    EXPECT_EQ(b.load(), 0);
    bus.fire(EB{});
    EXPECT_EQ(a.load(), 1);
    EXPECT_EQ(b.load(), 1);
}

TEST(EventBusTest, DisconnectNonExistentNoCrash)
{
    Subscription sub;
    EXPECT_NO_THROW(sub.disconnect());
}

TEST(EventBusTest, SubscribeDuringFire)
{
    EventBus bus;
    std::atomic<int> count{0};
    bus.subscribe<E1>([&](const E1&) {
        bus.subscribe<E2>([&](const E2&) { count.fetch_add(1); });
    });
    bus.fire(E1{});
    bus.fire(E2{});
    EXPECT_EQ(count.load(), 1);
}

TEST(EventBusTest, UnsubscribeDuringFire)
{
    EventBus bus;
    std::atomic<int> count{0};
    Subscription sub;
    sub = bus.subscribe<E1>([&](const E1&) {
        sub.disconnect();
    });
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    bus.fire(E1{});
    EXPECT_EQ(count.load(), 1);
    bus.fire(E1{});
    EXPECT_EQ(count.load(), 2);
}

TEST(EventBusTest, NestedFire)
{
    EventBus bus;
    std::vector<int> order;
    bus.subscribe<E1>([&](const E1&) {
        order.push_back(1);
        bus.fire(E2{});
        order.push_back(3);
    });
    bus.subscribe<E2>([&](const E2&) { order.push_back(2); });
    bus.fire(E1{});
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(EventBusTest, CustomData)
{
    EventBus bus;
    int value = 0;
    bus.subscribe<EData>([&](const EData& e) { value = e.value; });
    bus.fire(EData{42});
    EXPECT_EQ(value, 42);
}

TEST(EventBusTest, SubscriberCountAfterUnsubscribe)
{
    EventBus bus;
    auto sub = bus.subscribe<E1>([](const E1&) {});
    bus.subscribe<E1>([](const E1&) {});
    EXPECT_EQ(bus.subscriberCount<E1>(), 2u);
    sub.disconnect();
    EXPECT_EQ(bus.subscriberCount<E1>(), 1u);
}

TEST(EventBusTest, FireExceptionIsolates)
{
    EventBus bus;
    std::vector<int> order;
    bus.subscribe<E1>([&](const E1&) {
        order.push_back(1);
        throw std::runtime_error("callback error");
    });
    bus.subscribe<E1>([&](const E1&) { order.push_back(2); });
    EXPECT_NO_THROW(bus.fire(E1{}));
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST(EventBusTest, ManySubscribers)
{
    EventBus bus;
    std::atomic<int> count{0};
    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
        bus.subscribe<E1>([&](const E1&) { count.fetch_add(1, std::memory_order_relaxed); });
    bus.fire(E1{});
    EXPECT_EQ(count.load(), N);
}

TEST(EventBusTest, ManyEvents)
{
    EventBus bus;
    std::atomic<int> count{0};
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1, std::memory_order_relaxed); });
    constexpr int N = 1000;
    for (int i = 0; i < N; ++i)
        bus.fire(E1{});
    EXPECT_EQ(count.load(), N);
}

TEST(EventBusTest, ConcurrentFireAsync)
{
    ThreadPool pool(4);
    EventBus bus(threadPoolExecutor(pool));
    std::atomic<int> count{0};
    bus.subscribe<E1>([&](const E1&) { count.fetch_add(1, std::memory_order_relaxed); });
    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
        bus.fireAsync(E1{});
    pool.wait_all();
    EXPECT_EQ(count.load(), N);
}

TEST(EventBusTest, SubscriptionRaii)
{
    EventBus bus;
    std::atomic<int> count{0};
    {
        auto sub = bus.subscribe<E1>([&](const E1&) { count.fetch_add(1); });
    }
    bus.fire(E1{});
    EXPECT_EQ(count.load(), 0);
}
