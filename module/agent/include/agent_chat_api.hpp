#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <boost/json.hpp>

namespace agent {

class IAgentChat {
public:
    virtual ~IAgentChat() = default;

    virtual const std::string& getName() const = 0;
    virtual const std::string& getAvatar() const = 0;
    virtual const std::string& getModel() const = 0;
    virtual const std::string& getSystemPrompt() const = 0;
    virtual const std::vector<std::string>& getTools() const = 0;

    virtual void onUserMessage(
        const std::string& text,
        const std::string& sender_name,
        const std::vector<boost::json::object>& room_history
    ) = 0;

    using ResponseCallback = std::function<void(boost::json::object response_msg)>;
    virtual void setResponseCallback(ResponseCallback cb) = 0;

    using StreamCallback = std::function<void(boost::json::object stream_event)>;
    virtual void setStreamCallback(StreamCallback cb) = 0;

    using ToolExecutor = std::function<std::string(const std::string& name, const std::string& args)>;
    virtual void setToolExecutor(ToolExecutor executor) = 0;

    virtual void setIoContext(void* io_ctx) = 0;

    virtual void stop() = 0;
};

std::shared_ptr<IAgentChat> createAgentFromProfile(const std::string& yamlPath);
std::vector<std::shared_ptr<IAgentChat>> loadAgentsFromDir(const std::string& dir);

} // namespace agent
