#include "netconn.hpp"
#include "logger.hpp"
#include "toolbox.hpp"

#include <iostream>
#include <limits>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
namespace ssl   = boost::asio::ssl;
using tcp = asio::ip::tcp;

static std::string buildTarget(const ParsedUrl& pu)
{
    return pu.path + (pu.query.empty() ? "" : "?" + pu.query);
}

// ============================================================
//  HttpClient
// ============================================================

struct AsyncHttpSession : std::enable_shared_from_this<AsyncHttpSession>
{
    asio::io_context& io;
    beast::tcp_stream stream;
    asio::steady_timer timer;

    std::string host, port, path, method, body, content_type;
    HttpDelegate delegate;
    Logger* logger = nullptr;
    std::chrono::milliseconds timeout;
    std::atomic<bool> stopped{false};

    beast::flat_buffer buf;
    http::response<http::dynamic_body> res;

    AsyncHttpSession(asio::io_context& io_)
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
            logger->info() << "HttpClient: " << method << " " << host << path;

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
        if (delegate.on_connected) delegate.on_connected();
        auto body_str = beast::buffers_to_string(res.body().data());
        if (!body_str.empty() && delegate.on_data)
            delegate.on_data(body_str, true);
    }

    void fail(const std::string& msg)
    {
        if (stopped.exchange(true)) return;
        timer.cancel();
        if (logger) logger->error() << "HttpClient: " << msg;
        if (delegate.on_error) delegate.on_error(msg);
        stream.close();
    }
};

HttpClient::HttpClient(ThreadPool& pool)
    : m_pool(pool), m_io(pool.io_context())
{
}

HttpClient::~HttpClient()
{
    m_state.store(ConnState::CLOSED);
}

void HttpClient::doHttp(const std::string& url, const std::string& method,
                         const std::string& body, const std::string& content_type,
                         HttpDelegate d)
{
    auto pu = parseUrl(url);
    if (pu.host.empty())
    {
        if (d.on_error) d.on_error("invalid URL: " + url);
        return;
    }

    auto expected = ConnState::CLOSED;
    if (!m_state.compare_exchange_strong(expected, ConnState::CONNECTING))
    {
        if (d.on_error) d.on_error("already in use");
        return;
    }

    try
    {
        tcp::resolver resolver(m_io);
        beast::tcp_stream stream(m_io);
        stream.expires_after(m_timeout);

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
            m_logger->info() << "HttpClient: " << method << " " << url;

        beast::flat_buffer buf;
        http::response<http::dynamic_body> res;
        stream.expires_after(m_timeout);
        http::read(stream, buf, res);

        if (d.on_connected) d.on_connected();

        auto body_str = beast::buffers_to_string(res.body().data());
        if (!body_str.empty() && d.on_data)
            d.on_data(body_str, true);

        m_state.store(ConnState::DISCONNECTED);
    }
    catch (const boost::system::system_error& e)
    {
        m_state.store(ConnState::CLOSED);
        if (d.on_error) d.on_error(e.what());
    }
}

void HttpClient::get(const std::string& url, HttpDelegate d)
{
    m_pool.submit([this, url, d = std::move(d)]() mutable {
        doHttp(url, "GET", "", "", std::move(d));
    });
}

void HttpClient::post(const std::string& url, const std::string& body,
                       const std::string& content_type, HttpDelegate d)
{
    m_pool.submit([this, url, body, content_type, d = std::move(d)]() mutable {
        doHttp(url, "POST", body, content_type, std::move(d));
    });
}

static std::shared_ptr<AsyncHttpSession> makeAsyncSession(
    asio::io_context& io, const ParsedUrl& pu, const std::string& method,
    HttpDelegate d, Logger* logger, std::chrono::milliseconds timeout)
{
    auto session = std::make_shared<AsyncHttpSession>(io);
    session->host     = pu.host;
    session->port     = std::to_string(pu.port);
    session->path     = buildTarget(pu);
    session->method   = method;
    session->delegate = std::move(d);
    session->logger   = logger;
    session->timeout  = timeout;
    return session;
}

void HttpClient::getAsync(const std::string& url, HttpDelegate d)
{
    auto pu = parseUrl(url);
    if (pu.host.empty()) { if (d.on_error) d.on_error("invalid URL: " + url); return; }
    makeAsyncSession(m_io, pu, "GET", std::move(d), m_logger, m_timeout)->start();
}

void HttpClient::postAsync(const std::string& url, const std::string& body,
                            const std::string& content_type, HttpDelegate d)
{
    auto pu = parseUrl(url);
    if (pu.host.empty()) { if (d.on_error) d.on_error("invalid URL: " + url); return; }
    auto session = makeAsyncSession(m_io, pu, "POST", std::move(d), m_logger, m_timeout);
    session->body = body;
    session->content_type = content_type;
    session->start();
}

// ---- HTTPS streaming POST ----

