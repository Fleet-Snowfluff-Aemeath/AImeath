#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <boost/dll/shared_library.hpp>
#include <boost/noncopyable.hpp>

#include "app_api.hpp"
#include <iface_mod.hpp>

class AppModuleCache : private boost::noncopyable, public IModuleCache
{
public:
    static AppModuleCache& instance();

    AppModule load(const std::string& name) override;
    void evict(const std::string& name) override;
    void clear() override;

private:
    struct Entry {
        boost::dll::shared_library lib;
        AppModule mod;
    };
    std::unordered_map<std::string, Entry> m_cache;
    std::mutex m_mtx;
};
