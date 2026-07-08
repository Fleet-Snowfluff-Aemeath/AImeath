#include "plugin_cache.hpp"
#include <dlfcn.h>
#include <iostream>
#include <boost/dll.hpp>

PluginCache& PluginCache::instance()
{
    static PluginCache cache;
    return cache;
}

boost::dll::shared_library PluginCache::tryLoad(const std::string& name)
{
    std::string soname = "lib" + name + ".so";

    auto try_one = [&](const std::string& path) -> boost::dll::shared_library {
        boost::system::error_code ec;
        boost::dll::shared_library lib(path, ec);
        if (!ec) return lib;
        return {};
    };

    auto lib = try_one(soname);
    if (lib) return lib;

    boost::system::error_code ec;
    boost::filesystem::path exe = boost::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) return {};

    boost::filesystem::path exe_dir = exe.parent_path();

    struct { boost::filesystem::path base; const char* suffix; } fallbacks[] = {
        { exe_dir,                       "lib" },
        { exe_dir.parent_path(),         "output/lib" },
        { exe_dir,                       "output/lib" },
        { exe_dir.parent_path(),         "lib" },
    };

    for (auto& fb : fallbacks) {
        boost::filesystem::path full = fb.base / fb.suffix / soname;
        lib = try_one(full.string());
        if (lib) return lib;
    }

    return {};
}

PluginDescriptor PluginCache::load(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_cache.find(name);
    if (it != m_cache.end())
    {
        it->second.mod.loaded_ = true;
        return it->second.mod;
    }

    auto lib = tryLoad(name);
    if (!lib)
    {
        std::cerr << "failed to load lib" << name << ".so" << std::endl;
        return {};
    }

    PluginDescriptor m;
    m.loaded_ = true;

    try
    {
        m.create_     = lib.get<void*(const char*)>("plugin_create");
        m.destroy_    = lib.get<void(void*)>("plugin_destroy");
        m.process_    = lib.get<char*(void*,const char*)>("plugin_process");
        m.is_done_    = lib.get<int(void*)>("plugin_is_done");
    }
    catch (const boost::system::system_error& e)
    {
        std::cerr << "symbols not found in lib" << name << ".so: " << e.what() << std::endl;
        return {};
    }

    if (!m.create_ || !m.destroy_ || !m.process_ || !m.is_done_)
    {
        std::cerr << "required symbols not found in lib" << name << ".so" << std::endl;
        return {};
    }

    try {
        m.on_input_       = lib.get<void(void*,const char*)>("plugin_on_input");
        m.set_output_     = lib.get<void(void*,plugin_output_fn,void*)>("plugin_set_output");
        m.set_io_ctx_     = lib.get<void(void*,void*)>("plugin_set_io_context");
    } catch (...) {
    }

    m.async_ = (m.on_input_ && m.set_output_);

    m_cache.emplace(std::piecewise_construct,
                    std::forward_as_tuple(name),
                    std::forward_as_tuple(std::make_shared<boost::dll::shared_library>(std::move(lib)), m));
    return m;
}

void PluginCache::evict(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_cache.erase(name);
}

void PluginCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_cache.clear();
}
