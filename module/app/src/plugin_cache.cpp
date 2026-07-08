#include "plugin_cache.hpp"
#include <dlfcn.h>
#include <boost/dll.hpp>
#include <boost/json.hpp>

static Logger* s_pluginLogger = nullptr;

PluginCache& PluginCache::instance()
{
    static PluginCache cache;
    return cache;
}

void PluginCache::setLogger(Logger& logger)
{
    s_pluginLogger = &logger;
}

boost::dll::shared_library PluginCache::tryLoad(const std::string& name)
{
    std::string soname = "lib" + name + ".so";

    auto try_one = [&](const std::string& path) -> boost::dll::shared_library {
        boost::system::error_code ec;
        boost::dll::shared_library lib(path, ec);
        if (!ec) return lib;
        if (ec.value() != boost::system::errc::no_such_file_or_directory)
            if (s_pluginLogger) s_pluginLogger->debug("plugin") << "tryLoad " << path << ": " << ec.message();
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

    if (s_pluginLogger) s_pluginLogger->debug("plugin") << "tryLoad failed for " << soname;
    return {};
}

PluginDescriptor PluginCache::load(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_cache.find(name);
    if (it != m_cache.end())
    {
        if (s_pluginLogger) s_pluginLogger->debug("plugin") << "cache hit: " << name;
        it->second.mod.loaded_ = true;
        return it->second.mod;
    }

    if (s_pluginLogger) s_pluginLogger->debug("plugin") << "loading: " << name;
    auto lib = tryLoad(name);
    if (!lib)
    {
        if (s_pluginLogger) s_pluginLogger->error("plugin") << "failed to load lib" << name << ".so";
        return {};
    }

    PluginDescriptor m;
    m.loaded_ = true;

    try
    {
        m.create_     = lib.get<void*(const char*)>("app_create");
        m.destroy_    = lib.get<void(void*)>("app_destroy");
        m.process_    = lib.get<char*(void*,const char*)>("app_process");
        m.is_done_    = lib.get<int(void*)>("app_is_done");
    }
    catch (const boost::system::system_error& e)
    {
        if (s_pluginLogger) s_pluginLogger->error("plugin") << "symbols not found in lib" << name << ".so: " << e.what();
        return {};
    }

    if (!m.create_ || !m.destroy_ || !m.process_ || !m.is_done_)
    {
        if (s_pluginLogger) s_pluginLogger->error("plugin") << "required symbols missing in lib" << name << ".so";
        return {};
    }

    try {
        m.on_input_       = lib.get<void(void*,const char*)>("app_on_input");
        m.set_output_     = lib.get<void(void*,app_output_fn,void*)>("app_set_output");
        m.set_io_ctx_     = lib.get<void(void*,void*)>("app_set_io_context");
    } catch (...) {
    }

    m.async_ = (m.on_input_ && m.set_output_);

    if (s_pluginLogger) s_pluginLogger->info("plugin") << name << " loaded (async=" << m.async_ << ")";

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
