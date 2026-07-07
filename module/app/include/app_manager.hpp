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

class Session;

class AppManager : private boost::noncopyable
{
public:
    static AppManager& instance();

    void init(IPluginCache* cache);

    bool openApp(const std::string& appName, const std::string& configJson);
    bool closeApp(const std::string& appName);
    boost::json::value controlApp(const std::string& appName, const std::string& commandJson);
    boost::json::value getAppState(const std::string& appName);
    boost::json::array listApps();

    using StateCallback = std::function<void(const std::string& appName, const boost::json::value& state)>;
    uint64_t subscribe(StateCallback cb);
    void unsubscribe(uint64_t handle);

    void notifyStateChange(const std::string& appName, const boost::json::value& state);

    void registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& appName);
    void unregisterWindow(const std::string& windowId);
    boost::json::array listActiveWindows();

private:
    AppManager() = default;

    IPluginCache* cache_ = nullptr;
    std::mutex mtx_;

    struct SubEntry {
        StateCallback cb;
    };
    std::unordered_map<uint64_t, SubEntry> subscribers_;
    uint64_t nextSubId_ = 1;

    Subscription appStateSub_;
};
