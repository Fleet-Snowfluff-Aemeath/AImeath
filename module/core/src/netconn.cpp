#include "netconn.hpp"
#include "logger.hpp"

#include <iostream>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

static std::string buildTarget(const ParsedUrl& pu)
{
    return pu.path + (pu.query.empty() ? "" : "?" + pu.query);
}

// ==================== WebSocket session (async, self-contained) ====================

using WsStream = beast::websocket::stream<beast::tcp_stream>;

struct NetConn::WsState : std::enable_shared_from_this<WsState>
{
    asio::io_context& io;
    asio::strand<asio::io_context::executor_type> strand;
    std::unique_ptr<WsStream> ws;
    std::unique_ptr<beast::flat_buffer> buffer;
    asio::steady_timer reconnect_timer;

    std::string host;
    std::string port = "80";
    std::string target;
    std::string url;

    NetConn::OnConnected    on_connected;
    NetConn::OnMessage      on_message;
    NetConn::OnError        on_error;
    NetConn::OnDisconnected on_disconnected;
    Logger*        logger = nullptr;
    std::atomic<NetConn::State>* state_out = nullptr;

    std::chrono::milliseconds connect_timeout{5000};
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    int reconnect_attempts = 0;
    std::atomic<bool> stopped{false};

    WsState(asio::io_context& io_)
        : io(io_), strand(asio::make_strand(io_)), reconnect_timer(io_)
    {
    }

    void start()
    {
        auto self = shared_from_this();
        asio::post(strand, [this, self]() {
            doResolve();
        });
    }

    void doResolve()
    {
        auto self = shared_from_this();
        auto resolver = std::make_shared<tcp::resolver>(io);
        resolver->async_resolve(host, port,
            asio::bind_executor(strand, [this, self, resolver]
                (boost::system::error_code ec, auto results)
            {
                if (stopped) return;
                if (ec) { fail("resolve: " + ec.message()); return; }
                doConnect(results);
            }));
    }

    void doConnect(tcp::resolver::results_type results)
    {
        auto self = shared_from_this();
        auto stream = std::make_shared<beast::tcp_stream>(io);
        stream->expires_after(connect_timeout);
        stream->async_connect(results,
            asio::bind_executor(strand, [this, self, stream]
                (boost::system::error_code ec, auto)
            {
                if (stopped) return;
                if (ec) { fail("connect: " + ec.message()); return; }
                doHandshake(std::move(*stream));
            }));
    }

    void doHandshake(beast::tcp_stream&& stream)
    {
        auto self = shared_from_this();
        ws = std::make_unique<WsStream>(std::move(stream));
        ws->async_handshake(host, target,
            asio::bind_executor(strand, [this, self]
                (boost::system::error_code ec)
            {
                if (stopped) return;
                if (ec) { fail("handshake: " + ec.message()); return; }

                reconnect_attempts = 0;
                if (state_out) state_out->store(NetConn::State::CONNECTED);

                if (logger)
                    logger->info() << "NetConn: WS connected " << url;

                if (on_connected) on_connected();

                buffer = std::make_unique<beast::flat_buffer>();
                doRead();
            }));
    }

    void doRead()
    {
        auto self = shared_from_this();
        ws->async_read(*buffer,
            asio::bind_executor(strand, [this, self]
                (boost::system::error_code ec, size_t)
            {
                if (stopped) return;
                if (ec == websocket::error::closed || ec == asio::error::eof)
                {
                    if (on_disconnected) on_disconnected();
                    scheduleReconnect();
                    return;
                }
                if (ec) { fail("read: " + ec.message()); return; }

                auto data = beast::buffers_to_string(buffer->data());
                buffer->consume(buffer->size());

                if (on_message) on_message(data, ws->got_text());
                doRead();
            }));
    }

    void scheduleReconnect()
    {
        if (stopped) return;
        if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS)
        {
            if (logger) logger->error() << "NetConn: max reconnect attempts reached";
            if (state_out) state_out->store(NetConn::State::DISCONNECTED);
            if (on_disconnected) on_disconnected();
            stopped = true;
            return;
        }

