#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <boost/dll/shared_library.hpp>
#include <boost/noncopyable.hpp>
#include "game_base.hpp"

struct GameDeleter
{
    void (*deleter)(void*) = nullptr;
    void operator()(Game* g) const { if (deleter) deleter(g); }
};

using GamePtr = std::unique_ptr<Game, GameDeleter>;

struct GameModule
{
    boost::dll::shared_library lib;
    void* (*game_new)(int, int) = nullptr;
    void  (*game_free)(void*) = nullptr;

    explicit operator bool() const { return lib.is_loaded(); }

    GamePtr create(int w, int h) const
    {
        if (!game_new || !game_free) return {nullptr, GameDeleter{nullptr}};
        return {static_cast<Game*>(game_new(w, h)), GameDeleter{game_free}};
    }
};

/// Thread-safe cache for loaded game modules.
class GameModuleCache : private boost::noncopyable
{
public:
    GameModule load(const std::string& name);
    void evict(const std::string& name);
    void clear();

private:
    std::unordered_map<std::string, GameModule> m_cache;
    std::mutex m_mtx;
};