#include "app_mod.hpp"
#include <dlfcn.h>
#include <iostream>
#include <boost/dll.hpp>

static boost::dll::shared_library try_load(const std::string& name)
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

AppModule AppModuleCache::load(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_cache.find(name);
    if (it != m_cache.end())
    {
        it->second.mod.loaded = true;
        return it->second.mod;
    }

    auto lib = try_load(name);
    if (!lib)
    {
        std::cerr << "failed to load lib" << name << ".so" << std::endl;
        return {};
    }

    AppModule m;
    m.loaded = true;

    try
    {
        m.app_create      = lib.get<void*(const char*)>("app_create");
        m.app_destroy     = lib.get<void(void*)>("app_destroy");
        m.app_process     = lib.get<char*(void*,const char*)>("app_process");
        m.app_free_string = lib.get<void(char*)>("app_free_string");
        m.app_is_done     = lib.get<int(void*)>("app_is_done");
    }
    catch (const boost::system::system_error& e)
    {
        std::cerr << "symbols not found in lib" << name << ".so: " << e.what() << std::endl;
        return {};
    }

    if (!m.app_create || !m.app_destroy || !m.app_process || !m.app_free_string || !m.app_is_done)
    {
        std::cerr << "required symbols not found in lib" << name << ".so" << std::endl;
        return {};
    }

    try {
        m.app_on_input        = lib.get<void(void*,const char*)>("app_on_input");
        m.app_set_output      = lib.get<void(void*,app_output_fn,void*)>("app_set_output");
        m.app_set_io_context  = lib.get<void(void*,void*)>("app_set_io_context");
    } catch (...) {
    }

    m_cache[name] = {std::move(lib), m};
    return m;
}

void AppModuleCache::evict(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_cache.erase(name);
}

void AppModuleCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_cache.clear();
}
