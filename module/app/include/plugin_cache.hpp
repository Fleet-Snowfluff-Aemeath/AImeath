#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <boost/dll/shared_library.hpp>
#include <boost/noncopyable.hpp>

#include "plugin.hpp"
#include "logger.hpp"

class PluginCache : private boost::noncopyable, public IPluginCache
{
public:
    static PluginCache& instance();
    static void setLogger(Logger& logger);

    PluginDescriptor load(const std::string& name) override;
    void evict(const std::string& name) override;
    void clear() override;

private:
    static boost::dll::shared_library tryLoad(const std::string& name);

    struct Entry {
        std::shared_ptr<boost::dll::shared_library> lib;
        PluginDescriptor mod;
        Entry(std::shared_ptr<boost::dll::shared_library> l, PluginDescriptor m)
            : lib(std::move(l)), mod(std::move(m)) {}
    };
    std::unordered_map<std::string, Entry> m_cache;
    std::mutex m_mtx;
};
