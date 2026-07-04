#include "ws_server.hpp"
#include "config.hpp"
#include <chrono>

Session::Session(tcp::socket socket, Logger& logger,
                 IModuleCache& cache, ThreadPool* fallback_pool,
                 asio::io_context* io_ctx, int port)
    : logger_(logger)
    , cache_(cache)
    , fallback_pool_(fallback_pool)
    , io_ctx_(io_ctx)
    , strand_(io_ctx->get_executor())
    , port_(port)
    , ping_timer_(*io_ctx)
    , ping_interval_(std::chrono::seconds(Config::instance().pingIntervalSec()))
{
    stream_.emplace(std::move(socket));
    static int seq = 0;
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    session_id_ = "sess_" + std::to_string(ts) + "_" + std::to_string(++seq);
    logger_.info() << "[sess:" << this << "|" << session_id_ << "] new connection";
}

Session::~Session()
{
    if (connection_count_)
        connection_count_->fetch_sub(1, std::memory_order_release);
    if (closing_) return;
    if (!app_name_.empty()) {
        Config::instance().sessionRegistry().unregisterSession(app_name_, this);
        if (!window_id_.empty())
            Config::instance().sessionRegistry().unregisterWindow(window_id_);
    }
}

bool Session::is_open() const
{
    return !closing_ && ws_ && ws_->is_open();
}

void Session::start()
{
    do_http_read();
}

void Session::on_app_output(const char* json)
{
    if (closing_) return;
    reset_heartbeat();
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
    if (write_queue_.empty()) {
        writing_ = false;
        if (close_after_write_ && !closing_) close_ws();
        return;
    }
    if (closing_) { writing_ = false; return; }
    writing_ = true;
    auto self = shared_from_this();
    ws_->async_write(asio::buffer(write_queue_.front()),
        asio::bind_executor(strand_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->logger_.warn() << "[sess:" << self.get() << "] write error: " << ec.message();
                self->write_queue_.clear();
                self->writing_ = false;
                if (!self->closing_)
                    self->do_cleanup();
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
            boost::json::object hs;
            hs["type"] = "session";
            hs["id"] = self->session_id_;
            self->enqueue(boost::json::serialize(hs));
            self->buf_.clear();
            self->schedule_ping();
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
        res.set(http::field::server, "AImeath");
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
    res.set(http::field::server, "AImeath");
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
    std::string action;
    try {
        auto val = boost::json::parse(first_msg_);
        if (val.is_object()) {
            auto& obj = val.as_object();
            auto widIt = obj.find("window_id");
            if (widIt != obj.end() && widIt->value().is_string())
                window_id_ = std::string(widIt->value().as_string());

            auto dnIt = obj.find("display_name");
            if (dnIt != obj.end() && dnIt->value().is_string())
                display_name_ = std::string(dnIt->value().as_string());

            auto actIt = obj.find("action");
            if (actIt != obj.end() && actIt->value().is_string())
                action = std::string(actIt->value().as_string());

            std::string s = jsonParseStr(val, key::APP);
            if (!s.empty()) {
                app_name = std::move(s);
            } else {
                s = jsonParseStr(val, key::GAME);
                if (!s.empty())
                    app_name = std::move(s);
                else
                    app_name = appname::CHAT;
            }
        }
    } catch (...) {}

    if (action == "resume" && !window_id_.empty()) {
        AppPtr restoredApp;
        AppModule restoredMod;
        std::string restoredName;
        auto& reg = Config::instance().sessionRegistry();
        if (reg.restoreApp(window_id_, restoredApp, restoredMod, restoredName)) {
            logger_.info() << "Restored app " << restoredName << " from " << window_id_;
            app_ = std::move(restoredApp);
            mod_ = std::move(restoredMod);
            app_name_ = restoredName;

            reg.registerSession(app_name_, shared_from_this());
            reg.registerWindow(window_id_, session_id_, app_name_);

            if (app_is_done()) {
                logger_.info() << "Restored app is already done, closing";
                reg.removeStashedApp(window_id_);
                enqueue(jsonError("restored app has ended"));
                close_ws();
                return;
            }

            if (mod_.is_async()) {
                mod_.app_set_output(app_.get(), &Session::app_output_cb, this);
                if (mod_.app_set_io_context)
                    mod_.app_set_io_context(app_.get(), io_ctx_);
                do_read();
            } else {
                do_read();
            }
            return;
        }
        enqueue(jsonError("cannot resume: session expired for " + window_id_));
        close_ws();
        return;
    }

    logger_.info() << "Routing to app: " << app_name
                   << (window_id_.empty() ? "" : " wid:" + window_id_);
    app_name_ = app_name;

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

    if (app_name != appname::CHAT) {
        Config::instance().sessionRegistry().registerSession(app_name, shared_from_this());
        if (!window_id_.empty())
            Config::instance().sessionRegistry().registerWindow(window_id_, session_id_, app_name);
    } else {
        Config::instance().sessionRegistry().registerSession(app_name, shared_from_this());
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
                if (!self->closing_)
                    self->do_cleanup();
                return;
            }
            self->on_read(ec, 0);
        }));
}

