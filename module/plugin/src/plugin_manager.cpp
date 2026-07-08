#include "plugin_manager.hpp"
#include "config.hpp"
#include "ws_server.hpp"

#include <iostream>
#include <chrono>
#include <ctime>

#define APPMGR_LOG(level, msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto t = std::chrono::system_clock::to_time_t(now); \
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
            now.time_since_epoch()) % 1000; \
        char buf[32]; \
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t)); \
        std::cerr << "[" << buf << "." << ms.count() << "] [pluginmgr] " << level << " " << msg << std::endl; \
    } while(0)

PluginManager& PluginManager::instance()
{
    static PluginManager mgr;
    return mgr;
}

void PluginManager::init(IPluginCache* cache)
{
    cache_ = cache;
    pluginStateSub_ = pluginEventBus().subscribe<PluginStateEvent>(
        [this](const PluginStateEvent& e) {
            try {
                auto& stateVal = e.state;
                if (stateVal.is_object()) {
                    auto& obj = stateVal.as_object();
                    auto overIt = obj.find("over");
                    if (overIt != obj.end() && overIt->value().is_bool() && overIt->value().as_bool()) {
                        auto widIt = obj.find("window_id");
                        if (widIt != obj.end() && widIt->value().is_string())
                            unregisterWindow(std::string(widIt->value().as_string()));
                    }
                }
                notifyStateChange(e.pluginName, stateVal);
            } catch (...) {}
        });
    APPMGR_LOG("info", "PluginManager initialized");
}

bool PluginManager::openPlugin(const std::string& pluginName, const std::string& configJson)
{
    if (!cache_) {
        APPMGR_LOG("error", "PluginManager not initialized");
        return false;
    }
    auto mod = cache_->load(pluginName);
    if (!mod) {
        APPMGR_LOG("error", "failed to load module: " << pluginName);
        return false;
    }
    APPMGR_LOG("info", "opened plugin: " << pluginName);
    return true;
}

bool PluginManager::closePlugin(const std::string& pluginName)
{
    APPMGR_LOG("info", "close plugin: " << pluginName);
    return true;
}

boost::json::value PluginManager::controlPlugin(const std::string& pluginName, const std::string& commandJson)
{
    APPMGR_LOG("info", "control plugin: " << pluginName << " cmd: " << commandJson.substr(0, 80));

    auto& registry = SessionManager::instance();
    if (pluginName == pluginname::CHAT) {
        auto sessions = registry.findAllSessions(pluginName);
        if (sessions.empty()) {
            APPMGR_LOG("warn", "no active session for: " << pluginName);
            return boost::json::value(nullptr);
        }
        boost::json::array results;
        for (auto& sess : sessions) {
            std::string r = sess->call_plugin_process(commandJson);
            try {
                results.push_back(boost::json::parse(r));
            } catch (...) {
                results.push_back(boost::json::value(r));
            }
        }
        return results;
    }
    auto sess = registry.findSession(pluginName, 0);
    if (sess) {
        std::string result = sess->call_plugin_process(commandJson);
        try {
            return boost::json::parse(result);
        } catch (...) {
            return boost::json::value(result);
        }
    }

    APPMGR_LOG("warn", "no active session for: " << pluginName);
    return boost::json::value(nullptr);
}

boost::json::value PluginManager::getPluginState(const std::string& pluginName)
{
    auto& registry = SessionManager::instance();
    auto sess = registry.findSession(pluginName, 0);
    if (sess) {
        std::string result = sess->call_plugin_process("{\"action\":\"get_state\"}");
        try {
            return boost::json::parse(result);
        } catch (...) {
            return boost::json::value(result);
        }
    }
    return boost::json::value(nullptr);
}

boost::json::array PluginManager::listPlugins()
{
    auto& registry = SessionManager::instance();
    auto sessions = registry.listSessions();
    boost::json::array result;
    for (auto& [name, idx] : sessions) {
        boost::json::object entry;
        entry["name"] = name;
        entry["instance"] = idx;
        result.push_back(std::move(entry));
    }
    return result;
}

uint64_t PluginManager::subscribe(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t id = nextSubId_++;
    subscribers_[id] = SubEntry{std::move(cb)};
    return id;
}

void PluginManager::unsubscribe(uint64_t handle)
{
    std::lock_guard<std::mutex> lock(mtx_);
    subscribers_.erase(handle);
}

void PluginManager::notifyStateChange(const std::string& pluginName, const boost::json::value& state)
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& [id, entry] : subscribers_) {
        try {
            entry.cb(pluginName, state);
        } catch (const std::exception& e) {
            APPMGR_LOG("error", "subscriber " << id << " threw: " << e.what());
        }
    }
}

void PluginManager::registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& pluginName)
{
    SessionManager::instance().registerWindow(windowId, sessionId, pluginName);
    APPMGR_LOG("info", "registered window " << windowId << " session " << sessionId << " plugin " << pluginName);
}

void PluginManager::unregisterWindow(const std::string& windowId)
{
    SessionManager::instance().unregisterWindow(windowId);
    APPMGR_LOG("info", "unregistered window " << windowId);
}

boost::json::array PluginManager::listActiveWindows()
{
    return SessionManager::instance().listActiveWindows();
}
