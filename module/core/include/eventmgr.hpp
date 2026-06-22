#pragma once

#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <iostream>
#include <functional>
#include <any>
#include "threadmgr.hpp"

using EventType = uint64_t;

constexpr EventType EVT_SYS_STARTUP  = 1;
constexpr EventType EVT_SYS_SHUTDOWN = 2;

struct Event
{
    EventType type;
    std::any data;
};

struct ExceptionIsolatingCombiner
{
    using result_type = void;

    template<typename InputIterator>
    void operator()(InputIterator first, InputIterator last) const
    {
        while (first != last)
        {
            try { *first; }
            catch (...)
            {
                std::cerr << "EventManager: subscriber callback exception caught" << std::endl;
            }
            ++first;
        }
    }
};

class EventManager : private boost::noncopyable
{
public:
    using Callback = std::function<void(const Event&)>;
    using Handle   = boost::signals2::connection;
    using Signal   = boost::signals2::signal<void(const Event&), ExceptionIsolatingCombiner>;
    using SignalPtr = std::shared_ptr<Signal>;

    Handle subscribe(EventType type, Callback callback, int priority = 0);
    void unsubscribe(Handle handle);
    void fire(const Event& event) const;
    void fireAsync(const Event& event, ThreadPool& pool) const;
    size_t subscriberCount(EventType type);
    void cleanup();

    EventManager() = default;

private:
    mutable std::mutex m_mtx;
    std::unordered_map<EventType, SignalPtr> m_signals;
};