std::string HttpClient::postStream(
    const std::string& host, const std::string& port,
    const std::string& target, const std::string& body,
    const std::string& content_type, const std::string& authorization,
    std::function<void(const std::string&)> on_chunk,
    std::chrono::milliseconds timeout)
{
    asio::io_context io;
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_peer);
    ctx.set_default_verify_paths();

    tcp::resolver resolver(io);
    auto results = resolver.resolve(host, port);

    beast::ssl_stream<beast::tcp_stream> stream(io, ctx);
    stream.next_layer().expires_after(timeout);
    beast::get_lowest_layer(stream).connect(results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::string_body> req(http::verb::post, target, 11);
    req.set(http::field::host, host);
    req.set(http::field::content_type, content_type);
    req.set(http::field::connection, "close");
    if (!authorization.empty())
        req.set(http::field::authorization, authorization);
    req.body() = body;
    req.prepare_payload();
    http::write(stream, req);

    beast::flat_buffer buf;
    http::response_parser<http::string_body> parser;
    parser.body_limit(std::numeric_limits<uint64_t>::max());
    http::read_header(stream, buf, parser);

    int status = parser.get().result_int();
    if (status != 200)
    {
        beast::error_code ec;
        http::read(stream, buf, parser, ec);
        std::string err_body = parser.get().body();
        throw std::runtime_error("HTTPS error " + std::to_string(status) + ": " + err_body);
    }

    std::string full;
    while (!parser.is_done())
    {
        auto prev = parser.get().body().size();
        beast::error_code ec;
        http::read_some(stream, buf, parser, ec);
        if (ec == http::error::end_of_stream) break;
        if (ec) throw beast::system_error(ec);
        std::string chunk = parser.get().body().substr(prev);
        if (!chunk.empty())
        {
            if (on_chunk) on_chunk(chunk);
            full += chunk;
        }
    }
    return full;
}

// ============================================================
//  WsClient
// ============================================================

using WsStream = websocket::stream<beast::tcp_stream>;

struct WsClient::Impl : std::enable_shared_from_this<Impl>
{
    asio::io_context& io;
    asio::strand<asio::io_context::executor_type> strand;
    std::unique_ptr<WsStream> ws;
    std::unique_ptr<beast::flat_buffer> buffer;
    asio::steady_timer reconnect_timer;

    std::string host, port, target, url;
    WsDelegate delegate;
    Logger* logger = nullptr;
    std::chrono::milliseconds timeout{5000};
    std::atomic<ConnState>* state_out = nullptr;
    std::atomic<bool> stopped{false};

    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    int reconnect_attempts = 0;

    Impl(asio::io_context& io_)
        : io(io_), strand(asio::make_strand(io_)), reconnect_timer(io_) {}

    void start()
    {
        auto self = shared_from_this();
        asio::post(strand, [this, self]() { doResolve(); });
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
        stream->expires_after(timeout);
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
                if (state_out) state_out->store(ConnState::CONNECTED);

                if (logger)
                    logger->info() << "WsClient: connected " << url;

                if (delegate.on_connected) delegate.on_connected();

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
                    if (delegate.on_disconnected) delegate.on_disconnected();
                    scheduleReconnect();
                    return;
                }
                if (ec) { fail("read: " + ec.message()); return; }

                auto data = beast::buffers_to_string(buffer->data());
                buffer->consume(buffer->size());

                if (delegate.on_data) delegate.on_data(data, ws->got_text());
                doRead();
            }));
    }

    void scheduleReconnect()
    {
        if (stopped) return;
        if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS)
        {
            if (logger) logger->error() << "WsClient: max reconnect attempts reached";
            if (state_out) state_out->store(ConnState::DISCONNECTED);
            if (delegate.on_disconnected) delegate.on_disconnected();
            stopped = true;
            return;
        }

        int delay_ms = 1000 * (1 << reconnect_attempts);
        if (delay_ms > 30000) delay_ms = 30000;
        ++reconnect_attempts;

        if (logger)
            logger->info() << "WsClient: reconnecting in " << delay_ms << "ms (attempt "
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

    void doSend(const std::string& data)
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

    void doClose()
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
                    if (state_out) state_out->store(ConnState::DISCONNECTED);
                    ws.reset();
                    buffer.reset();
                }));
        });
    }

    void fail(const std::string& msg)
    {
        if (stopped) return;
        if (state_out) state_out->store(ConnState::DISCONNECTED);
        if (logger) logger->error() << "WsClient: " << msg;
        if (delegate.on_error) delegate.on_error(msg);
        stopped = true;
        reconnect_timer.cancel();
        ws.reset();
        buffer.reset();
    }
};

WsClient::WsClient(ThreadPool& pool)
    : m_pool(pool), m_io(pool.io_context())
{
}

WsClient::~WsClient()
{
    m_state.store(ConnState::CLOSED);
    close();
}

void WsClient::connect(const std::string& url, WsDelegate d)
{
    auto pu = parseUrl(url);
    if (pu.host.empty()) { if (d.on_error) d.on_error("invalid URL: " + url); return; }

    auto expected = ConnState::CLOSED;
    if (!m_state.compare_exchange_strong(expected, ConnState::CONNECTING))
    {
        if (d.on_error) d.on_error("already in use");
        return;
    }

    auto impl = std::make_shared<Impl>(m_io);
    impl->host      = pu.host;
    impl->port      = std::to_string(pu.port);
    impl->target    = buildTarget(pu);
    impl->url       = url;
    impl->delegate  = std::move(d);
    impl->logger    = m_logger;
    impl->timeout   = m_timeout;
    impl->state_out = &m_state;

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_impl = impl;
    }

    impl->start();
}

void WsClient::send(const std::string& data)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_impl) m_impl->doSend(data);
}

void WsClient::close()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_impl)
    {
        m_impl->doClose();
        m_impl.reset();
    }
}
