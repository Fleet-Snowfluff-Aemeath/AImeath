#include "agent_manager.hpp"
#include "agent_profile.hpp"
#include "agent_chat_participant.hpp"
#include <sstream>
#include <algorithm>

namespace agent {

AgentManager& AgentManager::instance() {
    static AgentManager mgr;
    return mgr;
}

std::string AgentManager::allocId() {
    std::ostringstream oss;
    oss << "ag_" << seq_.fetch_add(1);
    return oss.str();
}

std::shared_ptr<IAgentChat> AgentManager::createFromYaml(const std::string& yamlPath) {
    auto profile = AgentProfile::fromYaml(yamlPath);
    auto agent = AgentProfileManager::createAgent(profile);
    std::string id = allocId();
    std::lock_guard<std::mutex> lock(mtx_);
    agents_[id] = agent;
    return agent;
}

std::shared_ptr<IAgentChat> AgentManager::find(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = agents_.find(id);
    return it != agents_.end() ? it->second : nullptr;
}

bool AgentManager::joinChat(const std::string& agentId, const std::string& chatId, ChatType type) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!agents_.count(agentId)) return false;

    chatTypes_[chatId] = type;

    if (type == ChatType::PRIVATE_CHAT) {
        int count = 0;
        if (chatAgents_.count(chatId))
            count = static_cast<int>(chatAgents_[chatId].size());
        if (count >= 2) return false;
    }

    chatAgents_[chatId].push_back(agentId);
    agentChats_[agentId].push_back(chatId);
    return true;
}

void AgentManager::leaveChat(const std::string& agentId, const std::string& chatId) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (chatAgents_.count(chatId)) {
        auto& vec = chatAgents_[chatId];
        vec.erase(std::remove(vec.begin(), vec.end(), agentId), vec.end());
    }
    if (agentChats_.count(agentId)) {
        auto& avec = agentChats_[agentId];
        avec.erase(std::remove(avec.begin(), avec.end(), chatId), avec.end());
    }
}

void AgentManager::removeAgent(const std::string& agentId) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (agentChats_.count(agentId)) {
        for (auto& chatId : agentChats_[agentId]) {
            auto& vec = chatAgents_[chatId];
            vec.erase(std::remove(vec.begin(), vec.end(), agentId), vec.end());
        }
        agentChats_.erase(agentId);
    }
    auto it = agents_.find(agentId);
    if (it != agents_.end()) {
        it->second->stop();
        agents_.erase(it);
    }
}

std::vector<std::shared_ptr<IAgentChat>> AgentManager::getChatAgents(const std::string& chatId) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::shared_ptr<IAgentChat>> result;
    if (chatAgents_.count(chatId)) {
        for (auto& id : chatAgents_[chatId]) {
            if (agents_.count(id)) result.push_back(agents_[id]);
        }
    }
    return result;
}

int AgentManager::getAgentCount(const std::string& chatId) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (chatAgents_.count(chatId))
        return static_cast<int>(chatAgents_[chatId].size());
    return 0;
}

bool AgentManager::isPrivateChat(const std::string& chatId) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = chatTypes_.find(chatId);
    return it != chatTypes_.end() && it->second == ChatType::PRIVATE_CHAT;
}

void AgentManager::setChatType(const std::string& chatId, ChatType type) {
    std::lock_guard<std::mutex> lock(mtx_);
    chatTypes_[chatId] = type;
}

void AgentManager::broadcastToChat(
    const std::string& chatId,
    const std::string& text,
    const std::string& senderName,
    const std::vector<boost::json::object>& chatHistory)
{
    auto agents = getChatAgents(chatId);
    for (auto& ag : agents) {
        if (ag->getName() != senderName) {
            ag->onUserMessage(text, senderName, chatHistory);
        }
    }
}

} // namespace agent
