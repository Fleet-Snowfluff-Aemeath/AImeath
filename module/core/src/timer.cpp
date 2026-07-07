#include "timer.hpp"

Timer::Timer(boost::asio::io_context& io)
    : m_io(io)
{
}

Timer::~Timer()
{
    std::unordered_map<TimerId, TimerEntry> timers;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        timers = std::move(m_timers);
    }
    for (auto& [_, entry] : timers)
        entry.timer->cancel();
}

Timer::TimerId Timer::addTimer(std::shared_ptr<boost::asio::steady_timer> timer)
{
    auto id = m_next_id.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_timers[id] = TimerEntry{std::move(timer), nullptr};
    }
    return id;
}

Timer::TimerId Timer::setTimeoutImpl(std::shared_ptr<boost::asio::steady_timer> timer,
                                      std::function<void()> callback)
{
    auto id = addTimer(timer);
    timer->async_wait([this, id, cb = std::move(callback)](const boost::system::error_code& ec) mutable {
        if (ec) return;
        bool should_call = false;
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            should_call = (m_timers.erase(id) > 0);
        }
        if (should_call)
            cb();
    });
    return id;
}

Timer::TimerId Timer::setTimeout(std::chrono::milliseconds delay, std::function<void()> callback)
{
    auto timer = std::make_shared<boost::asio::steady_timer>(m_io);
    timer->expires_after(delay);
    return setTimeoutImpl(std::move(timer), std::move(callback));
}

Timer::TimerId Timer::setTimeoutAt(const std::chrono::steady_clock::time_point& expiry, std::function<void()> callback)
{
    auto timer = std::make_shared<boost::asio::steady_timer>(m_io);
    timer->expires_at(expiry);
    return setTimeoutImpl(std::move(timer), std::move(callback));
}

Timer::TimerId Timer::setInterval(std::chrono::milliseconds interval, std::function<void()> callback)
{
    auto timer = std::make_shared<boost::asio::steady_timer>(m_io);
    timer->expires_after(interval);

    auto id = m_next_id.fetch_add(1);
    auto alive = std::make_shared<bool>(true);
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_timers[id] = TimerEntry{timer, alive};
    }

    std::weak_ptr<bool> weak_alive = alive;
    std::function<void(const boost::system::error_code&)> tick;
    tick = [this, id, interval, cb = std::move(callback), weak_alive, &tick]
           (const boost::system::error_code& ec)
    {
        if (ec) return;
        if (auto a = weak_alive.lock())
        {
            cb();

            std::shared_ptr<boost::asio::steady_timer> t;
            {
                std::lock_guard<std::mutex> lock(m_mtx);
                auto it = m_timers.find(id);
                if (it == m_timers.end()) return;
                t = it->second.timer;
            }
            t->expires_after(interval);
            t->async_wait(tick);
        }
    };

    timer->async_wait(tick);
    return id;
}

bool Timer::cancel(TimerId id)
{
    std::shared_ptr<boost::asio::steady_timer> timer;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_timers.find(id);
        if (it == m_timers.end()) return false;
        timer = std::move(it->second.timer);
        m_timers.erase(it);
    }
    timer->cancel();
    return true;
}

bool Timer::exists(TimerId id) const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_timers.find(id) != m_timers.end();
}
