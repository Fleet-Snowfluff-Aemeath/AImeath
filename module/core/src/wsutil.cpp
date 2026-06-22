#include "wsutil.hpp"
#include <boost/url.hpp>

ParsedUrl parseUrl(const std::string& url)
{
    ParsedUrl result;
    auto r = boost::urls::parse_uri(url);
    if (!r) return result;

    auto& u = *r;
    auto scheme = u.scheme();
    if (scheme == "ws" || scheme == "wss")
        result.is_ws = true;

    if (scheme == "https" || scheme == "wss")
        result.port = 443;

    if (!u.host().empty())
        result.host = static_cast<std::string>(u.host());
    auto pnum = u.port_number();
    if (pnum > 0)
        result.port = static_cast<int>(pnum);
    if (!u.path().empty())
        result.path = static_cast<std::string>(u.path());
    if (u.has_query())
        result.query = static_cast<std::string>(u.query());

    return result;
}

std::string jsonError(const std::string& msg)
{
    boost::json::object obj;
    obj["type"] = "error";
    obj["msg"]  = msg;
    return boost::json::serialize(obj);
}

std::string jsonOk()
{
    return boost::json::serialize(boost::json::object{{"type", "ok"}});
}

static std::string valueToString(const boost::json::value& v)
{
    if (v.is_string())
        return static_cast<std::string>(v.as_string());
    if (v.is_int64() || v.is_uint64() || v.is_double() || v.is_bool())
        return boost::json::serialize(v);
    return {};
}

std::string jsonParseStr(const boost::json::value& val, const std::string& key)
{
    if (!val.is_object()) return {};
    auto& obj = val.as_object();
    auto it = obj.find(key);
    if (it == obj.end()) return {};
    return valueToString(it->value());
}

int jsonParseInt(const boost::json::value& val, const std::string& key)
{
    if (!val.is_object()) return 0;
    auto& obj = val.as_object();
    auto it = obj.find(key);
    if (it == obj.end()) return 0;
    auto r = boost::json::try_value_to<std::int64_t>(it->value());
    return r ? static_cast<int>(*r) : 0;
}

std::string jsonParseStr(const std::string& msg, const std::string& key)
{
    try
    {
        auto val = boost::json::parse(msg);
        return jsonParseStr(val, key);
    }
    catch (...)
    {
        return {};
    }
}

int jsonParseInt(const std::string& msg, const std::string& key)
{
    try
    {
        auto val = boost::json::parse(msg);
        return jsonParseInt(val, key);
    }
    catch (...)
    {
        return 0;
    }
}
