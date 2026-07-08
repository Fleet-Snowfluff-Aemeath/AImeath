#include "app_manager.hpp"
#include "config.hpp"
#include "ws_server.hpp"

AppManager& AppManager::instance()
{
    static AppManager mgr;
    return mgr;
}

void AppManager::init(IPluginCache* cache, Logger* logger)
{
    cache_ = cache;
    logger_ = logger;
    appStateSub_ = appEventBus().subscribe<AppStateEvent>(
        [this](const AppStateEvent& e) {
            try {
                auto& stateVal = e.state;
                if (stateVal.is_object()) {
                    auto& obj = stateVal.as_object();
                    auto overIt = obj.find("over");
                    if (overIt != obj.end() && overIt->value().is_bool() && overIt->value().as_bool()) {
                        auto widIt = obj.find("window_id");
                        if (widIt != obj.end() && widIt->value().is_string())
                            SessionManager::instance().unregisterWindow(std::string(widIt->value().as_string()));
                    }
                }
                notifyStateChange(e.appName, stateVal);
            } catch (...) {}
        });
    if (logger_) logger_->info() << "AppManager initialized";
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
            if (logger_) logger_->error() << "subscriber " << id << " threw: " << e.what();
        }
    }
}
