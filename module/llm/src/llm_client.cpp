#include "llm_client.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

#include <limits>
#include <sstream>
#include <atomic>
#include <memory>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ssl   = asio::ssl;
using tcp = asio::ip::tcp;

static std::atomic<bool> s_ssl_verify{true};

void LlmClient::enable_ssl_verify(bool enabled)
{
    s_ssl_verify = enabled;
}

static ssl::context& ssl_ctx()
{
    static ssl::context ctx(ssl::context::tlsv12_client);
    if (s_ssl_verify) {
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.set_default_verify_paths();
    } else {
        ctx.set_verify_mode(ssl::verify_none);
    }
    return ctx;
}

// ---- LlmClient (streaming) ----

struct LlmClient::Impl : public std::enable_shared_from_this<LlmClient::Impl>
{
    asio::io_context& io;
    tcp::resolver resolver_;
    std::unique_ptr<ssl::stream<beast::tcp_stream>> stream_;
    asio::steady_timer timer_;
    beast::flat_buffer buf_;
    http::response_parser<http::string_body> parser_;
    http::request<http::string_body> req_;

    std::string host_, port_, body_, auth_, target_;
    std::string full_response_, full_reasoning_;
    std::string leftover_;
    size_t prev_body_size_ = 0;
    int prompt_tokens_ = 0;
    int completion_tokens_ = 0;
    int retry_count_ = 0;
    static constexpr int max_retries_ = 1;
    std::chrono::seconds connect_timeout_{30};

    LlmEventFn on_event_;
    LlmDoneFn on_done_;
    std::atomic<bool> cancelled_{false};

    Impl(asio::io_context& io_)
        : io(io_), resolver_(io_)
        , stream_(std::make_unique<ssl::stream<beast::tcp_stream>>(io_, ssl_ctx()))
        , timer_(io_)
    {}

    void start(const std::string& host, const std::string& port,
               const std::string& body, const std::string& auth,
               LlmEventFn on_event, LlmDoneFn on_done,
               std::chrono::seconds timeout,
               const std::string& target,
               std::chrono::seconds connect_timeout)
    {
        on_event_ = std::move(on_event);
        on_done_ = std::move(on_done);
        body_ = body;
        auth_ = auth;
        host_ = host;
        port_ = port;
        target_ = target;
        connect_timeout_ = connect_timeout;
        retry_count_ = 0;

        auto self = shared_from_this();
        auto timeout_sec = timeout.count();
        timer_.expires_after(timeout);
        timer_.async_wait([self, timeout_sec](beast::error_code ec) {
            if (ec == asio::error::operation_aborted || self->cancelled_) return;
            self->finish_error("request timeout after " +
                std::to_string(timeout_sec) + "s");
        });

        do_resolve();
    }

    void cancel()
    {
        cancelled_ = true;
        timer_.cancel();
        resolver_.cancel();
        try {
            if (stream_) stream_->lowest_layer().cancel();
        } catch (...) {}
    }

private:
    void reset_stream()
    {
        stream_ = std::make_unique<ssl::stream<beast::tcp_stream>>(io, ssl_ctx());
        buf_ = beast::flat_buffer();
        using parser_t = http::response_parser<http::string_body>;
        parser_.~parser_t();
        new (&parser_) parser_t();
        leftover_.clear();
        prev_body_size_ = 0;
    }

    void do_resolve()
    {
        auto self = shared_from_this();
        resolver_.async_resolve(host_, port_,
            [self](beast::error_code ec, tcp::resolver::results_type results) {
                if (self->cancelled_) return;
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_connect(results);
            });
    }

    void do_connect(tcp::resolver::results_type results)
    {
        auto self = shared_from_this();
        beast::get_lowest_layer(*self->stream_).expires_after(connect_timeout_);
        beast::get_lowest_layer(*self->stream_).async_connect(results,
            [self](beast::error_code ec, auto) {
                if (self->cancelled_) return;
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_handshake();
            });
    }

    void do_handshake()
    {
        auto self = shared_from_this();
        self->stream_->async_handshake(ssl::stream_base::client,
            [self](beast::error_code ec) {
                if (self->cancelled_) return;
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_write_request();
            });
    }

