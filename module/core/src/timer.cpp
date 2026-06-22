#include "timer.hpp"

Timer::Timer(ThreadPool& pool)
    : m_io(pool.io_context())
{
    m_state->pool = &pool;
}

Timer::~Timer()
{
    auto state = std::move(m_state);
    std::unordered_map<TimerId, TimerEntry> timers;
    {
        std::lock_guard<std::mutex> lock(state->mtx);
        timers = std::move(state->timers);
    }
    for (auto& [_, entry] : timers)
        entry.timer->cancel();
}

Timer::TimerId Timer::addTimer(std::shared_ptr<boost::asio::steady_timer> timer)
{
    auto id = m_next_id.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(m_state->mtx);
        m_state->timers[id] = TimerEntry{std::move(timer), nullptr};
    }
    return id;
}

Timer::TimerId Timer::setTimeoutImpl(std::shared_ptr<boost::asio::steady_timer> timer,
                                      std::function<void()> callback)
{
    auto id = addTimer(timer);
    std::weak_ptr<TimerState> w_state = m_state;
    timer->async_wait([id, cb = std::move(callback), w_state](const boost::system::error_code& ec) mutable {
        if (ec) return;
        auto state = w_state.lock();
        if (!state) return;
        bool should_call = false;
        {
            std::lock_guard<std::mutex> lock(state->mtx);
            should_call = (state->timers.erase(id) > 0);
        }
        if (should_call && state->pool)
            state->pool->submit(std::move(cb));
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
    auto id = m_next_id.fetch_add(1);
    auto timer = std::make_shared<boost::asio::steady_timer>(m_io);
    timer->expires_after(interval);

    auto loop = std::make_shared<std::function<void(const boost::system::error_code&)>>();
    std::weak_ptr<std::function<void(const boost::system::error_code&)>> weak_loop = loop;
    std::weak_ptr<TimerState> w_state = m_state;

    *loop = [id, interval, cb = std::move(callback), weak_loop, w_state](const boost::system::error_code& ec) {
        if (ec) return;
        auto state = w_state.lock();
        if (!state) return;

        if (state->pool)
            state->pool->submit(cb);

        std::shared_ptr<boost::asio::steady_timer> timer;
        {
            std::lock_guard<std::mutex> lock(state->mtx);
            auto it = state->timers.find(id);
            if (it == state->timers.end()) return;
            timer = it->second.timer;
        }
        timer->expires_after(interval);
        if (auto locked = weak_loop.lock())
            timer->async_wait(*locked);
    };

    {
        std::lock_guard<std::mutex> lock(m_state->mtx);
        m_state->timers[id] = TimerEntry{timer, loop};
    }
    timer->async_wait(*loop);
    return id;
}

bool Timer::cancel(TimerId id)
{
    std::shared_ptr<boost::asio::steady_timer> timer;
    {
        std::lock_guard<std::mutex> lock(m_state->mtx);
        auto it = m_state->timers.find(id);
        if (it == m_state->timers.end()) return false;
        timer = std::move(it->second.timer);
        m_state->timers.erase(it);
    }
    timer->cancel();
    return true;
}

bool Timer::exists(TimerId id) const
{
    std::lock_guard<std::mutex> lock(m_state->mtx);
    return m_state->timers.find(id) != m_state->timers.end();
}
