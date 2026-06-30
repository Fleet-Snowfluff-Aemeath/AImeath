#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include "message_queue.hpp"

TEST(MessageQueueTest, PushAndTryPop)
{
    MessageQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    int v = 0;
    EXPECT_TRUE(q.try_pop(v));
    EXPECT_EQ(v, 1);
    EXPECT_TRUE(q.try_pop(v));
    EXPECT_EQ(v, 2);
    EXPECT_TRUE(q.try_pop(v));
    EXPECT_EQ(v, 3);
    EXPECT_FALSE(q.try_pop(v));
}

TEST(MessageQueueTest, SizeAfterPushPop)
{
    MessageQueue<int> q;
    EXPECT_EQ(q.size(), 0u);
    EXPECT_TRUE(q.empty());

    q.push(42);
    EXPECT_EQ(q.size(), 1u);
    EXPECT_FALSE(q.empty());

    int v = 0;
    q.try_pop(v);
    EXPECT_EQ(q.size(), 0u);
    EXPECT_TRUE(q.empty());
}

TEST(MessageQueueTest, ClearEmptiesQueue)
{
    MessageQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.size(), 3u);

    q.clear();
    EXPECT_EQ(q.size(), 0u);
    EXPECT_TRUE(q.empty());

    int v = 0;
    EXPECT_FALSE(q.try_pop(v));
}

TEST(MessageQueueTest, ThreadSafetyPushAndTryPop)
{
    MessageQueue<int> q;
    const int N = 10000;
    std::atomic<int> sum{0};

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i)
            q.push(i);
    });

    std::thread consumer([&]() {
        int received = 0;
        while (received < N) {
            int v = 0;
            if (q.try_pop(v)) {
                sum.fetch_add(v);
                ++received;
            }
        }
    });

    producer.join();
    consumer.join();

    int expected = (N - 1) * N / 2;
    EXPECT_EQ(sum.load(), expected);
}

TEST(MessageQueueTest, MoveOnlyType)
{
    MessageQueue<std::unique_ptr<int>> q;
    q.push(std::make_unique<int>(42));
    q.push(std::make_unique<int>(100));

    std::unique_ptr<int> v;
    EXPECT_TRUE(q.try_pop(v));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 42);

    EXPECT_TRUE(q.try_pop(v));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 100);

    EXPECT_FALSE(q.try_pop(v));
}

TEST(MessageQueueTest, StringType)
{
    MessageQueue<std::string> q;
    q.push(std::string("hello"));
    q.push(std::string("world"));

    std::string v;
    EXPECT_TRUE(q.try_pop(v));
    EXPECT_EQ(v, "hello");
    EXPECT_TRUE(q.try_pop(v));
    EXPECT_EQ(v, "world");
}

TEST(MessageQueueTest, MoveConstructor)
{
    MessageQueue<int> q1;
    q1.push(1);
    q1.push(2);

    MessageQueue<int> q2(std::move(q1));

    int v = 0;
    EXPECT_TRUE(q2.try_pop(v));
    EXPECT_EQ(v, 1);
    EXPECT_TRUE(q2.try_pop(v));
    EXPECT_EQ(v, 2);
    EXPECT_FALSE(q2.try_pop(v));
}

TEST(MessageQueueTest, PopBlocking)
{
    MessageQueue<int> q;
    std::atomic<bool> ready{false};

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ready = true;
        q.push(99);
    });

    int v = q.pop();
    EXPECT_TRUE(ready);
    EXPECT_EQ(v, 99);

    producer.join();
}

TEST(MessageQueueTest, ShutdownWakesPop)
{
    MessageQueue<int> q;
    std::atomic<bool> done{false};

    std::thread waiter([&]() {
        int v = q.pop();
        EXPECT_EQ(v, 0);
        done = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.shutdown();

    waiter.join();
    EXPECT_TRUE(done);
}

TEST(MessageQueueTest, PushAfterShutdownIsIgnored)
{
    MessageQueue<int> q;
    q.shutdown();
    q.push(42);

    int v = 0;
    EXPECT_FALSE(q.try_pop(v));
    EXPECT_EQ(q.size(), 0u);
}
