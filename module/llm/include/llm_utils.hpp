#pragma once

#include <boost/json.hpp>
#include <string>

namespace llm {

inline std::string build_chat_body(const boost::json::array& messages,
                                   const std::string& model = "deepseek-v4-flash",
                                   bool stream = true,
                                   bool thinking = true)
{
    boost::json::object body;
    body["model"] = model;
    body["messages"] = messages;
    body["stream"] = stream;
    if (thinking)
        body["thinking"] = {{"type", "enabled"}, {"budget_tokens", 4096}};
    return boost::json::serialize(body);
}

} // namespace llm
