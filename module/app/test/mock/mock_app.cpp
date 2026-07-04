#include "app_api.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

struct MockApp
{
    std::string config;
    bool done = false;
};

void* app_create(const char* config)
{
    auto* a = new MockApp;
    if (config) a->config = config;
    return a;
}

void app_destroy(void* p)
{
    delete static_cast<MockApp*>(p);
}

char* app_process(void* p, const char* input)
{
    auto* a = static_cast<MockApp*>(p);
    std::string out = R"([{"type":"mock","config":")" + a->config + R"(","input":")" + input + R"("}])";
    char* buf = static_cast<char*>(std::malloc(out.size() + 1));
    if (buf) std::memcpy(buf, out.data(), out.size() + 1);
    return buf;
}

void app_free_string(char* str) { std::free(str); }

int app_is_done(void* p)
{
    return static_cast<MockApp*>(p)->done ? 1 : 0;
}
