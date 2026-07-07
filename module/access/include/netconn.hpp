#pragma once

#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/noncopyable.hpp>
#include "threadmgr.hpp"

class Logger;

// ---- 回调委托 ----

struct HttpDelegate {
    std::function<void()>                       on_connected;
    std::function<void(const std::string& data, bool is_text)> on_data;
    std::function<void(const std::string& msg)> on_error;
};

struct WsDelegate : HttpDelegate {
    std::function<void()> on_disconnected;
};

// ---- 状态 ----

enum class ConnState { CLOSED, CONNECTING, CONNECTED, DISCONNECTED };

// ============================================================
//  HttpClient
// ============================================================

class HttpClient : private boost::noncopyable {
public:
    explicit HttpClient(ThreadPool& pool);
    ~HttpClient();

    HttpClient& withLogger(Logger* logger)  { m_logger = logger; return *this; }
    HttpClient& withTimeout(std::chrono::milliseconds t) { m_timeout = t; return *this; }

    void get(const std::string& url, HttpDelegate d);
    void post(const std::string& url, const std::string& body,
              const std::string& content_type, HttpDelegate d);
    void getAsync(const std::string& url, HttpDelegate d);
    void postAsync(const std::string& url, const std::string& body,
                   const std::string& content_type, HttpDelegate d);

    ConnState state() const { return m_state.load(); }

    // 同步 HTTPS 流式 POST。on_chunk 每收到一块数据回调一次。返回完整响应体。
    static std::string postStream(
        const std::string& host, const std::string& port,
        const std::string& target, const std::string& body,
        const std::string& content_type, const std::string& authorization,
        std::function<void(const std::string&)> on_chunk,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

private:
    struct Impl;
    void doHttp(const std::string& url, const std::string& method,
                const std::string& body, const std::string& content_type,
                HttpDelegate d);

    ThreadPool& m_pool;
    Logger* m_logger = nullptr;
    std::chrono::milliseconds m_timeout{5000};
    std::atomic<ConnState> m_state{ConnState::CLOSED};
    boost::asio::io_context& m_io;
};

// ============================================================
//  WsClient
// ============================================================

class WsClient : private boost::noncopyable {
public:
    explicit WsClient(ThreadPool& pool);
    ~WsClient();

    WsClient& withLogger(Logger* logger)  { m_logger = logger; return *this; }
    WsClient& withTimeout(std::chrono::milliseconds t) { m_timeout = t; return *this; }

    void connect(const std::string& url, WsDelegate d);
    void send(const std::string& data);
    void close();

    ConnState state() const { return m_state.load(); }

private:
    struct Impl;

    ThreadPool& m_pool;
    Logger* m_logger = nullptr;
    std::chrono::milliseconds m_timeout{5000};
    std::atomic<ConnState> m_state{ConnState::CLOSED};
    boost::asio::io_context& m_io;
    std::mutex m_mtx;
    std::shared_ptr<Impl> m_impl;
};
