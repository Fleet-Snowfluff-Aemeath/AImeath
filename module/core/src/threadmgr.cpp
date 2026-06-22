#include "threadmgr.hpp"
#include <iostream>

ThreadPool::ThreadPool(size_t thread_num)
    : m_work(boost::asio::make_work_guard(m_io))
{
    m_workers.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i)
        m_workers.emplace_back([this]() { m_io.run(); });
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::submit(std::function<void()> func)
{
    m_pending.fetch_add(1);
    boost::asio::post(m_io, [this, f = std::move(func)]() {
        m_active.fetch_add(1);
        try { f(); } catch (...) {
            std::cerr << "ThreadPool: task exception caught" << std::endl;
        }
        m_active.fetch_sub(1);
        if (m_pending.fetch_sub(1) == 1)
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_waiting) m_done.notify_one();
        }
    });
}

void ThreadPool::wait_all()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_waiting = true;
    m_done.wait(lock, [this]() { return m_active.load() == 0 && m_pending.load() == 0; });
    m_waiting = false;
}

size_t ThreadPool::active_count() const
{
    return m_active.load();
}

void ThreadPool::shutdown()
{
    m_work.reset();
    for (auto& t : m_workers)
        if (t.joinable()) t.join();
}

size_t ThreadPool::get_task_count() const
{
    return m_pending.load();
}
