#pragma once

#include "game_base.hpp"
#include "plugin.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <boost/json.hpp>

/// Place in each game module's .cpp before calling APP_GAME_API_COMMON():
///   #define GAME_CLASS SnakeGame
///
/// If the game constructor takes custom arguments, also define:
///   #define GAME_CONSTRUCT(w, h) new GAME_CLASS(w)
///
/// Then call this macro once in the .cpp to generate the unified
/// app_create / app_destroy / app_process / app_free_string / app_is_done
/// C ABI on top of the Game base class (tick / isOver / score / getState).
///
/// Note: includes plugin.hpp so game modules can also include this header
/// for the macro definition.  GAME_CLASS is checked at macro-expansion time.

static std::string _gameJsonGetStr(const std::string& msg, const std::string& key)
{
    try {
        auto val = boost::json::parse(msg);
        if (!val.is_object()) return {};
        auto& obj = val.as_object();
        auto it = obj.find(key);
        if (it == obj.end()) return {};
        auto& v = it->value();
        return v.is_string() ? std::string(v.as_string()) :
               v.is_int64() || v.is_uint64() || v.is_double() || v.is_bool()
                    ? boost::json::serialize(v) : std::string();
    } catch (...) { return {}; }
}

static int _gameJsonGetInt(const std::string& msg, const std::string& key)
{
    try {
        auto val = boost::json::parse(msg);
        if (!val.is_object()) return 0;
        auto& obj = val.as_object();
        auto it = obj.find(key);
        if (it == obj.end()) return 0;
        auto r = boost::json::try_value_to<std::int64_t>(it->value());
        return r ? static_cast<int>(*r) : 0;
    } catch (...) { return 0; }
}

static std::string _gameJsonError(const std::string& msg)
{
    boost::json::object obj;
    obj["type"] = "error";
    obj["msg"]  = msg;
    return boost::json::serialize(obj);
}

static std::string _gameJsonOk()
{
    return boost::json::serialize(boost::json::object{{"type", "ok"}});
}

static std::string _gameBuildArray(const boost::json::value& item)
{
    boost::json::array arr;
    arr.push_back(item);
    return boost::json::serialize(arr);
}

static char* _gameStrdup(const std::string& s)
{
    char* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (buf) std::memcpy(buf, s.data(), s.size() + 1);
    return buf;
}

// ---- _GameAppCtx: owns a Game* instance ----

struct _GameAppCtx
{
    Game* game = nullptr;
    int width = 0, height = 0;
    bool paused = false;
    bool done = false;
};

#ifndef GAME_CONSTRUCT
#define GAME_CONSTRUCT(w, h) new GAME_CLASS(w, h)
#endif

#ifndef GAME_NAME
#define GAME_NAME "unknown"
#endif
#ifndef GAME_DISPLAY_NAME
#define GAME_DISPLAY_NAME GAME_NAME
#endif

// ---- Unified app_* C ABI (call once per game .cpp) ----

