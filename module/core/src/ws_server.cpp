#include "ws_server.hpp"

Session::Session(tcp::socket socket, Logger& logger,
                 AppModuleCache& cache, ThreadPool* fallback_pool,
                 asio::io_context* io_ctx, int port)
    : logger_(logger)
    , cache_(cache)
    , fallback_pool_(fallback_pool)
    , io_ctx_(io_ctx)
    , strand_(io_ctx->get_executor())
    , port_(port)
{
    stream_.emplace(std::move(socket));
    logger_.info() << "[sess:" << this << "] new connection";
}

void Session::start()
{
    do_http_read();
}

void Session::on_app_output(const char* json)
{
    if (closing_) return;
    asio::post(strand_,
        [self = shared_from_this(), s = std::string(json)]() {
            if (self->closing_) return;
            self->enqueue(std::move(s));
        });
}

void Session::enqueue(std::string json)
{
    if (closing_) return;
    write_queue_.push_back(std::move(json));
    if (!writing_) do_write();
}

void Session::do_write()
{
    if (write_queue_.empty()) { writing_ = false; return; }
    if (closing_) { writing_ = false; return; }
    writing_ = true;
    auto self = shared_from_this();
    ws_->async_write(asio::buffer(write_queue_.front()),
        asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->logger_.warn() << "[sess:" << self.get() << "] write error: " << ec.message();
                self->closing_ = true;
                self->write_queue_.clear();
                self->writing_ = false;
                self->app_.reset();
                return;
            }
            self->write_queue_.pop_front();
            self->do_write();
        }));
}

void Session::do_http_read()
{
    auto self = shared_from_this();
    http::async_read(*stream_, buf_, req_,
        asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->logger_.warn() << "[sess:" << self.get() << "] http read error: " << ec.message();
                return;
            }
            if (websocket::is_upgrade(self->req_)) {
                self->do_ws_accept();
            } else {
                self->do_http_response();
            }
        }));
}

void Session::do_ws_accept()
{
    ws_.emplace(std::move(*stream_));
    stream_.reset();
    auto self = shared_from_this();
    ws_->async_accept(req_,
        asio::bind_executor(strand_, [self](beast::error_code ec) {
            if (ec) {
                self->logger_.warn() << "[sess:" << self.get() << "] ws accept error: " << ec.message();
                return;
            }
            self->logger_.info() << "[sess:" << self.get() << "] ws upgrade ok";
            self->buf_.clear();
            self->do_read_first_msg();
        }));
}

void Session::do_http_response()
{
    namespace http = beast::http;

    logger_.info() << "[sess:" << this << "] http request: " << req_.method_string() << " " << req_.target();

    auto self = shared_from_this();

    if (req_.method() == http::verb::options) {
        http::response<http::empty_body> res;
        res.version(11);
        res.result(http::status::no_content);
        res.set(http::field::server, "gameserver");
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "GET, OPTIONS");
        res.set(http::field::access_control_allow_headers, "*");
        res.set(http::field::access_control_max_age, "86400");
        http::async_write(*stream_, res,
            asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
                if (ec)
                    self->logger_.warn() << "[sess:" << self.get() << "] http write error: " << ec.message();
            }));
        return;
    }

    http::response<http::string_body> res;
    res.version(11);
    res.result(http::status::ok);
    res.set(http::field::server, "gameserver");
    res.set(http::field::content_type, "application/json");
    res.set(http::field::access_control_allow_origin, "*");

    boost::json::object obj;
    obj["port"] = port_;
    res.body() = boost::json::serialize(obj);
    res.prepare_payload();

    http::async_write(*stream_, res,
        asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
            if (ec)
                self->logger_.warn() << "[sess:" << self.get() << "] http write error: " << ec.message();
            else
                self->logger_.info() << "[sess:" << self.get() << "] http response sent";
        }));
}

void Session::do_read_first_msg()
{
    auto self = shared_from_this();
    ws_->async_read(buf_,
        asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->logger_.warn() << "[sess:" << self.get() << "] first read error: " << ec.message();
                return;
            }
            self->first_msg_ = beast::buffers_to_string(self->buf_.data());
            self->buf_.clear();
            self->route_and_setup();
        }));
}

