#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <utility>

#include <boost/noncopyable.hpp>

template<typename T>
class MessageQueue : private boost::noncopyable
{
public:
    MessageQueue() = default;
    MessageQueue(MessageQueue&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.m_mtx);
        m_queue = std::move(other.m_queue);
        m_shutdown.store(other.m_shutdown.load());
    }
    ~MessageQueue() = default;

    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_shutdown) return;
            m_queue.push_back(std::move(item));
        }
        m_cv.notify_one();
    }

    bool try_pop(T& item)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_queue.empty()) return false;
        item = std::move(m_queue.front());
        m_queue.pop_front();
        return true;
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_shutdown; });
        if (m_shutdown) return T{};
        T item = std::move(m_queue.front());
        m_queue.pop_front();
        return item;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_queue.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_queue.empty();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_queue.clear();
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_shutdown = true;
        }
        m_cv.notify_all();
    }

private:
    std::deque<T> m_queue;
    mutable std::mutex m_mtx;
    std::condition_variable m_cv;
    std::atomic<bool> m_shutdown{false};
};