#define APP_GAME_API_COMMON()                                      \
extern "C" {                                                       \
                                                                   \
void* app_create(const char* /*config_json*/)                      \
{                                                                  \
    return new _GameAppCtx();                                      \
}                                                                  \
                                                                   \
void app_destroy(void* p)                                          \
{                                                                  \
    auto* ctx = static_cast<_GameAppCtx*>(p);                      \
    if (ctx->game) { delete ctx->game; ctx->game = nullptr; }      \
    delete ctx;                                                    \
}                                                                  \
                                                                   \
char* app_process(void* p, const char* input_json)                 \
{                                                                  \
    auto* ctx = static_cast<_GameAppCtx*>(p);                      \
    std::string msg(input_json);                                   \
                                                                   \
    std::string action = _gameJsonGetStr(msg, "action");           \
    if (action.empty()) action = _gameJsonGetStr(msg, "type");     \
                                                                   \
    if (action == "start" || action == "new_game")                 \
    {                                                              \
        if (ctx->game)                                             \
            return _gameStrdup(_gameBuildArray(                    \
                boost::json::parse(_gameJsonError("game already running"))));\
                                                                   \
        int w = _gameJsonGetInt(msg, "width");                     \
        if (w <= 0) w = 20;                                       \
        int h = _gameJsonGetInt(msg, "height");                    \
        if (h <= 0) h = 20;                                       \
        ctx->width = w; ctx->height = h;                           \
                                                                   \
        ctx->game = GAME_CONSTRUCT(w, h);                     \
        ctx->paused = false;                                       \
        ctx->done = false;                                         \
                                                                   \
        if (!ctx->game)                                            \
            return _gameStrdup(_gameBuildArray(                    \
                boost::json::parse(_gameJsonError("game_new failed"))));\
                                                                   \
        std::string state = ctx->game->getState();                  \
        return _gameStrdup(_gameBuildArray(boost::json::parse(state)));\
    }                                                              \
    else if (action == "tick" || action == "input")                \
    {                                                              \
        if (!ctx->game)                                            \
            return _gameStrdup(_gameBuildArray(                    \
                boost::json::parse(_gameJsonError("no game"))));  \
        if (ctx->paused)                                           \
            return _gameStrdup("[]");                              \
                                                                   \
        int val = _gameJsonGetInt(msg, "value");                   \
        ctx->game->tick(val);                                      \
        std::string state = ctx->game->getState();                  \
        return _gameStrdup(_gameBuildArray(boost::json::parse(state)));\
    }                                                              \
    else if (action == "pause")                                    \
    {                                                              \
        ctx->paused = true;                                        \
        return _gameStrdup(_gameBuildArray(                        \
            boost::json::parse(_gameJsonOk())));                   \
    }                                                              \
    else if (action == "resume")                                   \
    {                                                              \
        ctx->paused = false;                                       \
        return _gameStrdup(_gameBuildArray(                        \
            boost::json::parse(_gameJsonOk())));                   \
    }                                                              \
    else if (action == "end_game")                                 \
    {                                                              \
        ctx->done = true;                                          \
        if (ctx->game) { delete ctx->game; ctx->game = nullptr; }  \
        return _gameStrdup(_gameBuildArray(                        \
            boost::json::parse(_gameJsonOk())));                   \
    }                                                              \
    else if (action == "get_state")                                \
    {                                                              \
        if (!ctx->game)                                            \
            return _gameStrdup(_gameBuildArray(                    \
                boost::json::parse(_gameJsonError("no game"))));  \
        std::string state = ctx->game->getState();                 \
        return _gameStrdup(_gameBuildArray(                        \
            boost::json::parse(state)));                            \
    }                                                              \
    else                                                           \
    {                                                              \
        return _gameStrdup(_gameBuildArray(                        \
            boost::json::parse(_gameJsonError(                     \
                "unknown action: " + action))));                   \
    }                                                              \
}                                                                  \
                                                                   \
void app_free_string(char* str)                                    \
{                                                                  \
    std::free(str);                                                \
}                                                                  \
                                                                   \
int app_is_done(void* p)                                           \
{                                                                  \
    return static_cast<_GameAppCtx*>(p)->done ? 1 : 0;             \
}                                                                  \
                                                                    \
char* app_get_info(void)                                           \
{                                                                  \
    const char* json =                                              \
        "{\"name\":\"" GAME_NAME "\","                             \
        "\"display_name\":\"" GAME_DISPLAY_NAME "\","              \
        "\"type\":\"game\","                                       \
        "\"tools\":["                                              \
        "{\"name\":\"control_app\","                               \
        "\"description\":\"Control the game\","                     \
        "\"handler\":\"game_tick\","                               \
        "\"params\":{\"type\":\"object\",\"properties\":{"         \
        "\"value\":{\"type\":\"integer\",\"description\":\"Direction: 0=up 1=down 2=left 3=right\"}"\
        "},\"required\":[\"value\"]}},"                             \
        "{\"name\":\"get_app_state\","                              \
        "\"description\":\"Get current game state\","               \
        "\"handler\":\"game_state\","                               \
        "\"params\":{\"type\":\"object\",\"properties\":{}}}"       \
        "]}";                                                       \
    return _gameStrdup(json);                                       \
}                                                                  \
                                                                    \
} /* extern "C" */
