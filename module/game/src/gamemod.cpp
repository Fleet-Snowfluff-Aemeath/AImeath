#include "gamemod.hpp"
#include <dlfcn.h>
#include <iostream>

GameModule GameModuleCache::load(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_cache.find(name);
    if (it != m_cache.end())
        return it->second;

    std::string soname = "lib" + name + ".so";
    void* handle = dlopen(soname.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle)
    {
        std::cerr << "dlopen " << soname << ": " << dlerror() << std::endl;
        return {};
    }

    GameModule m;
    m.lib = boost::dll::shared_library(handle);

    try
    {
        m.game_new  = m.lib.get<void*(int, int)>("game_new");
        m.game_free = m.lib.get<void(void*)>("game_free");
    }
    catch (const boost::system::system_error& e)
    {
        std::cerr << "symbols not found in " << soname << ": " << e.what() << std::endl;
        return {};
    }

    if (!m.game_new || !m.game_free)
    {
        std::cerr << "symbols not found in " << soname << std::endl;
        return {};
    }

    m_cache[name] = m;
    return m;
}

void GameModuleCache::evict(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_cache.erase(name);
}

void GameModuleCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_cache.clear();
}