#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>

#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>

struct ExceptionIsolator
{
    using result_type = void;

    template<typename InputIterator>
    void operator()(InputIterator first, InputIterator last) const
    {
        while (first != last)
        {
            try { *first; }
            catch (...) {}
            ++first;
        }
    }
};

/**
 * EventBus — 类型化事件总线
 *
 * 每类事件是一个 C++ 类型，编译期保证 fire/subscribe 类型匹配。
 *
 * 使用:
 *   struct MyEvent { int value; };
 *   EventBus bus;
 *
 *   auto sub = bus.subscribe<MyEvent>([](const MyEvent& e) {
 *       std::cout << e.value;
 *   });
 *   bus.fire(MyEvent{42});
 *
 * 异步:
 *   EventBus bus(&threadPool);          // 注入 executor
 *   bus.fireAsync(MyEvent{42});         // 在 executor 上执行
 */

class ThreadPool;

using Executor = std::function<void(std::function<void()>)>;

inline Executor syncExecutor()
{
    return [](auto fn) { fn(); };
}

class Subscription
{
public:
    Subscription() = default;
    explicit Subscription(boost::signals2::connection conn) : m_conn(std::move(conn)) {}
    Subscription(Subscription&&) = default;
    Subscription& operator=(Subscription&&) = default;

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    ~Subscription() { m_conn.disconnect(); }

    void disconnect() { m_conn.disconnect(); }
    bool connected() const { return m_conn.connected(); }

private:
    boost::signals2::connection m_conn;
};

class EventBus : private boost::noncopyable
{
public:
    EventBus() : m_executor(syncExecutor()) {}
    explicit EventBus(Executor exec) : m_executor(std::move(exec)) {}

    template<typename E>
    Subscription subscribe(std::function<void(const E&)> cb, int priority = 0)
    {
        auto& sig = getSignal<E>();
        return Subscription(sig->connect(-priority, std::move(cb)));
    }

    template<typename E>
    void fire(const E& event)
    {
        auto sig = copySignal<E>();
        if (sig) (*sig)(event);
    }

    template<typename E>
    void fireAsync(const E& event)
    {
        auto sig = copySignal<E>();
        if (sig)
            m_executor([sig, event] { (*sig)(event); });
    }

    template<typename E>
    size_t subscriberCount()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_signals.find(typeid(E));
        if (it == m_signals.end()) return 0;
        return std::static_pointer_cast<Signal<E>>(it->second)->num_slots();
    }

private:
    using SignalBase = boost::signals2::signal_base;
    using SignalPtr  = std::shared_ptr<SignalBase>;

    template<typename E>
    using Signal = boost::signals2::signal<void(const E&), ExceptionIsolator>;

    template<typename E>
    Signal<E>& getSignal()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto key = std::type_index(typeid(E));
        auto it = m_signals.find(key);
        if (it == m_signals.end())
        {
            auto sig = std::make_shared<Signal<E>>();
            m_signals[key] = sig;
            return *sig;
        }
        return static_cast<Signal<E>&>(*it->second);
    }

    template<typename E>
    std::shared_ptr<Signal<E>> copySignal()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_signals.find(std::type_index(typeid(E)));
        if (it == m_signals.end()) return nullptr;
        return std::static_pointer_cast<Signal<E>>(it->second);
    }

    Executor m_executor;
    mutable std::mutex m_mtx;
    std::unordered_map<std::type_index, SignalPtr> m_signals;
};

inline Executor threadPoolExecutor(ThreadPool& pool);
