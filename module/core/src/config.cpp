#include "config.hpp"
#include "ws_server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

void SessionRegistry::registerSession(const std::string& appName, std::weak_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_[appName].push_back(std::move(session));
}

std::shared_ptr<Session> SessionRegistry::findSession(const std::string& appName, int index)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = sessions_.find(appName);
    if (it == sessions_.end())
        return nullptr;
    auto& vec = it->second;
    // 清理过期项
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [](auto& w) { return w.expired(); }), vec.end());
    if (vec.empty()) {
        sessions_.erase(it);
        return nullptr;
    }
    if (index < 0 || index >= (int)vec.size())
        return nullptr;
    return vec[index].lock();
}

std::vector<std::shared_ptr<Session>> SessionRegistry::findAllSessions(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::shared_ptr<Session>> result;
    auto it = sessions_.find(appName);
    if (it == sessions_.end())
        return result;
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [](auto& w) { return w.expired(); }), vec.end());
    if (vec.empty()) {
        sessions_.erase(it);
        return result;
    }
    for (auto& w : vec) {
        auto s = w.lock();
        if (s) result.push_back(std::move(s));
    }
    return result;
}

void SessionRegistry::unregisterSession(const std::string& appName, Session* ptr)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = sessions_.find(appName);
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

std::vector<std::pair<std::string, int>> SessionRegistry::listSessions()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::pair<std::string, int>> result;
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [](auto& w) { return w.expired(); }), vec.end());
        if (vec.empty()) {
            it = sessions_.erase(it);
        } else {
            for (int i = 0; i < (int)vec.size(); ++i)
                result.emplace_back(it->first, i);
            ++it;
        }
    }
    return result;
}

void SessionRegistry::registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& appName)
{
    std::lock_guard<std::mutex> lock(mtx_);
    windowMap_[windowId] = WinInfo{sessionId, appName};
}

void SessionRegistry::unregisterWindow(const std::string& windowId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    windowMap_.erase(windowId);
}

boost::json::array SessionRegistry::listActiveWindows()
{
    std::lock_guard<std::mutex> lock(mtx_);
    boost::json::array result;
    for (auto& [appName, vec] : sessions_) {
        int idx = 0;
        for (auto& w : vec) {
            auto s = w.lock();
            if (!s) continue;
            boost::json::object entry;
            entry["window_id"] = s->window_id();
            entry["session_id"] = s->session_id();
            entry["app"] = appName;
            entry["instance"] = idx++;
            if (!s->display_name().empty())
                entry["display_name"] = s->display_name();
            result.push_back(std::move(entry));
        }
    }
    return result;
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
