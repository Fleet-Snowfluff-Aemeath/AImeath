#include "plugin_module.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

struct MockPlugin
{
    std::string config;
    bool done = false;
};

void* plugin_create(const char* config)
{
    auto* a = new MockPlugin;
    if (config) a->config = config;
    return a;
}

void plugin_destroy(void* p)
{
    delete static_cast<MockPlugin*>(p);
}

char* plugin_process(void* p, const char* input)
{
    auto* a = static_cast<MockPlugin*>(p);
    std::string out = R"([{"type":"mock","config":")" + a->config + R"(","input":")" + input + R"("}])";
    char* buf = static_cast<char*>(std::malloc(out.size() + 1));
    if (buf) std::memcpy(buf, out.data(), out.size() + 1);
    return buf;
}

void plugin_free_string(char* str) { std::free(str); }

int plugin_is_done(void* p)
{
    return static_cast<MockPlugin*>(p)->done ? 1 : 0;
}
