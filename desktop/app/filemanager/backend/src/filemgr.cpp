// filemgr.cpp — 文件管理器 C ABI 后端
// 提供目录列表/文件读写等操作，委托给 module/core 的 VirtualFileSystem

#include "app_api.hpp"
#include "filemgr.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>

#include <boost/json.hpp>

#include "vfs.hpp"
#include "config.hpp"

namespace json = boost::json;

static VirtualFileSystem& vfs()
{
    static VirtualFileSystem instance(Config::instance().fileRoot());
    return instance;
}

static json::array listAction(const std::string& path)
{
    auto entries = vfs().listDir(path);
    if (entries.empty())
    {
        json::array a;
        a.push_back(json::object{{"type","error"},{"msg","invalid path or empty directory"}});
        return a;
    }

    json::array items;
    for (auto& e : entries)
    {
        json::object obj;
        obj["name"]     = std::move(e.name);
        obj["kind"]     = std::move(e.kind);
        obj["size"]     = static_cast<std::int64_t>(e.size);
        obj["modified"] = std::move(e.modified);
        obj["url"]      = std::move(e.url);
        items.push_back(std::move(obj));
    }

    json::array result;
    result.push_back(json::object{
        {"type", "listing"},
        {"path", path},
        {"entries", std::move(items)}
    });
    return result;
}

static json::array readAction(const std::string& path)
{
    std::string content = vfs().readFile(path);
    if (content.empty())
    {
        json::array a;
        a.push_back(json::object{{"type","error"},{"msg","cannot read file: " + path}});
        return a;
    }

    json::array result;
    result.push_back(json::object{
        {"type", "file"},
        {"path", path},
        {"content", std::move(content)}
    });
    return result;
}

static json::array mkdirAction(const std::string& path)
{
    bool ok = vfs().mkdir(path);
    json::array a;
    a.push_back(json::object{
        {"type", ok ? "ok" : "error"},
        {"msg", ok ? "created" : "cannot create directory"}
    });
    return a;
}

static json::array writeAction(const std::string& path, const std::string& content)
{
    vfs().writeFile(path, content);
    json::array a;
    a.push_back(json::object{
        {"type", "ok"},
        {"msg", "written"}
    });
    return a;
}

static json::array removeAction(const std::string& path)
{
    bool ok = vfs().remove(path);
    json::array a;
    a.push_back(json::object{
        {"type", ok ? "ok" : "error"},
        {"msg", ok ? "removed" : "cannot remove"}
    });
    return a;
}

extern "C"
{

void* app_create(const char*)                  { return new int(0); }
void  app_destroy(void* p)                     { delete static_cast<int*>(p); }
void  app_free_string(char* s)                 { std::free(s); }
int   app_is_done(void*)                       { return 0; }

char* app_process(void*, const char* input_json)
{
    auto makeReply = [](json::array arr) -> char*
    {
        auto s = json::serialize(arr);
        auto* buf = static_cast<char*>(std::malloc(s.size() + 1));
        if (buf) std::memcpy(buf, s.data(), s.size() + 1);
        return buf;
    };

    try
    {
        auto val = json::parse(input_json);
        if (!val.is_object())
            return makeReply(json::array{json::object{{"type","error"},{"msg","not an object"}}});

        auto& obj = val.as_object();

        auto it = obj.find("action");
        if (it == obj.end() || !it->value().is_string())
            return makeReply(json::array{json::object{{"type","error"},{"msg","missing action"}}});

        std::string action(it->value().as_string());
        it = obj.find("path");
        std::string path = (it != obj.end() && it->value().is_string())
                           ? std::string(it->value().as_string()) : "/";

        if (action == "list")
            return makeReply(listAction(path));

        if (action == "read")
            return makeReply(readAction(path));

        if (action == "mkdir")
            return makeReply(mkdirAction(path));

        if (action == "write")
        {
            auto cit = obj.find("content");
            std::string content = (cit != obj.end() && cit->value().is_string())
                                  ? std::string(cit->value().as_string()) : "";
            return makeReply(writeAction(path, content));
        }

        if (action == "remove")
            return makeReply(removeAction(path));

        return makeReply(json::array{json::object{{"type","error"},{"msg","unknown action: "+action}}});
    }
    catch (const std::exception& e)
    {
        return makeReply(json::array{json::object{{"type","error"},{"msg",std::string(e.what())}}});
    }
}

} // extern "C"
