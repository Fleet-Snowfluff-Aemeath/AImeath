#include "app_manager.hpp"
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
        std::cerr << "[" << buf << "." << ms.count() << "] [appmgr] " << level << " " << msg << std::endl; \
    } while(0)

AppManager& AppManager::instance()
{
    static AppManager mgr;
    return mgr;
}

static void appStateNotifyBridge(const char* app, const char* state, void* ctx)
{
    auto* self = static_cast<AppManager*>(ctx);
    try {
        auto stateVal = boost::json::parse(state);
        if (stateVal.is_object()) {
            auto& obj = stateVal.as_object();
            auto overIt = obj.find("over");
            if (overIt != obj.end() && overIt->value().is_bool() && overIt->value().as_bool()) {
                auto widIt = obj.find("window_id");
                if (widIt != obj.end() && widIt->value().is_string())
                    self->unregisterWindow(std::string(widIt->value().as_string()));
            }
        }
        self->notifyStateChange(app, stateVal);
    } catch (...) {}
}

void AppManager::init(IModuleCache* cache)
{
    cache_ = cache;
    Config::instance().setAppStateNotifyFn(&appStateNotifyBridge, this);
    APPMGR_LOG("info", "AppManager initialized");
}

bool AppManager::openApp(const std::string& appName, const std::string& configJson)
{
    if (!cache_) {
        APPMGR_LOG("error", "AppManager not initialized");
        return false;
    }
    auto mod = cache_->load(appName);
    if (!mod) {
        APPMGR_LOG("error", "failed to load module: " << appName);
        return false;
    }
    APPMGR_LOG("info", "opened app: " << appName);
    return true;
}

bool AppManager::closeApp(const std::string& appName)
{
    APPMGR_LOG("info", "close app: " << appName);
    return true;
}

boost::json::value AppManager::controlApp(const std::string& appName, const std::string& commandJson)
{
    APPMGR_LOG("info", "control app: " << appName << " cmd: " << commandJson.substr(0, 80));

    auto& registry = SessionManager::instance();
    if (appName == appname::CHAT) {
        auto sessions = registry.findAllSessions(appName);
        if (sessions.empty()) {
            APPMGR_LOG("warn", "no active session for: " << appName);
            return boost::json::value(nullptr);
        }
        boost::json::array results;
        for (auto& sess : sessions) {
            std::string r = sess->call_app_process(commandJson);
            try {
                results.push_back(boost::json::parse(r));
            } catch (...) {
                results.push_back(boost::json::value(r));
            }
        }
        return results;
    }
    auto sess = registry.findSession(appName, 0);
    if (sess) {
        std::string result = sess->call_app_process(commandJson);
        try {
            return boost::json::parse(result);
        } catch (...) {
            return boost::json::value(result);
        }
    }

    APPMGR_LOG("warn", "no active session for: " << appName);
    return boost::json::value(nullptr);
}

boost::json::value AppManager::getAppState(const std::string& appName)
{
    auto& registry = SessionManager::instance();
    auto sess = registry.findSession(appName, 0);
    if (sess) {
        std::string result = sess->call_app_process("{\"action\":\"get_state\"}");
        try {
            return boost::json::parse(result);
        } catch (...) {
            return boost::json::value(result);
        }
    }
    return boost::json::value(nullptr);
}

boost::json::array AppManager::listApps()
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

uint64_t AppManager::subscribe(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t id = nextSubId_++;
    subscribers_[id] = SubEntry{std::move(cb)};
    return id;
}

void AppManager::unsubscribe(uint64_t handle)
{
    std::lock_guard<std::mutex> lock(mtx_);
    subscribers_.erase(handle);
}

void AppManager::notifyStateChange(const std::string& appName, const boost::json::value& state)
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& [id, entry] : subscribers_) {
        try {
            entry.cb(appName, state);
        } catch (const std::exception& e) {
            APPMGR_LOG("error", "subscriber " << id << " threw: " << e.what());
        }
    }
}

void AppManager::registerWindow(const std::string& windowId, const std::string& sessionId, const std::string& appName)
{
    SessionManager::instance().registerWindow(windowId, sessionId, appName);
    APPMGR_LOG("info", "registered window " << windowId << " session " << sessionId << " app " << appName);
}

void AppManager::unregisterWindow(const std::string& windowId)
{
    SessionManager::instance().unregisterWindow(windowId);
    APPMGR_LOG("info", "unregistered window " << windowId);
}

boost::json::array AppManager::listActiveWindows()
{
    return SessionManager::instance().listActiveWindows();
}
