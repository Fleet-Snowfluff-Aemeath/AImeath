#include "eventmgr.hpp"

EventManager::Handle EventManager::subscribe(EventType type, Callback callback, int priority)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto& sig_ptr = m_signals[type];
    if (!sig_ptr)
        sig_ptr = std::make_shared<Signal>();
    return sig_ptr->connect(-priority, std::move(callback));
}

void EventManager::unsubscribe(Handle handle)
{
    handle.disconnect();
}

void EventManager::fire(const Event& event) const
{
    SignalPtr sig;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_signals.find(event.type);
        if (it != m_signals.end())
            sig = it->second;
    }
    if (sig)
        (*sig)(event);
}

void EventManager::fireAsync(const Event& event, ThreadPool& pool) const
{
    SignalPtr sig;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_signals.find(event.type);
        if (it != m_signals.end())
            sig = it->second;
    }
    if (sig)
        pool.submit([sig, event]() { (*sig)(event); });
}

size_t EventManager::subscriberCount(EventType type)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_signals.find(type);
    if (it == m_signals.end()) return 0;
    size_t n = it->second->num_slots();
    if (n == 0)
        m_signals.erase(it);
    return n;
}

void EventManager::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    for (auto it = m_signals.begin(); it != m_signals.end(); )
    {
        if (it->second->num_slots() == 0)
            it = m_signals.erase(it);
        else
            ++it;
    }
}
