#pragma once

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

class ThreadPool : private boost::noncopyable
{
public:
    explicit ThreadPool(size_t thread_num);
    ~ThreadPool();

    void submit(std::function<void()> func);
    void wait_all();
    size_t active_count() const;
    void shutdown();
    size_t get_task_count() const;

    boost::asio::io_context& io_context() { return m_io; }

private:
    boost::asio::io_context m_io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;
    std::vector<std::thread> m_workers;

    std::atomic<size_t> m_active{0};
    std::atomic<size_t> m_pending{0};

    mutable std::mutex m_mtx;
    std::condition_variable m_done;
    bool m_waiting{false};
};
