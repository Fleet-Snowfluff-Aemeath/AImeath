#include <gtest/gtest.h>
#include <atomic>
#include <set>
#include <thread>
#include <chrono>
#include <mutex>
#include "timer.hpp"

TEST(TimerTest, SetTimeoutFiresOnce)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    timer.setTimeout(std::chrono::milliseconds(50), [&]() { count.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(count.load(), 1);
}

TEST(TimerTest, SetIntervalFiresMultiple)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto id = timer.setInterval(std::chrono::milliseconds(50), [&]() {
        count.fetch_add(1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(230));
    timer.cancel(id);
    int v = count.load();
    EXPECT_GE(v, 3);
    EXPECT_LE(v, 6);
}

TEST(TimerTest, CancelPreventsFire)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto id = timer.setTimeout(std::chrono::milliseconds(100), [&]() {
        count.fetch_add(1);
    });
    timer.cancel(id);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(count.load(), 0);
}

TEST(TimerTest, ExistsBeforeAndAfter)
{
    ThreadPool pool(2);
    Timer timer(pool);
    auto id = timer.setTimeout(std::chrono::milliseconds(200), []() {});
    EXPECT_TRUE(timer.exists(id));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_FALSE(timer.exists(id));
}

TEST(TimerTest, MultipleTimersFireInOrder)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::vector<int> fired;
    std::mutex mtx;
    timer.setTimeout(std::chrono::milliseconds(100), [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        fired.push_back(100);
    });
    timer.setTimeout(std::chrono::milliseconds(50), [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        fired.push_back(50);
    });
    timer.setTimeout(std::chrono::milliseconds(150), [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        fired.push_back(150);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ASSERT_EQ(fired.size(), 3);
    EXPECT_EQ(fired[0], 50);
    EXPECT_EQ(fired[1], 100);
    EXPECT_EQ(fired[2], 150);
}

TEST(TimerTest, SetTimeoutAtFiresOnce)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    timer.setTimeoutAt(expiry, [&]() { count.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(count.load(), 1);
}

TEST(TimerTest, SetTimeoutAtPastExpiryFiresImmediately)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto past = std::chrono::steady_clock::now() - std::chrono::milliseconds(10);
    timer.setTimeoutAt(past, [&]() { count.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(count.load(), 1);
}

TEST(TimerTest, SetTimeoutAtCancel)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    auto id = timer.setTimeoutAt(expiry, [&]() { count.fetch_add(1); });
    timer.cancel(id);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(count.load(), 0);
}

TEST(TimerTest, SetTimeoutAtOrderWithSetTimeout)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::vector<int> fired;
    std::mutex mtx;

    auto far = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    timer.setTimeoutAt(far, [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        fired.push_back(150);
    });
    timer.setTimeout(std::chrono::milliseconds(50), [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        fired.push_back(50);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ASSERT_EQ(fired.size(), 2);
    EXPECT_EQ(fired[0], 50);
    EXPECT_EQ(fired[1], 150);
}

TEST(TimerTest, ZeroDelayFiresImmediately)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    timer.setTimeout(std::chrono::milliseconds(0), [&]() { count.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(count.load(), 1);
}

TEST(TimerTest, CancelNonExistentTimer)
{
    ThreadPool pool(2);
    Timer timer(pool);
    EXPECT_FALSE(timer.cancel(99999));
}

TEST(TimerTest, CancelAlreadyFired)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto id = timer.setTimeout(std::chrono::milliseconds(20), [&]() { count.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(count.load(), 1);
    EXPECT_FALSE(timer.cancel(id));
}

TEST(TimerTest, CancelPeriodicMidExecution)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto id = timer.setInterval(std::chrono::milliseconds(30), [&]() {
        count.fetch_add(1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.cancel(id);
    int before = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(count.load(), before);
}

TEST(TimerTest, CallbackThatThrows)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    timer.setTimeout(std::chrono::milliseconds(20), [&]() {
        count.fetch_add(1);
        throw std::runtime_error("timer error");
    });
    timer.setTimeout(std::chrono::milliseconds(40), [&]() { count.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(count.load(), 2);
}

TEST(TimerTest, ExistsAfterCancel)
{
    ThreadPool pool(2);
    Timer timer(pool);
    auto id = timer.setTimeout(std::chrono::milliseconds(200), []() {});
    EXPECT_TRUE(timer.exists(id));
    timer.cancel(id);
    EXPECT_FALSE(timer.exists(id));
}

TEST(TimerTest, PeriodicCallbackThatThrows)
{
    ThreadPool pool(2);
    Timer timer(pool);
    std::atomic<int> count{0};
    auto id = timer.setInterval(std::chrono::milliseconds(30), [&]() {
        count.fetch_add(1);
        throw std::runtime_error("periodic error");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.cancel(id);
    int v = count.load();
    EXPECT_GE(v, 2);
}

TEST(TimerTest, ManyTimersStress)
{
    ThreadPool pool(4);
    Timer timer(pool);
    std::atomic<int> count{0};
    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
        timer.setTimeout(std::chrono::milliseconds(10 + i % 50), [&]() {
            count.fetch_add(1, std::memory_order_relaxed);
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(count.load(), N);
}

TEST(TimerTest, ConcurrentCancelStress)
{
    ThreadPool pool(4);
    Timer timer(pool);
    std::atomic<int> fired{0};
    constexpr int N = 100;
    for (int iter = 0; iter < 5; ++iter)
    {
        auto id = timer.setInterval(std::chrono::milliseconds(20), [&]() {
            fired.fetch_add(1, std::memory_order_relaxed);
        });
        std::vector<std::thread> cancellers;
        for (int t = 0; t < 4; ++t)
            cancellers.emplace_back([&, id]() { timer.cancel(id); });
        for (auto& th : cancellers)
            if (th.joinable()) th.join();
    }
    (void)fired.load();
}

TEST(TimerTest, RapidCancelStress)
{
    ThreadPool pool(4);
    Timer timer(pool);
    std::atomic<int> fired{0}, cancelled{0};
    constexpr int N = 500;
    std::vector<uint64_t> ids;
    for (int i = 0; i < N; ++i)
    {
        auto id = timer.setTimeout(std::chrono::milliseconds(100), [&]() {
            fired.fetch_add(1, std::memory_order_relaxed);
        });
        ids.push_back(id);
    }
    for (auto id : ids)
        if (timer.cancel(id))
            cancelled.fetch_add(1, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(fired.load(), 0);
    EXPECT_EQ(cancelled.load(), N);
}
