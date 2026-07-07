#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <boost/dll/shared_library.hpp>
#include <boost/noncopyable.hpp>

#include "plugin.hpp"

class PluginCache : private boost::noncopyable, public IPluginCache
{
public:
    static PluginCache& instance();

    PluginDescriptor load(const std::string& name) override;
    void evict(const std::string& name) override;
    void clear() override;

private:
    static boost::dll::shared_library tryLoad(const std::string& name);

    struct Entry {
        boost::dll::shared_library lib;
        PluginDescriptor mod;
    };
    std::unordered_map<std::string, Entry> m_cache;
    std::mutex m_mtx;
};
