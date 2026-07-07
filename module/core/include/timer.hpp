#pragma once

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

class Timer : private boost::noncopyable
{
public:
    using TimerId = uint64_t;

    explicit Timer(boost::asio::io_context& io);
    ~Timer();

    TimerId setTimeout(std::chrono::milliseconds delay, std::function<void()> callback);
    TimerId setTimeoutAt(const std::chrono::steady_clock::time_point& expiry, std::function<void()> callback);
    TimerId setInterval(std::chrono::milliseconds interval, std::function<void()> callback);
    bool cancel(TimerId id);
    bool exists(TimerId id) const;

private:
    struct TimerEntry
    {
        std::shared_ptr<boost::asio::steady_timer> timer;
        std::shared_ptr<bool> alive;
    };

    TimerId addTimer(std::shared_ptr<boost::asio::steady_timer> timer);

    TimerId setTimeoutImpl(std::shared_ptr<boost::asio::steady_timer> timer,
                           std::function<void()> callback);

    boost::asio::io_context& m_io;
    mutable std::mutex m_mtx;
    std::unordered_map<TimerId, TimerEntry> m_timers;
    std::atomic<TimerId> m_next_id{1};
};