static bool isCloseWindowMsg(const std::string& msg)
{
    try {
        auto val = boost::json::parse(msg);
        if (val.is_object()) {
            auto& obj = val.as_object();
            auto it = obj.find("action");
            if (it != obj.end() && it->value().is_string()
                && it->value().as_string() == "close_window")
                return true;
        }
    } catch (...) {}
    return false;
}

void Session::on_read(beast::error_code /*ec*/, std::size_t /*n*/)
{
    std::string msg = beast::buffers_to_string(buf_.data());
    buf_.clear();

    reset_heartbeat();

    if (isCloseWindowMsg(msg)) {
        logger_.info() << "[sess:" << this << "] received close_window";
        user_close_ = true;
        if (!window_id_.empty())
            Config::instance().sessionRegistry().removeStashedApp(window_id_);
        close_ws();
        return;
    }

    if (mod_.is_async()) {
        mod_.app_on_input(app_.get(), msg.c_str());
        if (app_is_done()) {
            logger_.info() << "[sess:" << this << "] app done, closing";
            boost::json::object done;
            done["type"] = "app_exited";
            if (!window_id_.empty()) done["window_id"] = window_id_;
            enqueue(boost::json::serialize(done));
            close_after_write_ = true;
        } else {
            do_read();
        }
    } else {
        process_legacy(std::move(msg));
    }
}

void Session::process_legacy(const std::string& msg)
{
    if (isCloseWindowMsg(msg)) {
        logger_.info() << "[sess:" << this << "] legacy close_window";
        user_close_ = true;
        if (!window_id_.empty())
            Config::instance().sessionRegistry().removeStashedApp(window_id_);
        close_ws();
        return;
    }

    auto app  = app_.get();
    auto mod  = &mod_;
    auto self = shared_from_this();

    bool accepted = fallback_pool_->try_submit([self, msg, app, mod]() {
        char* out = mod->app_process(app, msg.c_str());
        if (out) {
            std::string results(out);
            mod->app_free_string(out);
            asio::post(self->strand_,
                [self, results = std::move(results)]() {
                    try {
                        auto arr = boost::json::parse(results).as_array();
                        for (auto& item : arr) {
                            self->enqueue(boost::json::serialize(item));
                            if (item.is_object() && self->app_name_ != appname::CHAT) {
                                auto& obj = item.as_object();
                                auto it = obj.find("data");
                                if (it != obj.end() && it->value().is_object()) {
                                    auto& data = it->value().as_object();
                                    auto overIt = data.find("over");
                                    if (overIt != data.end() && overIt->value().is_bool() && overIt->value().as_bool()) {
                                        Config::instance().fireAppStateNotify(self->app_name_, boost::json::serialize(data));
                                    }
                                }
                            }
                        }
                    } catch (...) {}
                });
        }

        asio::post(self->strand_, [self]() {
            if (self->app_is_done()) {
                if (!self->app_name_.empty() && self->app_name_ != appname::CHAT) {
                    boost::json::object doneState;
                    doneState["over"] = true;
                    doneState["reason"] = "session_closed";
                    Config::instance().fireAppStateNotify(self->app_name_, boost::json::serialize(doneState));
                }
                self->close_ws();
            } else {
                self->do_read();
            }
        });
    });

    if (!accepted) {
        enqueue(jsonError("server overloaded, please retry"));
        do_read();
    }
}

bool Session::app_is_done() const
{
    return mod_.app_is_done
        && mod_.app_is_done(app_.get()) != 0;
}

std::string Session::call_app_process(const std::string& input)
{
    if (!mod_ || !app_) return "[]";
    char* out = mod_.app_process(app_.get(), input.c_str());
    std::string result(out ? out : "[]");
    if (mod_.app_free_string)
        mod_.app_free_string(out);
    return result;
}

