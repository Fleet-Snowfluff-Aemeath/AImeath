#pragma once

#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "threadmgr.hpp"
#include "wsutil.hpp"
#include <boost/noncopyable.hpp>

class Logger;

class NetConn : private boost::noncopyable
{
public:
    enum class State { CLOSED, CONNECTING, CONNECTED, DISCONNECTED };

    using OnConnected    = std::function<void()>;
    using OnMessage      = std::function<void(const std::string& data, bool is_text)>;
    using OnError        = std::function<void(const std::string& msg)>;
    using OnDisconnected = std::function<void()>;

    explicit NetConn(ThreadPool& pool, Logger* logger = nullptr);
    ~NetConn();

    void httpGet(const std::string& url,
                 OnConnected on_connected,
                 OnMessage on_message,
                 OnError on_error);

    void httpPost(const std::string& url,
                  const std::string& body,
                  const std::string& content_type,
                  OnConnected on_connected,
                  OnMessage on_message,
                  OnError on_error);

    void httpGetAsync(const std::string& url,
                      OnConnected on_connected,
                      OnMessage on_message,
                      OnError on_error);

    void httpPostAsync(const std::string& url,
                       const std::string& body,
                       const std::string& content_type,
                       OnConnected on_connected,
                       OnMessage on_message,
                       OnError on_error);

    void wsConnect(const std::string& url,
                   OnConnected on_connected,
                   OnMessage on_message,
                   OnError on_error,
                   OnDisconnected on_disconnected);

    void wsSend(const std::string& data);
    void wsClose();

    State state() const { return m_state.load(); }
    void setConnectTimeout(std::chrono::milliseconds ms) { m_connect_timeout = ms; }

private:
    struct WsState;
    struct HttpSession;

    void doHttp(const std::string& url,
                const std::string& method,
                const std::string& body,
                const std::string& content_type,
                OnConnected on_connected,
                OnMessage on_message,
                OnError on_error);

    std::shared_ptr<HttpSession> initHttpSession(const ParsedUrl& pu,
                                                  const std::string& method,
                                                  OnConnected on_connected,
                                                  OnMessage on_message,
                                                  OnError on_error);

    ThreadPool& m_pool;
    Logger* m_logger;
    std::atomic<State> m_state{State::CLOSED};
    std::chrono::milliseconds m_connect_timeout{5000};

    boost::asio::io_context& m_io;

    mutable std::mutex m_ws_mtx;
    std::shared_ptr<WsState> m_ws;
};
