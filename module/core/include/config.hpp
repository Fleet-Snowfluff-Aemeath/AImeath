#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <boost/json.hpp>
#include <boost/noncopyable.hpp>

class Session;

class SessionRegistry : private boost::noncopyable
{
public:
    void registerSession(const std::string& appName, std::weak_ptr<Session> session);
    std::shared_ptr<Session> findSession(const std::string& appName);
    void unregisterSession(const std::string& appName);
    std::vector<std::string> listSessions();

private:
    std::mutex mtx_;
    std::map<std::string, std::weak_ptr<Session>> sessions_;
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
    std::string gitToken() const { return getString("git_token"); }
    std::string fileRoot() const { return getString("file_root", "desktop/public/home"); }

    void setChatCachePtr(uintptr_t ptr) { chat_cache_ptr_ = ptr; }
    uintptr_t chatCachePtr() const { return chat_cache_ptr_; }

    SessionRegistry& sessionRegistry() { return session_registry_; }

private:
    uintptr_t chat_cache_ptr_ = 0;
    SessionRegistry session_registry_;
    Config();
    void load();

    boost::json::object root_;
};
