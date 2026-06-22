#pragma once

#include "game_base.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

/// Place in each module's header to declare the 6 C-API functions.
#define GAME_API_DECL()            \
extern "C" {                       \
    void* game_new(int w, int h);  \
    void  game_free(void* g);      \
    void  game_tick(void* g, int a); \
    int   game_is_over(void* g);   \
    int   game_get_score(void* g); \
    char* game_get_state(void* g); \
}

/// Place in each module's .cpp to define the 5 common C-API functions.
/// game_new() must be defined separately per module.
#define GAME_API_COMMON()                                               \
void  game_free(void* g)                                                \
    { delete static_cast<Game*>(g); }                                   \
void  game_tick(void* g, int a)                                         \
    { static_cast<Game*>(g)->tick(a); }                                 \
int   game_is_over(void* g)                                             \
    { return static_cast<Game*>(g)->isOver(); }                         \
int   game_get_score(void* g)                                           \
    { return static_cast<Game*>(g)->score(); }                          \
char* game_get_state(void* g)                                           \
{                                                                       \
    std::string s = static_cast<Game*>(g)->getState();                  \
    char* buf = static_cast<char*>(std::malloc(s.size() + 1));         \
    if (buf) { std::memcpy(buf, s.data(), s.size() + 1); }             \
    return buf;                                                         \
}