void Session::route_and_setup()
{
    std::string app_name;
    try {
        auto val = boost::json::parse(first_msg_);
        if (val.is_object()) {
            std::string s = jsonParseStr(val, key::APP);
            if (!s.empty()) {
                app_name = std::move(s);
            } else {
                s = jsonParseStr(val, key::TEXT);
                if (!s.empty()) {
                    app_name = appname::CHAT;
                } else {
                    s = jsonParseStr(val, key::GAME);
                    if (!s.empty())
                        app_name = std::move(s);
                    else
                        app_name = appname::SNAKE;
                }
            }
        }
    } catch (...) {}

    logger_.info() << "Routing to app: " << app_name;

    mod_ = cache_.load(app_name);
    if (!mod_) {
        enqueue(jsonError("failed to load " + app_name));
        close_ws();
        return;
    }

    app_ = mod_.create(first_msg_);
    if (!app_) {
        enqueue(jsonError("failed to create " + app_name + " instance"));
        close_ws();
        return;
    }

    if (mod_.is_async()) {
        mod_.app_set_output(app_.get(), &Session::app_output_cb, this);
        if (mod_.app_set_io_context)
            mod_.app_set_io_context(app_.get(), io_ctx_);
        mod_.app_on_input(app_.get(), first_msg_.c_str());
        do_read();
    } else {
        process_legacy(first_msg_);
    }
}

void Session::app_output_cb(void* userdata, const char* json)
{
    static_cast<Session*>(userdata)->on_app_output(json);
}

void Session::do_read()
{
    if (closing_) return;
    auto self = shared_from_this();
    ws_->async_read(buf_,
        asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->logger_.warn() << "[sess:" << self.get() << "] read error: " << ec.message();
                self->closing_ = true;
                self->app_.reset();
                return;
            }
            self->on_read(ec, 0);
        }));
}

void Session::on_read(beast::error_code /*ec*/, std::size_t /*n*/)
{
    std::string msg = beast::buffers_to_string(buf_.data());
    buf_.clear();

    if (mod_.is_async()) {
        mod_.app_on_input(app_.get(), msg.c_str());
        if (app_is_done()) {
            logger_.info() << "[sess:" << this << "] app done, closing";
            close_ws();
        } else {
            do_read();
        }
    } else {
        process_legacy(std::move(msg));
    }
}

void Session::process_legacy(const std::string& msg)
{
    auto app  = app_.get();
    auto mod  = &mod_;
    auto self = shared_from_this();

    fallback_pool_->submit([self, msg, app, mod]() {
        char* out = mod->app_process(app, msg.c_str());
        if (out) {
            std::string results(out);
            mod->app_free_string(out);
            asio::post(self->strand_,
                [self, results = std::move(results)]() {
                    try {
                        auto arr = boost::json::parse(results).as_array();
                        for (auto& item : arr)
                            self->enqueue(boost::json::serialize(item));
                    } catch (...) {}
                });
        }

        asio::post(self->strand_, [self]() {
            if (self->app_is_done()) {
                self->close_ws();
            } else {
                self->do_read();
            }
        });
    });
}

bool Session::app_is_done() const
{
    return mod_.app_is_done
        && mod_.app_is_done(app_.get()) != 0;
}

void Session::close_ws()
{
    if (ws_ && !closing_) {
        closing_ = true;
        logger_.info() << "[sess:" << this << "] closing ws";
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
    }
}

Listener::Listener(asio::io_context& io, Logger& logger,
                   AppModuleCache& cache, ThreadPool* fallback_pool,
                   int port)
    : io_(io)
    , acceptor_(io, tcp::endpoint(tcp::v4(), port))
    , logger_(logger)
    , cache_(cache)
    , fallback_pool_(fallback_pool)
    , port_(port)
{}

void Listener::run()
{
    do_accept();
}

void Listener::shutdown()
{
    beast::error_code ec;
    acceptor_.close(ec);
}

void Listener::do_accept()
{
    auto self = shared_from_this();
    acceptor_.async_accept(
        [self](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<Session>(
                    std::move(socket), self->logger_,
                    self->cache_, self->fallback_pool_,
                    &self->io_, self->port_);
                session->start();
            } else if (ec != asio::error::operation_aborted) {
                self->logger_.warn() << "accept error: " << ec.message();
            }
            if (ec != asio::error::operation_aborted)
                self->do_accept();
        });
}
