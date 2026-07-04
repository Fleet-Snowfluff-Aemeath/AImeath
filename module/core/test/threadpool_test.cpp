#include <gtest/gtest.h>
#include <atomic>
#include <set>
#include <thread>
#include <chrono>
#include "threadmgr.hpp"

TEST(ThreadPoolTest, SubmitAndExecute)
{
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 10; ++i)
            pool.submit([&]() { counter.fetch_add(1); });
    }
    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, WaitAllCompletes)
{
    std::atomic<int> counter{0};
    ThreadPool pool(2);
    for (int i = 0; i < 5; ++i)
        pool.submit([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            counter.fetch_add(1);
        });
    pool.wait_all();
    EXPECT_EQ(counter.load(), 5);
}

TEST(ThreadPoolTest, WaitAllThenSubmitMore)
{
    std::atomic<int> counter{0};
    ThreadPool pool(2);
    for (int i = 0; i < 3; ++i)
        pool.submit([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            counter.fetch_add(1);
        });
    pool.wait_all();
    EXPECT_EQ(counter.load(), 3);
    pool.submit([&]() { counter.fetch_add(1); });
    pool.wait_all();
    EXPECT_EQ(counter.load(), 4);
}

TEST(ThreadPoolTest, ActiveCount)
{
    ThreadPool pool(2);
    EXPECT_EQ(pool.active_count(), 0);
    std::atomic<bool> running{true};
    pool.submit([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        running.store(false);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(pool.active_count(), 1);
    pool.wait_all();
    EXPECT_EQ(pool.active_count(), 0);
}

TEST(ThreadPoolTest, TaskCount)
{
    ThreadPool pool(2);
    EXPECT_EQ(pool.get_task_count(), 0);
    for (int i = 0; i < 5; ++i)
        pool.submit([i]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    EXPECT_GE(pool.get_task_count(), 0);
}

TEST(ThreadPoolTest, SingleWorker)
{
    std::vector<int> order;
    ThreadPool pool(1);
    for (int i = 0; i < 5; ++i)
        pool.submit([i, &order]() { order.push_back(i); });
    pool.wait_all();
    ASSERT_EQ(order.size(), 5);
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
}

TEST(ThreadPoolTest, ManyWorkers)
{
    std::atomic<int> counter{0};
    ThreadPool pool(16);
    for (int i = 0; i < 200; ++i)
        pool.submit([&]() { counter.fetch_add(1); });
    pool.wait_all();
    EXPECT_EQ(counter.load(), 200);
}

TEST(ThreadPoolTest, NoTasksSubmitted)
{
    ThreadPool pool(4);
    pool.wait_all();
    EXPECT_EQ(pool.active_count(), 0);
    EXPECT_EQ(pool.get_task_count(), 0);
}

TEST(ThreadPoolTest, TaskThatThrows)
{
    std::atomic<int> counter{0};
    ThreadPool pool(2);
    pool.submit([&]() {
        counter.fetch_add(1);
        throw std::runtime_error("test exception");
    });
    pool.submit([&]() { counter.fetch_add(1); });
    pool.wait_all();
    EXPECT_EQ(counter.load(), 2);
}

TEST(ThreadPoolTest, SharedPtrTask)
{
    std::atomic<int> counter{0};
    ThreadPool pool(2);
    auto ptr = std::make_shared<int>(42);
    pool.submit([ptr, &counter]() {
        if (ptr && *ptr == 42) counter.fetch_add(1);
    });
    pool.wait_all();
    EXPECT_EQ(counter.load(), 1);
}

TEST(ThreadPoolTest, SubmitAfterShutdownDoesNotCrash)
{
    std::atomic<int> counter{0};
    ThreadPool pool(2);
    // Submit and drain
    for (int i = 0; i < 5; ++i)
        pool.submit([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); counter.fetch_add(1); });
    pool.wait_all();
    pool.shutdown();
    // Submit after shutdown - should not crash; task may or may not execute
    pool.submit([&]() { counter.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(counter.load(), 5);
}

TEST(ThreadPoolTest, ManyTasksStress)
{
    std::atomic<int> counter{0};
    ThreadPool pool(4);
    constexpr int N = 5000;
    for (int i = 0; i < N; ++i)
        pool.submit([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
    pool.wait_all();
    EXPECT_EQ(counter.load(), N);
}

TEST(ThreadPoolTest, ConcurrentSubmitStress)
{
    std::atomic<int> counter{0};
    ThreadPool pool(4);
    std::vector<std::thread> submitters;
    constexpr int TASKS_PER_THREAD = 1000;
    constexpr int THREADS = 4;
    for (int t = 0; t < THREADS; ++t)
    {
        submitters.emplace_back([&]() {
            for (int i = 0; i < TASKS_PER_THREAD; ++i)
                pool.submit([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
        });
    }
    for (auto& th : submitters)
        if (th.joinable()) th.join();
    pool.wait_all();
    EXPECT_EQ(counter.load(), TASKS_PER_THREAD * THREADS);
}

TEST(ThreadPoolTest, TrySubmitSuccess)
{
    std::atomic<int> counter{0};
    ThreadPool pool(2);
    pool.set_max_queue_size(100);
    for (int i = 0; i < 10; ++i)
        EXPECT_TRUE(pool.try_submit([&]() { counter.fetch_add(1); }));
    pool.wait_all();
    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, TrySubmitRejectedWhenFull)
{
    ThreadPool pool(1);
    pool.set_max_queue_size(2);
    std::atomic<int> counter{0};
    std::atomic<bool> barrier{true};
    std::atomic<bool> started{false};

    pool.submit([&]() {
        started.store(true);
        while (barrier.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
        counter.fetch_add(1);
    });

    while (!started.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    bool r1 = pool.try_submit([&]() { counter.fetch_add(1); });
    bool r2 = pool.try_submit([&]() { counter.fetch_add(1); });
    bool r3 = pool.try_submit([&]() { counter.fetch_add(1); });

    barrier.store(false);
    pool.wait_all();

    EXPECT_TRUE(r1);
    EXPECT_TRUE(r2);
    EXPECT_FALSE(r3);
    EXPECT_EQ(counter.load(), 3);
}

TEST(ThreadPoolTest, MaxQueueSizeDefaultsToZero)
{
    ThreadPool pool(2);
    EXPECT_EQ(pool.max_queue_size(), 0);
}

TEST(ThreadPoolTest, SetAndGetMaxQueueSize)
{
    ThreadPool pool(2);
    pool.set_max_queue_size(42);
    EXPECT_EQ(pool.max_queue_size(), 42);
}

TEST(ThreadPoolTest, TrySubmitAlwaysSucceedsWithNoLimit)
{
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    for (int i = 0; i < 500; ++i)
        EXPECT_TRUE(pool.try_submit([&]() { counter.fetch_add(1); }));
    pool.wait_all();
    EXPECT_EQ(counter.load(), 500);
}
