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

class PluginManager : private boost::noncopyable
{
public:
    static PluginManager& instance();

    void init(IPluginCache* cache);

    bool openPlugin(const std::string& pluginName, const std::string& configJson);
    bool closePlugin(const std::string& pluginName);
    boost::json::value controlPlugin(const std::string& pluginName, const std::string& commandJson);
    boost::json::value getPluginState(const std::string& pluginName);
    boost::json::array listPlugins();

    using StateCallback = std::function<void(const std::string& pluginName, const boost::json::value& state)>;
    uint64_t subscribe(StateCallback cb);
    void unsubscribe(uint64_t handle);

    void notifyStateChange(const std::string& pluginName, const boost::json::value& state);

    void registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& pluginName);
    void unregisterWindow(const std::string& windowId);
    boost::json::array listActiveWindows();

private:
    PluginManager() = default;

    IPluginCache* cache_ = nullptr;
    std::mutex mtx_;

    struct SubEntry {
        StateCallback cb;
    };
    std::unordered_map<uint64_t, SubEntry> subscribers_;
    uint64_t nextSubId_ = 1;

    Subscription pluginStateSub_;
};
