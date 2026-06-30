#include "config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

Config& Config::instance()
{
    // C++11 function-static — thread-safe per standard
    static Config cfg;
    return cfg;
}

Config::Config()
{
    load();
}

void Config::load()
{
    // L1: silent on failure — caller decides whether to warn/fallback.
    //     Both main.cpp and chat_server.cpp previously printed warnings;
    //     that responsibility stays at the call site.
    std::ifstream f("config.json");
    if (!f.is_open())
        return;

    std::string content(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    try {
        auto val = boost::json::parse(content);
        root_ = val.as_object();
    } catch (const std::exception& e) {
        std::cerr << "Config: failed to parse config.json: " << e.what() << "\n";
    }
}

int Config::getInt(const std::string& key, int default_val) const
{
    // L1: iterating root_ on every call trades a tiny perf cost
    //     for simple, obvious semantics. config.json is tiny (<10 fields),
    //     so the O(n) scan is noise.
    auto it = root_.find(key);
    if (it == root_.end())
        return default_val;
    if (!it->value().is_int64())
        return default_val;
    return it->value().as_int64();
}

std::string Config::getString(const std::string& key, const std::string& default_val) const
{
    auto it = root_.find(key);
    if (it == root_.end())
        return default_val;
    if (!it->value().is_string())
        return default_val;
    return it->value().as_string().c_str();
}
