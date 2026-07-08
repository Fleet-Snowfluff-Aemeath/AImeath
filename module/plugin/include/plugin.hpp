#pragma once

#include <memory>
#include <string>
#include <cstdlib>
#include <boost/json.hpp>

// ---- 回调类型 ----

typedef void (*plugin_output_fn)(void* userdata, const char* json);

// ---- C ABI（.so 插件必须导出的符号，供 PluginCache 通过 dlsym 加载） ----

#ifdef __cplusplus
extern "C" {
#endif

void* plugin_create(const char* config_json);
void  plugin_destroy(void* plugin);
int   plugin_is_done(void* plugin);

void  plugin_on_input(void* plugin, const char* input_json);
void  plugin_set_output(void* plugin, plugin_output_fn cb, void* userdata);
void  plugin_set_io_context(void* plugin, void* io_context);

char* plugin_process(void* plugin, const char* input_json);
void  plugin_free_string(char* str);
char* plugin_get_info(void);

#ifdef __cplusplus
}
#endif

// ---- C++ 封装 ----

class PluginInstance
{
public:
    PluginInstance() = default;

    ~PluginInstance()
    {
        if (destroy_ && handle_) destroy_(handle_);
    }

    PluginInstance(PluginInstance&& other) noexcept
        : handle_(other.handle_)
        , async_(other.async_)
        , destroy_(other.destroy_)
        , is_done_(other.is_done_)
        , on_input_(other.on_input_)
        , set_output_(other.set_output_)
        , set_io_ctx_(other.set_io_ctx_)
        , process_(other.process_)
    {
        other.handle_ = nullptr;
    }

    PluginInstance& operator=(PluginInstance&& other) noexcept
    {
        if (this != &other)
        {
            if (destroy_ && handle_) destroy_(handle_);
            handle_ = other.handle_;
            async_ = other.async_;
            destroy_ = other.destroy_;
            is_done_ = other.is_done_;
            on_input_ = other.on_input_;
            set_output_ = other.set_output_;
            set_io_ctx_ = other.set_io_ctx_;
            process_ = other.process_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    PluginInstance(const PluginInstance&) = delete;
    PluginInstance& operator=(const PluginInstance&) = delete;

    explicit operator bool() const { return handle_ != nullptr; }

    std::string process(const std::string& input)
    {
        if (!handle_ || !process_) return "[]";
        char* out = process_(handle_, input.c_str());
        std::string result(out ? out : "[]");
        std::free(out);
        return result;
    }

    void onInput(const std::string& input)
    {
        if (handle_ && on_input_) on_input_(handle_, input.c_str());
    }

    void setOutput(plugin_output_fn cb, void* userdata)
    {
        if (handle_ && set_output_) set_output_(handle_, cb, userdata);
    }

    void setIoContext(void* ctx)
    {
        if (handle_ && set_io_ctx_) set_io_ctx_(handle_, ctx);
    }

    bool isDone() const
    {
        return is_done_ && is_done_(handle_) != 0;
    }

    bool isAsync() const { return async_; }

    boost::json::array processParsed(const std::string& msg)
    {
        if (!handle_ || !process_) return {};
        char* out = process_(handle_, msg.c_str());
        if (!out) return {};
        try {
            auto arr = boost::json::parse(out).as_array();
            std::free(out);
            return arr;
        } catch (...) {
            std::free(out);
            return {};
        }
    }

private:
    friend class PluginDescriptor;

    void* handle_ = nullptr;
    bool async_ = false;

    void  (*destroy_)(void*) = nullptr;
    int   (*is_done_)(void*) = nullptr;
    void  (*on_input_)(void*, const char*) = nullptr;
    void  (*set_output_)(void*, plugin_output_fn, void*) = nullptr;
    void  (*set_io_ctx_)(void*, void*) = nullptr;
    char* (*process_)(void*, const char*) = nullptr;
};

class PluginDescriptor
{
public:
    bool loaded() const { return loaded_; }
    bool isAsync() const { return async_; }
    explicit operator bool() const { return loaded(); }

    PluginInstance createInstance(const std::string& config) const
    {
        if (!create_ || !destroy_) return PluginInstance{};
        PluginInstance inst;
        inst.handle_ = create_(config.c_str());
        if (!inst.handle_) return PluginInstance{};
        inst.async_ = async_;
        inst.destroy_ = destroy_;
        inst.is_done_ = is_done_;
        inst.on_input_ = on_input_;
        inst.set_output_ = set_output_;
        inst.set_io_ctx_ = set_io_ctx_;
        inst.process_ = process_;
        return inst;
    }

private:
    friend class PluginCache;

    bool loaded_ = false;
    bool async_ = false;

    void* (*create_)(const char*) = nullptr;
    void  (*destroy_)(void*) = nullptr;
    int   (*is_done_)(void*) = nullptr;
    void  (*on_input_)(void*, const char*) = nullptr;
    void  (*set_output_)(void*, plugin_output_fn, void*) = nullptr;
    void  (*set_io_ctx_)(void*, void*) = nullptr;
    char* (*process_)(void*, const char*) = nullptr;
};

class IPluginCache
{
public:
    virtual ~IPluginCache() = default;
    virtual PluginDescriptor load(const std::string& name) = 0;
    virtual void evict(const std::string& name) = 0;
    virtual void clear() = 0;
};