std::string Session::call_app_process_and_notify(const std::string& input)
{
    if (!mod_ || !app_) return "[]";
    char* out = mod_.app_process(app_.get(), input.c_str());
    std::string result(out ? out : "[]");
    if (mod_.app_free_string)
        mod_.app_free_string(out);

    // 推送到游戏 WebSocket 客户端，使其显示更新
    std::string copy = result;
    asio::post(strand_, [self = shared_from_this(), copy = std::move(copy)]() {
        if (self->closing_) return;
        try {
            auto arr = boost::json::parse(copy).as_array();
            for (auto& item : arr)
                self->enqueue(boost::json::serialize(item));
        } catch (...) {}
    });

    return result;
}

void Session::do_cleanup()
{
    closing_ = true;
    ping_timer_.cancel();

    bool stashed = false;
    if (!window_id_.empty() && app_ && !user_close_ && !app_is_done()) {
        auto& reg = Config::instance().sessionRegistry();
        reg.stashApp(window_id_, std::move(app_), std::move(mod_), app_name_);
        reg.unregisterSession(app_name_, this);
        stashed = true;
        logger_.info() << "[sess:" << this << "] app stashed for " << window_id_;
    }

    if (!stashed && !app_name_.empty()) {
        Config::instance().sessionRegistry().unregisterSession(app_name_, this);
        if (!window_id_.empty())
            Config::instance().sessionRegistry().unregisterWindow(window_id_);
        boost::json::object doneState;
        doneState["over"] = true;
        doneState["reason"] = "session_closed";
        doneState["session_id"] = session_id_;
        if (!window_id_.empty()) doneState["window_id"] = window_id_;
        if (!display_name_.empty()) doneState["display_name"] = display_name_;
        Config::instance().fireAppStateNotify(app_name_, boost::json::serialize(doneState));
    }
    app_.reset();
}

void Session::close_ws()
{
    if (ws_ && !closing_) {
        do_cleanup();
        logger_.info() << "[sess:" << this << "] closing ws";
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
    }
}

void Session::schedule_ping()
{
    if (closing_) return;
    auto self = shared_from_this();
    ping_timer_.expires_after(ping_interval_);
    ping_timer_.async_wait(
        asio::bind_executor(strand_, [self](beast::error_code ec) {
            self->on_ping_timer(ec);
        }));
}

void Session::on_ping_timer(beast::error_code ec)
{
    if (ec == asio::error::operation_aborted || closing_) return;

    if (missed_pongs_ >= MAX_MISSED_PONGS) {
        logger_.warn() << "[sess:" << this << "] heartbeat lost after "
                       << MAX_MISSED_PONGS << " missed pongs, closing";
        close_ws();
        return;
    }

    missed_pongs_++;
    if (ws_ && ws_->is_open()) {
        ws_->async_ping("",
            asio::bind_executor(strand_, [](beast::error_code) {}));
    }

    schedule_ping();
}

void Session::reset_heartbeat()
{
    missed_pongs_ = 0;
}

Listener::Listener(asio::io_context& io, Logger& logger,
                   IModuleCache& cache, ThreadPool* fallback_pool,
                   int port)
    : io_(io)
    , acceptor_(io, tcp::endpoint(tcp::v4(), port))
    , logger_(logger)
    , cache_(cache)
    , fallback_pool_(fallback_pool)
    , port_(port)
    , connection_count_(std::make_shared<std::atomic<size_t>>(0))
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
            self->on_accept(ec, std::move(socket));
        });
}

void Listener::on_accept(beast::error_code ec, tcp::socket socket)
{
    if (ec == asio::error::operation_aborted)
        return;

    if (!ec) {
        if (max_connections_ > 0 && connection_count_->load() >= static_cast<size_t>(max_connections_)) {
            logger_.warn() << "connection rejected: max connections (" << max_connections_ << ") reached";
            beast::error_code ignore;
            socket.close(ignore);
        } else {
            connection_count_->fetch_add(1, std::memory_order_release);
            auto session = std::make_shared<Session>(
                std::move(socket), logger_,
                cache_, fallback_pool_,
                &io_, port_);
            session->set_connection_counter(connection_count_);
            session->start();
        }
    } else if (ec != asio::error::operation_aborted) {
        logger_.warn() << "accept error: " << ec.message();
    }

    if (ec != asio::error::operation_aborted)
        do_accept();
}