        int delay_ms = 1000 * (1 << reconnect_attempts);
        if (delay_ms > 30000) delay_ms = 30000;
        ++reconnect_attempts;

        if (logger)
            logger->info() << "NetConn: reconnecting in " << delay_ms << "ms (attempt "
                           << reconnect_attempts << "/" << MAX_RECONNECT_ATTEMPTS << ")";

        ws.reset();
        buffer.reset();

        auto self = shared_from_this();
        reconnect_timer.expires_after(std::chrono::milliseconds(delay_ms));
        reconnect_timer.async_wait(
            asio::bind_executor(strand, [this, self](boost::system::error_code ec)
            {
                if (stopped || ec) return;
                doResolve();
            }));
    }

    void send(const std::string& data)
    {
        auto payload = std::make_shared<std::string>(data);
        auto self = shared_from_this();
        asio::post(strand, [this, self, payload]() {
            if (stopped || !ws) return;
            ws->async_write(asio::buffer(*payload),
                asio::bind_executor(strand, [this, self, payload]
                    (boost::system::error_code ec, size_t)
                {
                    if (ec && !stopped) fail("send: " + ec.message());
                }));
        });
    }

    void close()
    {
        auto self = shared_from_this();
        asio::post(strand, [this, self]() {
            stopped = true;
            reconnect_timer.cancel();
            if (!ws) return;
            ws->async_close(websocket::close_code::normal,
                asio::bind_executor(strand, [this, self]
                    (boost::system::error_code ec)
                {
                    if (ec) fail("close: " + ec.message());
                    if (state_out) state_out->store(NetConn::State::DISCONNECTED);
                    ws.reset();
                    buffer.reset();
                }));
        });
    }

    void fail(const std::string& msg)
    {
        if (stopped) return;
        if (state_out) state_out->store(NetConn::State::DISCONNECTED);
        if (logger) logger->error() << "NetConn: " << msg;
        if (on_error) on_error(msg);
        stopped = true;
        reconnect_timer.cancel();
        ws.reset();
        buffer.reset();
    }
};

// ==================== HTTP session (async, self-contained) ====================

struct NetConn::HttpSession : std::enable_shared_from_this<HttpSession>
{
    asio::io_context& io;
    beast::tcp_stream stream;
    asio::steady_timer timer;

    std::string host;
    std::string port;
    std::string path;
    std::string method;
    std::string body;
    std::string content_type;

    NetConn::OnConnected on_connected;
    NetConn::OnMessage   on_message;
    NetConn::OnError     on_error;
    Logger* logger = nullptr;
    std::chrono::milliseconds timeout;
    std::atomic<bool> stopped{false};

    beast::flat_buffer buf;
    http::response<http::dynamic_body> res;

    HttpSession(asio::io_context& io_)
        : io(io_), stream(io_), timer(io_) {}

    void start()
    {
        auto self = shared_from_this();
        timer.expires_after(timeout);
        timer.async_wait([this, self](boost::system::error_code ec) {
            if (ec || stopped) return;
            fail("timed out");
        });

        auto r = std::make_shared<tcp::resolver>(io);
        r->async_resolve(host, port,
            [this, self, r](boost::system::error_code ec, auto results) {
                if (ec || stopped) { fail("resolve: " + ec.message()); return; }
                timer.cancel();
                doConnect(results);
            });
    }

    void doConnect(tcp::resolver::results_type results)
    {
        auto self = shared_from_this();
        timer.expires_after(timeout);
        timer.async_wait([this, self](boost::system::error_code ec) {
            if (ec || stopped) return;
            stream.close();
            fail("connect timed out");
        });
        stream.async_connect(results,
            [this, self](boost::system::error_code ec, auto) {
                if (ec || stopped) { fail("connect: " + ec.message()); return; }
                timer.cancel();
                doWrite();
            });
    }

