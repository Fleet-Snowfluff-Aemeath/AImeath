#pragma once

#include <boost/json.hpp>
#include <string>
#include <map>
#include <vector>
#include "llm_client.hpp"

namespace llm {

inline std::string build_chat_body(const boost::json::array& messages,
                                   const std::string& model = "deepseek-v4-flash",
                                   bool stream = true,
                                   bool thinking = true,
                                   double temperature = 0.7,
                                   int max_tokens = 4096)
{
    boost::json::object body;
    body["model"] = model;
    body["messages"] = messages;
    body["stream"] = stream;
    body["temperature"] = temperature;
    body["max_tokens"] = max_tokens;
    if (thinking)
        body["thinking"] = {{"type", "enabled"}, {"budget_tokens", 4096}};
    return boost::json::serialize(body);
}

inline void inject_tools(std::string& body, bool with_tools,
    const std::vector<std::string>& toolWhitelist,
    const boost::json::array& toolDefs)
{
    if (!with_tools) return;
    auto body_json = boost::json::parse(body);
    if (!body_json.is_object()) return;
    if (toolWhitelist.empty()) {
        body_json.as_object()["tools"] = toolDefs;
    } else {
        boost::json::array filtered;
        for (auto& t : toolDefs) {
            std::string name = t.at("function").as_object().at("name").as_string().c_str();
            for (auto& w : toolWhitelist) {
                if (name == w) {
                    filtered.push_back(t);
                    break;
                }
            }
        }
        body_json.as_object()["tools"] = std::move(filtered);
    }
    body_json.as_object()["tool_choice"] = boost::json::string("auto");
    body = boost::json::serialize(body_json);
}

inline std::map<std::string, LlmToolCall> merge_tool_calls(const std::vector<LlmToolCall>& chunks)
{
    std::map<std::string, LlmToolCall> merged;
    for (auto& tc : chunks) {
        if (tc.id.empty()) {
            if (!merged.empty())
                merged.rbegin()->second.function_arguments += tc.function_arguments;
            continue;
        }
        auto& entry = merged[tc.id];
        if (entry.id.empty()) entry.id = tc.id;
        if (!tc.function_name.empty()) entry.function_name = tc.function_name;
        entry.function_arguments += tc.function_arguments;
    }
    return merged;
}

} // namespace llm
