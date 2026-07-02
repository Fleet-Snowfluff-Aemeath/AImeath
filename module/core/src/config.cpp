#include "config.hpp"
#include "ws_server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

void SessionRegistry::registerSession(const std::string& appName, std::weak_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_[appName] = std::move(session);
}

std::shared_ptr<Session> SessionRegistry::findSession(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = sessions_.find(appName);
    if (it == sessions_.end())
        return nullptr;
    auto s = it->second.lock();
    if (!s) {
        sessions_.erase(it);
        return nullptr;
    }
    return s;
}

void SessionRegistry::unregisterSession(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_.erase(appName);
}

std::vector<std::string> SessionRegistry::listSessions()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> names;
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (it->second.expired()) {
            it = sessions_.erase(it);
        } else {
            names.push_back(it->first);
            ++it;
        }
    }
    return names;
}

Config& Config::instance()
{
    // C++11 function-static — thread-safe per standard
    static Config cfg;
    return cfg;
}

Config::Config()
{
    load();
}

void Config::load()
{
    // L1: silent on failure — caller decides whether to warn/fallback.
    //     Both main.cpp and chat_server.cpp previously printed warnings;
    //     that responsibility stays at the call site.
    std::ifstream f("config.json");
    if (!f.is_open())
        return;

    std::string content(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    try {
        auto val = boost::json::parse(content);
        root_ = val.as_object();
    } catch (const std::exception& e) {
        std::cerr << "Config: failed to parse config.json: " << e.what() << "\n";
    }
}

int Config::getInt(const std::string& key, int default_val) const
{
    // L1: iterating root_ on every call trades a tiny perf cost
    //     for simple, obvious semantics. config.json is tiny (<10 fields),
    //     so the O(n) scan is noise.
    auto it = root_.find(key);
    if (it == root_.end())
        return default_val;
    if (!it->value().is_int64())
        return default_val;
    return it->value().as_int64();
}

std::string Config::getString(const std::string& key, const std::string& default_val) const
{
    auto it = root_.find(key);
    if (it == root_.end())
        return default_val;
    if (!it->value().is_string())
        return default_val;
    return it->value().as_string().c_str();
}