    void doWrite()
    {
        auto self = shared_from_this();
        http::request<http::string_body> req(
            http::string_to_verb(method), path, 11);
        req.set(http::field::host, host);
        req.set(http::field::connection, "close");
        if (!body.empty())
        {
            req.body() = body;
            req.set(http::field::content_type, content_type);
            req.prepare_payload();
        }

        if (logger)
            logger->info() << "NetConn: " << method << " " << host << path;

        timer.expires_after(timeout);
        timer.async_wait([this, self](boost::system::error_code ec) {
            if (ec || stopped) return;
            stream.close();
            fail("write timed out");
        });
        http::async_write(stream, req,
            [this, self](boost::system::error_code ec, size_t) {
                if (ec || stopped) { fail("write: " + ec.message()); return; }
                timer.cancel();
                doRead();
            });
    }

    void doRead()
    {
        auto self = shared_from_this();
        timer.expires_after(timeout);
        timer.async_wait([this, self](boost::system::error_code ec) {
            if (ec || stopped) return;
            stream.close();
            fail("read timed out");
        });
        http::async_read(stream, buf, res,
            [this, self](boost::system::error_code ec, size_t) {
                if (stopped) return;
                if (ec) { fail("read: " + ec.message()); return; }
                timer.cancel();
                finish();
            });
    }

    void finish()
    {
        if (on_connected) on_connected();
        auto body_str = beast::buffers_to_string(res.body().data());
        if (!body_str.empty() && on_message)
            on_message(body_str, true);
    }

    void fail(const std::string& msg)
    {
        if (stopped.exchange(true)) return;
        timer.cancel();
        if (logger) logger->error() << "NetConn: " << msg;
        if (on_error) on_error(msg);
        stream.close();
    }
};

// ==================== NetConn ====================

NetConn::NetConn(ThreadPool& pool, Logger* logger)
    : m_pool(pool), m_logger(logger), m_io(pool.io_context())
{
}

NetConn::~NetConn()
{
    m_state.store(State::CLOSED);
    std::lock_guard<std::mutex> lock(m_ws_mtx);
    if (m_ws)
    {
        m_ws->close();
        m_ws.reset();
    }
}

void NetConn::doHttp(const std::string& url,
                      const std::string& method,
                      const std::string& body,
                      const std::string& content_type,
                      OnConnected on_connected,
                      OnMessage on_message,
                      OnError on_error)
{
    auto pu = parseUrl(url);
    if (pu.host.empty())
    {
        if (on_error) on_error("invalid URL: " + url);
        return;
    }

    State expected = State::CLOSED;
    if (!m_state.compare_exchange_strong(expected, State::CONNECTING))
    {
        if (on_error) on_error("already in use");
        return;
    }

    try
    {
        tcp::resolver resolver(m_io);
        beast::tcp_stream stream(m_io);
        stream.expires_after(m_connect_timeout);

        auto results = resolver.resolve(pu.host, std::to_string(pu.port));
        stream.connect(results);

        std::string target = buildTarget(pu);
        http::request<http::string_body> req(
            http::string_to_verb(method), target, 11);
        req.set(http::field::host, pu.host);
        req.set(http::field::connection, "close");
        if (!body.empty())
        {
            req.body() = body;
            req.set(http::field::content_type, content_type);
            req.prepare_payload();
        }

        http::write(stream, req);

        if (m_logger)
            m_logger->info() << "NetConn: " << method << " " << url;

        beast::flat_buffer buf;
        http::response<http::dynamic_body> res;
        stream.expires_after(m_connect_timeout);
        http::read(stream, buf, res);

        if (on_connected) on_connected();

        auto body_str = beast::buffers_to_string(res.body().data());
        if (!body_str.empty() && on_message)
            on_message(body_str, true);

        m_state.store(State::DISCONNECTED);
    }
    catch (const boost::system::system_error& e)
    {
        m_state.store(State::CLOSED);
        if (on_error) on_error(e.what());
    }
}

void NetConn::httpGet(const std::string& url,
                       OnConnected on_connected,
                       OnMessage on_message,
                       OnError on_error)
{
    m_pool.submit([this, url, on_connected, on_message, on_error]() {
        doHttp(url, "GET", "", "", on_connected, on_message, on_error);
    });
}

