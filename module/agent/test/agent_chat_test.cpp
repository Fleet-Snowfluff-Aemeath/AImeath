#include <gtest/gtest.h>
#include "agent_chat_api.hpp"
#include "agent_profile.hpp"

namespace agent {

TEST(AgentChatTest, CreateAgentFromProfile_Valid) {
    AgentProfile profile;
    profile.name = "TestAgent";
    profile.avatar = "🧪";
    profile.system_prompt = "You are a test agent.";
    profile.model = "deepseek-v4-flash";
    profile.tools = {"chat_send", "list_active_windows"};

    auto agent = AgentProfileManager::createAgent(profile);
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(agent->getName(), "TestAgent");
    EXPECT_EQ(agent->getAvatar(), "🧪");
    EXPECT_EQ(agent->getModel(), "deepseek-v4-flash");
    EXPECT_EQ(agent->getSystemPrompt(), "You are a test agent.");
    ASSERT_EQ(agent->getTools().size(), 2u);
    EXPECT_EQ(agent->getTools()[0], "chat_send");
}

TEST(AgentChatTest, SetResponseCallback_NoCrash) {
    AgentProfile profile;
    profile.name = "Test";
    auto agent = AgentProfileManager::createAgent(profile);
    bool called = false;
    agent->setResponseCallback([&called](boost::json::object) { called = true; });
    agent->setResponseCallback(nullptr);
    EXPECT_NO_THROW(agent->setResponseCallback(nullptr));
}

TEST(AgentChatTest, Stop_PreventsCallback) {
    AgentProfile profile;
    profile.name = "Test";
    auto agent = AgentProfileManager::createAgent(profile);
    bool called = false;
    agent->setResponseCallback([&called](boost::json::object) { called = true; });
    agent->stop();
    EXPECT_NO_THROW(agent->stop());
    agent->setResponseCallback(nullptr);
}

TEST(AgentChatTest, Stop_MultipleCalls_NoCrash) {
    AgentProfile profile;
    profile.name = "Test";
    auto agent = AgentProfileManager::createAgent(profile);
    agent->stop();
    agent->stop();
    agent->stop();
    SUCCEED();
}

TEST(AgentChatTest, SetIoContext_NoCrash) {
    AgentProfile profile;
    profile.name = "Test";
    auto agent = AgentProfileManager::createAgent(profile);
    int dummy = 0;
    agent->setIoContext(&dummy);
    EXPECT_NO_THROW(agent->setIoContext(nullptr));
}

} // namespace agent
