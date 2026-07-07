#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include "agent_chat_api.hpp"
#include <boost/json.hpp>
#include <boost/noncopyable.hpp>

namespace agent {

enum class ChatType { GROUP, PRIVATE_CHAT };

class AgentManager : private boost::noncopyable {
public:
    static AgentManager& instance();

    std::string allocId();

    std::shared_ptr<IAgentChat> createFromYaml(const std::string& yamlPath);
    std::shared_ptr<IAgentChat> find(const std::string& id);

    bool joinChat(const std::string& agentId, const std::string& chatId, ChatType type);
    void leaveChat(const std::string& agentId, const std::string& chatId);
    void removeAgent(const std::string& agentId);

    std::vector<std::shared_ptr<IAgentChat>> getChatAgents(const std::string& chatId);
    int getAgentCount(const std::string& chatId);
    bool isPrivateChat(const std::string& chatId);
    void setChatType(const std::string& chatId, ChatType type);

    void broadcastToChat(
        const std::string& chatId,
        const std::string& text,
        const std::string& senderName,
        const std::vector<boost::json::object>& chatHistory);

private:
    AgentManager() = default;
    std::mutex mtx_;
    std::atomic<uint64_t> seq_{1};
    std::map<std::string, std::shared_ptr<IAgentChat>> agents_;
    std::map<std::string, std::vector<std::string>> chatAgents_;
    std::map<std::string, std::vector<std::string>> agentChats_;
    std::map<std::string, ChatType> chatTypes_;
};

} // namespace agent
