#include "ws_server.hpp"
#include "config.hpp"
#include <chrono>
#include <algorithm>
#include <future>

Session::Session(tcp::socket socket, Logger& logger,
                 IPluginCache& cache, ThreadPool* fallback_pool,
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
    if (!plugin_name_.empty()) {
        SessionManager::instance().unregisterSession(plugin_name_, this);
        if (!window_id_.empty())
            SessionManager::instance().unregisterWindow(window_id_);
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

void Session::on_plugin_output(const char* json)
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
            self->start_ping();
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
    std::string plugin_name;
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
                plugin_name = std::move(s);
            } else {
                s = jsonParseStr(val, key::GAME);
                if (!s.empty())
                    plugin_name = std::move(s);
                else
                    plugin_name = pluginname::CHAT;
            }
        }
    } catch (...) {}

    if (action == "resume") {
        if (window_id_.empty()) {
            enqueue(jsonError("resume requires window_id"));
            close_ws();
            return;
        }
        PluginInstance restoredPlugin;
        std::string restoredName;
        auto& reg = SessionManager::instance();
        if (reg.restorePlugin(window_id_, restoredPlugin, restoredName)) {
            logger_.info() << "Restored plugin " << restoredName << " from " << window_id_;
            plugin_ = std::move(restoredPlugin);
            plugin_name_ = restoredName;

            reg.registerSession(plugin_name_, shared_from_this());
            reg.registerWindow(window_id_, session_id_, plugin_name_);

            if (plugin_.isDone()) {
                logger_.info() << "Restored plugin is already done, closing";
                reg.removeStashedPlugin(window_id_);
                enqueue(jsonError("restored plugin has ended"));
                close_ws();
                return;
            }

            if (plugin_.isAsync()) {
                plugin_.setOutput(&Session::plugin_output_cb, this);
                plugin_.setIoContext(io_ctx_);
            }
            do_read();
            return;
        }
        enqueue(jsonError("cannot resume: session expired for " + window_id_));
        close_ws();
        return;
    }

    logger_.info() << "Routing to plugin: " << plugin_name
                   << (window_id_.empty() ? "" : " wid:" + window_id_);
    plugin_name_ = plugin_name;

    auto mod = cache_.load(plugin_name);
    if (!mod) {
        enqueue(jsonError("failed to load " + plugin_name));
        close_ws();
        return;
    }

    plugin_ = mod.createInstance(first_msg_);
    if (!plugin_) {
        enqueue(jsonError("failed to create " + plugin_name + " instance"));
        close_ws();
        return;
    }

    if (plugin_name != pluginname::CHAT) {
        SessionManager::instance().registerSession(plugin_name, shared_from_this());
        if (!window_id_.empty())
            SessionManager::instance().registerWindow(window_id_, session_id_, plugin_name);
    } else {
        SessionManager::instance().registerSession(plugin_name, shared_from_this());
    }

    if (plugin_.isAsync()) {
        plugin_.setOutput(&Session::plugin_output_cb, this);
        plugin_.setIoContext(io_ctx_);
        plugin_.onInput(first_msg_);
        do_read();
    } else {
        process_legacy(first_msg_);
    }
}

