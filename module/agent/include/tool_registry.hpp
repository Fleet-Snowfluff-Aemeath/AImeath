#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <boost/json.hpp>
#include <yaml-cpp/yaml.h>

#include "plugin_cache.hpp"

namespace agent {

inline boost::json::array loadToolsFromYaml(const std::string& yamlPath) {
    boost::json::array tools;
    YAML::Node config = YAML::LoadFile(yamlPath);
    if (!config["tools"]) return tools;

    for (auto t : config["tools"]) {
        boost::json::object tool;
        tool["type"] = "function";

        boost::json::object func;
        func["name"] = t["name"].as<std::string>();
        func["description"] = t["description"].as<std::string>();

        boost::json::object params;
        params["type"] = "object";

        boost::json::object props;
        if (t["params"]["properties"]) {
            for (auto prop : t["params"]["properties"]) {
                boost::json::object p;
                p["type"] = prop.second["type"].as<std::string>();
                if (prop.second["description"])
                    p["description"] = prop.second["description"].as<std::string>();
                if (prop.second["items"]) {
                    boost::json::object items;
                    items["type"] = prop.second["items"]["type"].as<std::string>();
                    p["items"] = std::move(items);
                }
                props[prop.first.as<std::string>()] = std::move(p);
            }
        }
        params["properties"] = std::move(props);

        boost::json::array required;
        if (t["params"]["required"]) {
            for (auto r : t["params"]["required"])
                required.push_back(boost::json::string(r.as<std::string>()));
        }
        params["required"] = std::move(required);

        func["parameters"] = std::move(params);
        tool["function"] = std::move(func);
        tools.push_back(std::move(tool));
    }

    return tools;
}

class ToolRegistry {
public:
    static ToolRegistry& instance();

    void registerApp(const std::string& appName, const AppInfo& info);
    boost::json::value execute(const std::string& appName, const std::string& toolName, const boost::json::value& args);
    boost::json::array toolDefs() const;

    using ToolHandler = std::function<boost::json::value(const boost::json::value& args)>;
    void setHandler(const std::string& name, ToolHandler handler);
    boost::json::object buildToolDef(const std::string& name, const std::string& desc, const boost::json::object& params) const;

private:
    ToolRegistry() = default;
    std::unordered_map<std::string, AppInfo> appInfos_;
    std::unordered_map<std::string, ToolHandler> handlers_;
};

} // namespace agent
