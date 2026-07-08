#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>

#include <boost/json.hpp>
#include <boost/noncopyable.hpp>

#include "plugin.hpp"
#include "eventmgr.hpp"
#include "logger.hpp"

class Session;

class AppManager : private boost::noncopyable
{
public:
    static AppManager& instance();

    void init(IPluginCache* cache, Logger* logger);

    using StateCallback = std::function<void(const std::string& appName, const boost::json::value& state)>;
    uint64_t subscribe(StateCallback cb);
    void unsubscribe(uint64_t handle);

    void notifyStateChange(const std::string& appName, const boost::json::value& state);

private:
    AppManager() = default;

    IPluginCache* cache_ = nullptr;
    Logger* logger_ = nullptr;
    std::mutex mtx_;

    struct SubEntry {
        StateCallback cb;
    };
    std::unordered_map<uint64_t, SubEntry> subscribers_;
    uint64_t nextSubId_ = 1;

    Subscription appStateSub_;
};