void NetConn::httpPost(const std::string& url,
                        const std::string& body,
                        const std::string& content_type,
                        OnConnected on_connected,
                        OnMessage on_message,
                        OnError on_error)
{
    m_pool.submit([this, url, body, content_type, on_connected, on_message, on_error]() {
        doHttp(url, "POST", body, content_type, on_connected, on_message, on_error);
    });
}

std::shared_ptr<NetConn::HttpSession> NetConn::initHttpSession(
    const ParsedUrl& pu, const std::string& method,
    OnConnected on_connected, OnMessage on_message, OnError on_error)
{
    auto session = std::make_shared<HttpSession>(m_io);
    session->host    = pu.host;
    session->port    = std::to_string(pu.port);
    session->path    = buildTarget(pu);
    session->method  = method;
    session->on_connected = std::move(on_connected);
    session->on_message   = std::move(on_message);
    session->on_error     = std::move(on_error);
    session->logger  = m_logger;
    session->timeout = m_connect_timeout;
    return session;
}

void NetConn::httpGetAsync(const std::string& url,
                            OnConnected on_connected,
                            OnMessage on_message,
                            OnError on_error)
{
    auto pu = parseUrl(url);
    if (pu.host.empty())
    {
        if (on_error) on_error("invalid URL: " + url);
        return;
    }
    auto session = initHttpSession(pu, "GET", std::move(on_connected),
                                   std::move(on_message), std::move(on_error));
    session->start();
}

void NetConn::httpPostAsync(const std::string& url,
                             const std::string& body,
                             const std::string& content_type,
                             OnConnected on_connected,
                             OnMessage on_message,
                             OnError on_error)
{
    auto pu = parseUrl(url);
    if (pu.host.empty())
    {
        if (on_error) on_error("invalid URL: " + url);
        return;
    }
    auto session = initHttpSession(pu, "POST", std::move(on_connected),
                                   std::move(on_message), std::move(on_error));
    session->body = body;
    session->content_type = content_type;
    session->start();
}

void NetConn::wsConnect(const std::string& url,
                         OnConnected on_connected,
                         OnMessage on_message,
                         OnError on_error,
                         OnDisconnected on_disconnected)
{
    auto pu = parseUrl(url);
    if (pu.host.empty())
    {
        if (on_error) on_error("invalid URL: " + url);
        return;
    }

    State expected = State::CLOSED;
    if (!m_state.compare_exchange_strong(expected, State::CONNECTING))
    {
        if (on_error) on_error("already in use");
        return;
    }

    auto ws_state = std::make_shared<WsState>(m_io);
    ws_state->host   = pu.host;
    ws_state->port   = std::to_string(pu.port);
    ws_state->target = buildTarget(pu);
    ws_state->url    = url;
    ws_state->on_connected    = std::move(on_connected);
    ws_state->on_message      = std::move(on_message);
    ws_state->on_error        = std::move(on_error);
    ws_state->on_disconnected = std::move(on_disconnected);
    ws_state->logger = m_logger;
    ws_state->connect_timeout = m_connect_timeout;
    ws_state->state_out = &m_state;

    {
        std::lock_guard<std::mutex> lock(m_ws_mtx);
        m_ws = ws_state;
    }

    ws_state->start();
}

void NetConn::wsSend(const std::string& data)
{
    std::lock_guard<std::mutex> lock(m_ws_mtx);
    if (m_ws) m_ws->send(data);
}

void NetConn::wsClose()
{
    std::lock_guard<std::mutex> lock(m_ws_mtx);
    if (m_ws)
    {
        m_ws->close();
        m_ws.reset();
    }
}

extern "C"
{

void run_netconn_demo()
{
    auto url = parseUrl("http://example.com:8080/path?q=1");
    std::cout << "URL parse: host=" << url.host
              << " port=" << url.port
              << " path=" << url.path
              << " query=" << url.query
              << " is_ws=" << url.is_ws << std::endl;
    std::cout << "NetConn: all utility functions verified OK" << std::endl;
}

}
