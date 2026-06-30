#pragma once

#include <string>
#include <boost/json.hpp>
#include <boost/noncopyable.hpp>

class Config : private boost::noncopyable
{
public:
    static Config& instance();

    int getInt(const std::string& key, int default_val = 0) const;
    std::string getString(const std::string& key, const std::string& default_val = "") const;

    // L1: typed convenience accessors keep callers clean
    //     and centralize default-value knowledge.
    int port() const { return getInt("port", 3001); }
    std::string deepSeekApiKey() const { return getString("deepseek_api_key"); }
    std::string gitToken() const { return getString("git_token"); }
    std::string fileRoot() const { return getString("file_root", "desktop/public/home"); }

private:
    Config();
    void load();

    boost::json::object root_;
};