    void do_write_request()
    {
        auto self = shared_from_this();
        http::request<http::string_body> req;
        req.method(http::verb::post);
        req.target(self->target_);
        req.version(11);
        req.set(http::field::host, self->host_);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::connection, "close");
        if (!self->auth_.empty()) req.set(http::field::authorization, self->auth_);
        req.body() = self->body_;
        req.prepare_payload();
        self->req_ = std::move(req);

        http::async_write(*self->stream_, self->req_,
            [self](beast::error_code ec, std::size_t) {
                if (self->cancelled_) return;
                if (ec) { self->finish_error(ec.message()); return; }
                self->do_read_header();
            });
    }

    void do_read_header()
    {
        auto self = shared_from_this();
        http::async_read_header(*self->stream_, self->buf_, self->parser_,
            [self](beast::error_code ec, std::size_t) {
                if (self->cancelled_) return;
                if (ec) { self->finish_error(ec.message()); return; }
                int status = self->parser_.get().result_int();
                if (status != 200) {
                    http::async_read(*self->stream_, self->buf_, self->parser_,
                        [self, status](beast::error_code ec2, std::size_t) {
                            if (self->cancelled_) return;
                            std::string resp_body;
                            if (!ec2) resp_body = self->parser_.get().body();
                            std::string msg = "HTTP " + std::to_string(
                                self->parser_.get().result_int());
                            if (!resp_body.empty()) msg += ": " + resp_body;

                            if ((status == 429 || (status >= 500 && status < 600)) &&
                                self->retry_count_ < max_retries_) {
                                ++self->retry_count_;
                                self->reset_stream();
                                auto self2 = self;
                                self->timer_.expires_after(std::chrono::seconds(1));
                                self->timer_.async_wait([self2](beast::error_code ec3) {
                                    if (ec3 || self2->cancelled_) return;
                                    self2->do_resolve();
                                });
                                return;
                            }
                            self->finish_error(msg);
                        });
                    return;
                }
                self->parser_.body_limit(std::numeric_limits<std::uint64_t>::max());
                self->prev_body_size_ = 0;
                self->prompt_tokens_ = 0;
                self->completion_tokens_ = 0;
                self->do_read_body();
            });
    }

    void do_read_body()
    {
        auto self = shared_from_this();
        http::async_read_some(*self->stream_, self->buf_, self->parser_,
            [self](beast::error_code ec, std::size_t) {
                if (self->cancelled_) return;
                if (ec == http::error::end_of_stream) {
                    self->drain_remaining();
                    self->finish_success();
                    return;
                }
                if (ec) { self->finish_error(ec.message()); return; }
                self->drain_remaining();
                if (self->parser_.is_done()) {
                    self->finish_success();
                } else {
                    self->do_read_body();
                }
            });
    }

    void drain_remaining()
    {
        auto& body = parser_.get().body();
        size_t prev = prev_body_size_;
        prev_body_size_ = body.size();
        if (prev < body.size()) {
            std::string chunk = body.substr(prev);
            process_sse(chunk);
        }
    }

    void process_sse(const std::string& chunk)
    {
        leftover_ += chunk;
        size_t pos;
        while ((pos = leftover_.find("\n\n")) != std::string::npos) {
            std::string event = leftover_.substr(0, pos);
            leftover_.erase(0, pos + 2);
            std::istringstream ss(event);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                if (line[0] == ':') continue;
                if (line.rfind("data: ", 0) != 0) continue;
                std::string data = line.substr(6);
                if (data == "[DONE]") continue;
                try {
                    auto val = boost::json::parse(data);
                    auto& obj = val.as_object();

                    if (obj.contains("usage") && obj.at("usage").is_object()) {
                        auto& usage = obj.at("usage").as_object();
                        if (usage.contains("prompt_tokens") && usage.at("prompt_tokens").is_int64())
                            prompt_tokens_ = static_cast<int>(usage.at("prompt_tokens").as_int64());
                        if (usage.contains("completion_tokens") && usage.at("completion_tokens").is_int64())
                            completion_tokens_ = static_cast<int>(usage.at("completion_tokens").as_int64());

                        LlmEvent ev;
                        ev.type = "usage";
                        auto& u = ev.usage;
                        if (usage.contains("prompt_tokens") && usage.at("prompt_tokens").is_int64())
                            u.prompt_tokens = static_cast<int>(usage.at("prompt_tokens").as_int64());
                        if (usage.contains("completion_tokens") && usage.at("completion_tokens").is_int64())
                            u.completion_tokens = static_cast<int>(usage.at("completion_tokens").as_int64());
                        if (usage.contains("total_tokens") && usage.at("total_tokens").is_int64())
                            u.total_tokens = static_cast<int>(usage.at("total_tokens").as_int64());
                        if (usage.contains("prompt_cache_hit_tokens") && usage.at("prompt_cache_hit_tokens").is_int64())
                            u.prompt_cache_hit_tokens = static_cast<int>(usage.at("prompt_cache_hit_tokens").as_int64());
                        if (usage.contains("prompt_cache_miss_tokens") && usage.at("prompt_cache_miss_tokens").is_int64())
                            u.prompt_cache_miss_tokens = static_cast<int>(usage.at("prompt_cache_miss_tokens").as_int64());
                        if (usage.contains("completion_tokens_details") && usage.at("completion_tokens_details").is_object()) {
                            auto& details = usage.at("completion_tokens_details").as_object();
                            if (details.contains("reasoning_tokens") && details.at("reasoning_tokens").is_int64())
                                u.reasoning_tokens = static_cast<int>(details.at("reasoning_tokens").as_int64());
                        }
                        if (on_event_) on_event_(std::move(ev));
                    }

                    auto& choices = obj.at("choices").as_array();
                    if (choices.empty()) continue;

                    auto& choice = choices[0].as_object();

                    if (choice.contains("finish_reason") && choice.at("finish_reason").is_string()) {
                        std::string fr = choice.at("finish_reason").as_string().c_str();
                        if (!fr.empty()) {
                            LlmEvent ev;
                            ev.type = "finish";
                            ev.finish_reason = fr;
                            if (on_event_) on_event_(std::move(ev));
                        }
                    }

                    if (choice.contains("delta") && choice.at("delta").is_object()) {
                        auto& delta = choice.at("delta").as_object();

                        if (delta.contains("reasoning_content") &&
                            delta.at("reasoning_content").is_string()) {
                            std::string r = delta.at("reasoning_content").as_string().c_str();
                            if (!r.empty()) {
                                full_reasoning_ += r;
                                LlmEvent ev;
                                ev.type = "reasoning";
                                ev.text = r;
                                if (on_event_) on_event_(std::move(ev));
                            }
                        }
                        if (delta.contains("content") &&
                            delta.at("content").is_string()) {
                            std::string c = delta.at("content").as_string().c_str();
                            if (!c.empty()) {
                                full_response_ += c;
                                LlmEvent ev;
                                ev.type = "delta";
                                ev.text = std::move(c);
                                if (on_event_) on_event_(std::move(ev));
                            }
                        }
                        if (delta.contains("tool_calls") &&
                            delta.at("tool_calls").is_array()) {
                            auto& tc_arr = delta.at("tool_calls").as_array();
                            for (auto& tc_val : tc_arr) {
                                if (!tc_val.is_object()) continue;
                                auto& tc = tc_val.as_object();
                                LlmToolCall tc_ev;
                                if (tc.contains("id") && tc.at("id").is_string())
                                    tc_ev.id = tc.at("id").as_string().c_str();
                                if (tc.contains("type") && tc.at("type").is_string())
                                    tc_ev.type = tc.at("type").as_string().c_str();
                                if (tc.contains("function") && tc.at("function").is_object()) {
                                    auto& fn = tc.at("function").as_object();
                                    if (fn.contains("name") && fn.at("name").is_string())
                                        tc_ev.function_name = fn.at("name").as_string().c_str();
                                    if (fn.contains("arguments") && fn.at("arguments").is_string())
                                        tc_ev.function_arguments = fn.at("arguments").as_string().c_str();
                                }
                                LlmEvent ev;
                                ev.type = "tool_call";
                                ev.tool_calls.push_back(std::move(tc_ev));
                                if (on_event_) on_event_(std::move(ev));
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LlmEvent ev;
                    ev.type = "error";
                    ev.msg = std::string("sse parse: ") + e.what();
                    if (on_event_) on_event_(std::move(ev));
                }
            }
        }
    }

    void stop_timer()
    {
        timer_.cancel();
    }

    void finish_success()
    {
        stop_timer();
        if (cancelled_) return;
        LlmEvent ev;
        ev.type = "stream_end";
        if (on_event_) on_event_(std::move(ev));
        if (on_done_) on_done_(std::move(full_response_), std::move(full_reasoning_),
                               prompt_tokens_, completion_tokens_);
    }

    void finish_error(const std::string& msg)
    {
        stop_timer();
        if (cancelled_) return;
        LlmEvent ev;
        ev.type = "stream_end";
        if (!msg.empty()) ev.msg = msg;
        if (on_event_) on_event_(std::move(ev));
        if (on_done_) on_done_(std::move(full_response_), std::move(full_reasoning_),
                               prompt_tokens_, completion_tokens_);
    }
};

// ---- LlmClient public API ----

LlmClient::LlmClient(boost::asio::io_context& io)
    : impl_(std::make_shared<Impl>(io))
{}

LlmClient::~LlmClient()
{
    impl_->cancel();
}

void LlmClient::start(const std::string& host, const std::string& port,
                      const std::string& body, const std::string& auth,
                      LlmEventFn on_event, LlmDoneFn on_done,
                      std::chrono::seconds timeout,
                      const std::string& target,
                      std::chrono::seconds connect_timeout)
{
    impl_->start(host, port, body, auth, std::move(on_event), std::move(on_done),
                 timeout, target, connect_timeout);
}

void LlmClient::cancel()
{
    impl_->cancel();
}

// ---- LlmRequest (synchronous) ----

static LlmResponse do_request(
    const std::string& host, const std::string& port,
    const std::string& target, const std::string& method,
    const std::string& body, const std::string& auth,
    std::chrono::seconds timeout,
    std::chrono::seconds connect_timeout)
{
    LlmResponse resp;
    try {
        asio::io_context io;
        asio::ip::tcp::resolver resolver(io);
        ssl::stream<beast::tcp_stream> stream(io, ssl_ctx());

        beast::get_lowest_layer(stream).expires_after(connect_timeout);
        auto results = resolver.resolve(host, port);
        beast::get_lowest_layer(stream).connect(results);

        beast::get_lowest_layer(stream).expires_after(timeout);
        stream.handshake(ssl::stream_base::client);

        http::request<http::string_body> req;
        if (method == "GET")
            req.method(http::verb::get);
        else
            req.method(http::verb::post);
        req.target(target);
        req.version(11);
        req.set(http::field::host, host);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::connection, "close");
        if (!auth.empty()) req.set(http::field::authorization, auth);
        if (!body.empty() && method != "GET") {
            req.body() = body;
            req.prepare_payload();
        }

        http::write(stream, req);

        beast::flat_buffer buf;
        http::response<http::string_body> res;
        http::read(stream, buf, res);

        resp.status = res.result_int();
        resp.body = res.body();

        beast::error_code ec;
        stream.shutdown(ec);
    } catch (const std::exception& e) {
        resp.error = e.what();
    }
    return resp;
}

LlmResponse LlmRequest::post(
    const std::string& host, const std::string& port,
    const std::string& target, const std::string& body,
    const std::string& auth,
    std::chrono::seconds timeout,
    std::chrono::seconds connect_timeout)
{
    return do_request(host, port, target, "POST", body, auth, timeout, connect_timeout);
}

LlmResponse LlmRequest::get(
    const std::string& host, const std::string& port,
    const std::string& target, const std::string& auth,
    std::chrono::seconds timeout,
    std::chrono::seconds connect_timeout)
{
    return do_request(host, port, target, "GET", "", auth, timeout, connect_timeout);
}