void Session::plugin_output_cb(void* userdata, const char* json)
{
    static_cast<Session*>(userdata)->on_plugin_output(json);
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

    try {
        auto val = boost::json::parse(msg);
        if (val.is_object()) {
            auto& obj = val.as_object();
            auto typeIt = obj.find("type");
            if (typeIt != obj.end() && typeIt->value().is_string()
                && typeIt->value().as_string() == "pong") {
                do_read();
                return;
            }
        }
    } catch (...) {}

    if (isCloseWindowMsg(msg)) {
        logger_.info() << "[sess:" << this << "] received close_window";
        user_close_ = true;
        if (!window_id_.empty())
            SessionManager::instance().removeStashedPlugin(window_id_);
        close_ws();
        return;
    }

    if (plugin_.isAsync()) {
        plugin_.onInput(msg);
        if (plugin_.isDone()) {
            logger_.info() << "[sess:" << this << "] plugin done, closing";
            boost::json::object done;
            done["type"] = "plugin_exited";
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
            SessionManager::instance().removeStashedPlugin(window_id_);
        close_ws();
        return;
    }

    if (!plugin_) {
        do_read();
        return;
    }

    auto results = plugin_.processParsed(msg);
    for (auto& item : results) {
        enqueue(boost::json::serialize(item));
        if (item.is_object() && plugin_name_ != pluginname::CHAT) {
            auto& obj = item.as_object();
            auto it = obj.find("data");
            if (it != obj.end() && it->value().is_object()) {
                auto& data = it->value().as_object();
                auto overIt = data.find("over");
                if (overIt != data.end() && overIt->value().is_bool() && overIt->value().as_bool()) {
                    pluginEventBus().fire(PluginStateEvent{plugin_name_, boost::json::value(data)});
                }
            }
        }
    }

    if (plugin_.isDone()) {
        if (!plugin_name_.empty() && plugin_name_ != pluginname::CHAT) {
            boost::json::object doneState;
            doneState["over"] = true;
            doneState["reason"] = "session_closed";
            pluginEventBus().fire(PluginStateEvent{plugin_name_, boost::json::value(doneState)});
        }
        close_ws();
    } else {
        do_read();
    }
}

std::string Session::call_plugin_process(const std::string& input)
{
    if (closing_) return "[]";
    std::promise<std::string> p;
    auto f = p.get_future();
    auto self = shared_from_this();
    asio::post(strand_, [self, input = std::string(input), p = std::move(p)]() mutable {
        if (self->closing_ || !self->plugin_) {
            p.set_value("[]");
            return;
        }
        std::string result = self->plugin_.process(input);
        self->reset_heartbeat();
        p.set_value(std::move(result));
    });
    return f.get();
}

std::string Session::call_plugin_process_and_notify(const std::string& input)
{
    if (closing_) return "[]";
    std::promise<std::string> p;
    auto f = p.get_future();
    auto self = shared_from_this();
    asio::post(strand_, [self, input = std::string(input), p = std::move(p)]() mutable {
        if (self->closing_ || !self->plugin_) {
            p.set_value("[]");
            return;
        }
        std::string result = self->plugin_.process(input);

        self->reset_heartbeat();
        try {
            auto arr = boost::json::parse(result).as_array();
            for (auto& item : arr)
                self->enqueue(boost::json::serialize(item));
        } catch (...) {}
        p.set_value(std::move(result));
    });
    return f.get();
}

void Session::do_cleanup()
{
    closing_ = true;
    if (ping_timer_id_)
        ping_timer_.cancel(ping_timer_id_);

    bool stashed = false;
    if (!window_id_.empty() && plugin_ && !user_close_ && !plugin_.isDone()) {
        auto& reg = SessionManager::instance();
        reg.stashPlugin(window_id_, std::move(plugin_), plugin_name_);
        reg.unregisterSession(plugin_name_, this);
        stashed = true;
        logger_.info() << "[sess:" << this << "] plugin stashed for " << window_id_;
    }

    if (!stashed && !plugin_name_.empty()) {
        SessionManager::instance().unregisterSession(plugin_name_, this);
        if (!window_id_.empty())
            SessionManager::instance().unregisterWindow(window_id_);
        boost::json::object doneState;
        doneState["over"] = true;
        doneState["reason"] = "session_closed";
        doneState["session_id"] = session_id_;
        if (!window_id_.empty()) doneState["window_id"] = window_id_;
        if (!display_name_.empty()) doneState["display_name"] = display_name_;
        pluginEventBus().fire(PluginStateEvent{plugin_name_, boost::json::value(doneState)});
    }
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

void Session::start_ping()
{
    if (closing_) return;
    auto self = shared_from_this();
    ping_timer_id_ = ping_timer_.setInterval(ping_interval_, [self]() {
        asio::post(self->strand_, [self] {
            if (self->closing_) return;

            if (self->missed_pongs_ >= MAX_MISSED_PONGS) {
                self->logger_.warn() << "[sess:" << self.get()
                    << "] heartbeat lost after " << MAX_MISSED_PONGS << " missed pongs, closing";
                self->close_ws();
                return;
            }

            self->missed_pongs_++;
            self->enqueue(R"({"type":"ping"})");
        });
    });
}

void Session::reset_heartbeat()
{
    missed_pongs_ = 0;
}

Listener::Listener(asio::io_context& io, Logger& logger,
                   IPluginCache& cache, ThreadPool* fallback_pool,
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

// ============================================================
//  SessionManager
// ============================================================

SessionManager& SessionManager::instance()
{
    static SessionManager mgr;
    return mgr;
}

void SessionManager::registerSession(const std::string& pluginName, std::weak_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_[pluginName].push_back(std::move(session));
}

void SessionManager::purgeDead(std::vector<std::weak_ptr<Session>>& vec)
{
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [](auto& w) {
            auto s = w.lock();
            return !s || !s->is_open();
        }), vec.end());
}

std::shared_ptr<Session> SessionManager::findSession(const std::string& pluginName, int index)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = sessions_.find(pluginName);
    if (it == sessions_.end())
        return nullptr;
    purgeDead(it->second);
    if (it->second.empty()) {
        sessions_.erase(it);
        return nullptr;
    }
    if (index < 0 || index >= (int)it->second.size())
        return nullptr;
    return it->second[index].lock();
}

