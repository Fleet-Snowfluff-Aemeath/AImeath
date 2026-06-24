#include "app_api.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <boost/json.hpp>

struct GameAppAdapter
{
    void* game;
    bool done;
};

extern "C" void* game_new(const char* config);
extern "C" void  game_destroy(void* game);
extern "C" int   game_tick(void* game, const char* input, char** output);
extern "C" int   game_get_state(void* game, char** output);

void* app_create(const char* config_json)
{
    auto* adapter = new GameAppAdapter;
    adapter->done = false;
    try
    {
        auto val = boost::json::parse(config_json ? config_json : "{}");
        std::string config;
        if (val.is_object())
        {
            auto it = val.as_object().find("game_config");
            if (it != val.as_object().end() && it->value().is_string())
                config = static_cast<std::string>(it->value().as_string());
            else
                config = boost::json::serialize(val);
        }
        adapter->game = game_new(config.c_str());
        if (!adapter->game)
        {
            delete adapter;
            return nullptr;
        }
    }
    catch (...)
    {
        adapter->game = game_new("{}");
        if (!adapter->game)
        {
            delete adapter;
            return nullptr;
        }
    }
    return adapter;
}

void app_destroy(void* p)
{
    auto* adapter = static_cast<GameAppAdapter*>(p);
    if (adapter->game)
        game_destroy(adapter->game);
    delete adapter;
}

char* app_process(void* p, const char* input_json)
{
    auto* adapter = static_cast<GameAppAdapter*>(p);
    if (!adapter->game) return nullptr;

    char* raw_output = nullptr;
    int result = game_tick(adapter->game, input_json, &raw_output);

    if (result != 0)
        adapter->done = true;

    if (!raw_output)
    {
        char* state = nullptr;
        game_get_state(adapter->game, &state);
        if (state)
        {
            std::string wrapped = R"([{"type":"game_state","data":)" + std::string(state) + "}]";
            char* buf = static_cast<char*>(std::malloc(wrapped.size() + 1));
            if (buf) std::memcpy(buf, wrapped.data(), wrapped.size() + 1);
            std::free(state);
            return buf;
        }
        return nullptr;
    }

    return raw_output;
}

void app_free_string(char* str)
{
    std::free(str);
}

int app_is_done(void* p)
{
    return static_cast<GameAppAdapter*>(p)->done ? 1 : 0;
}
