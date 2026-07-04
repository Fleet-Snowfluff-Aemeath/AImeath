#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <utility>
#include <thread>
#include <boost/json.hpp>
#include <boost/noncopyable.hpp>
#include "iface_mod.hpp"

class Session;

class SessionRegistry : private boost::noncopyable
{
public:
    void registerSession(const std::string& appName, std::weak_ptr<Session> session);
    std::shared_ptr<Session> findSession(const std::string& appName, int index = 0);
    std::vector<std::shared_ptr<Session>> findAllSessions(const std::string& appName);
    void unregisterSession(const std::string& appName, Session* ptr);
    std::vector<std::pair<std::string, int>> listSessions();

    void registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& appName);
    void unregisterWindow(const std::string& windowId);
    boost::json::array listActiveWindows();

    void stashApp(const std::string& windowId, AppPtr app, AppModule mod, std::string appName);
    bool restoreApp(const std::string& windowId, AppPtr& outApp, AppModule& outMod, std::string& outAppName);
    void removeStashedApp(const std::string& windowId);
    void setStashTtlSec(int ttl) { stashTtlSec_ = ttl; }

private:
    std::mutex mtx_;
    std::map<std::string, std::vector<std::weak_ptr<Session>>> sessions_;
    struct WinInfo {
        std::string sessionId;
        std::string appName;
    };
    std::map<std::string, WinInfo> windowMap_;

    struct StashedApp {
        AppPtr app;
        AppModule mod;
        std::string appName;
        std::chrono::steady_clock::time_point at;
    };
    std::map<std::string, StashedApp> stashedApps_;
    int stashTtlSec_{0};
};

class Config : private boost::noncopyable
{
public:
    static Config& instance();

    int getInt(const std::string& key, int default_val = 0) const;
    std::string getString(const std::string& key, const std::string& default_val = "") const;

    // L1: typed convenience accessors keep callers clean
    //     and centralize default-value knowledge.
    int port() const { return getInt("port", 3001); }
    std::string deepSeekApiKey() const { return getString("deepseek_api_key"); }
    std::string fileRoot() const { return getString("file_root", "desktop/public/home"); }

    static int defaultIoThreads() {
        unsigned n = std::thread::hardware_concurrency();
        return n > 0 ? static_cast<int>(n) : 4;
    }
    int ioThreads() const { return getInt("io_threads", defaultIoThreads()); }
    int fallbackThreads() const { return getInt("fallback_threads", defaultIoThreads()); }
    int maxConnections() const { return getInt("max_connections", 0); }
    int pingIntervalSec() const { return getInt("ping_interval_sec", 30); }
    int stashTtlSec() const { return getInt("stash_ttl_sec", 0); }

    void setAppStateNotifyFn(void (*fn)(const char* app, const char* state, void* ctx), void* ctx) {
        state_notify_fn_ = fn;
        state_notify_ctx_ = ctx;
    }
    void fireAppStateNotify(const std::string& app, const std::string& state) const {
        if (state_notify_fn_) state_notify_fn_(app.c_str(), state.c_str(), state_notify_ctx_);
    }

    SessionRegistry& sessionRegistry() { return session_registry_; }

private:
    void (*state_notify_fn_)(const char*, const char*, void*) = nullptr;
    void* state_notify_ctx_ = nullptr;
    SessionRegistry session_registry_;
    Config();
    void load();

    boost::json::object root_;
};