std::vector<std::shared_ptr<Session>> SessionManager::findAllSessions(const std::string& pluginName)
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::shared_ptr<Session>> result;
    auto it = sessions_.find(pluginName);
    if (it == sessions_.end())
        return result;
    purgeDead(it->second);
    if (it->second.empty()) {
        sessions_.erase(it);
        return result;
    }
    for (auto& w : it->second) {
        auto s = w.lock();
        if (s) result.push_back(std::move(s));
    }
    return result;
}

void SessionManager::unregisterSession(const std::string& pluginName, Session* ptr)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = sessions_.find(pluginName);
    if (it == sessions_.end())
        return;
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [ptr](auto& w) {
            auto s = w.lock();
            return !s || s.get() == ptr;
        }), vec.end());
    if (vec.empty())
        sessions_.erase(it);
}

std::vector<std::pair<std::string, int>> SessionManager::listSessions()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::pair<std::string, int>> result;
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        purgeDead(it->second);
        if (it->second.empty()) {
            it = sessions_.erase(it);
        } else {
            for (int i = 0; i < (int)it->second.size(); ++i)
                result.emplace_back(it->first, i);
            ++it;
        }
    }
    return result;
}

void SessionManager::registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& pluginName)
{
    std::lock_guard<std::mutex> lock(mtx_);
    windowMap_[windowId] = WinInfo{sessionId, pluginName};
}

void SessionManager::unregisterWindow(const std::string& windowId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    windowMap_.erase(windowId);
}

void SessionManager::stashPlugin(const std::string& windowId, PluginInstance plugin, std::string pluginName)
{
    if (windowId.empty()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    StashedPlugin s;
    s.plugin = std::move(plugin);
    s.pluginName = std::move(pluginName);
    s.at = std::chrono::steady_clock::now();
    stashedPlugins_[windowId] = std::move(s);
}

bool SessionManager::restorePlugin(const std::string& windowId, PluginInstance& outPlugin, std::string& outPluginName)
{
    if (windowId.empty()) return false;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = stashedPlugins_.find(windowId);
    if (it == stashedPlugins_.end()) return false;
    if (stashTtlSec_ > 0 && std::chrono::steady_clock::now() - it->second.at > std::chrono::seconds(stashTtlSec_)) {
        stashedPlugins_.erase(it);
        return false;
    }
    outPlugin = std::move(it->second.plugin);
    outPluginName = std::move(it->second.pluginName);
    stashedPlugins_.erase(it);
    return true;
}

void SessionManager::removeStashedPlugin(const std::string& windowId)
{
    if (windowId.empty()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    stashedPlugins_.erase(windowId);
}

boost::json::array SessionManager::listActiveWindows()
{
    std::lock_guard<std::mutex> lock(mtx_);
    boost::json::array result;
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        purgeDead(it->second);
        if (it->second.empty()) {
            it = sessions_.erase(it);
            continue;
        }
        auto& pluginName = it->first;
        int idx = 0;
        for (auto& w : it->second) {
            auto s = w.lock();
            if (!s) continue;
            boost::json::object entry;
            entry["window_id"] = s->window_id();
            entry["session_id"] = s->session_id();
            entry["plugin"] = pluginName;
            entry["instance"] = idx++;
            if (!s->display_name().empty())
                entry["display_name"] = s->display_name();
            result.push_back(std::move(entry));
        }
        ++it;
    }
    return result;
}

// ============================================================
//  pluginEventBus
// ============================================================

EventBus& pluginEventBus()
{
    static EventBus bus;
    return bus;
}
