#pragma once

#include <string>
#include <boost/json.hpp>

// ---- URL parsing ----

struct ParsedUrl
{
    std::string host;
    int port = 80;
    std::string path = "/";
    std::string query;
    bool is_ws = false;
};

ParsedUrl parseUrl(const std::string& url);

// ---- JSON helpers ----

std::string jsonError(const std::string& msg);
std::string jsonOk();
std::string jsonParseStr(const std::string& msg, const std::string& key);
int jsonParseInt(const std::string& msg, const std::string& key);

std::string jsonParseStr(const boost::json::value& val, const std::string& key);
int jsonParseInt(const boost::json::value& val, const std::string& key);
