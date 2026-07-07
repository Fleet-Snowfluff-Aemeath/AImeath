#pragma once

#include <string>
#include <thread>
#include <boost/json.hpp>
#include <boost/noncopyable.hpp>

class Config : private boost::noncopyable
{
public:
    static Config& instance();

    int getInt(const std::string& key, int default_val = 0) const;
    std::string getString(const std::string& key, const std::string& default_val = "") const;

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

    void setChatCachePtr(uintptr_t ptr) { chat_cache_ptr_ = ptr; }
    uintptr_t chatCachePtr() const { return chat_cache_ptr_; }

    void setAppStateNotifyFn(void (*fn)(const char* app, const char* state, void* ctx), void* ctx) {
        state_notify_fn_ = fn;
        state_notify_ctx_ = ctx;
    }
    void fireAppStateNotify(const std::string& app, const std::string& state) const {
        if (state_notify_fn_) state_notify_fn_(app.c_str(), state.c_str(), state_notify_ctx_);
    }

private:
    Config();
    void load();

    uintptr_t chat_cache_ptr_ = 0;
    void (*state_notify_fn_)(const char*, const char*, void*) = nullptr;
    void* state_notify_ctx_ = nullptr;
    boost::json::object root_;
};
