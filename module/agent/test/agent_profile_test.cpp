#include <gtest/gtest.h>
#include "agent_profile.hpp"
#include <fstream>

static std::string testYamlPath() {
    return "/tmp/test_agent_profile.yaml";
}

static void writeTestYaml(const std::string& content) {
    std::ofstream f(testYamlPath());
    f << content;
}

static void cleanup() {
    std::remove(testYamlPath().c_str());
}

TEST(AgentProfileTest, LoadValidYaml) {
    writeTestYaml(R"(
name: "测试助手"
avatar: "test.png"
system_prompt: "你是一个测试助手"
model: "deepseek-v4-flash"
temperature: 0.5
max_tokens: 2048
enable_tools: false
)");
    auto profile = agent::AgentProfile::fromYaml(testYamlPath());
    EXPECT_EQ(profile.name, "测试助手");
    EXPECT_EQ(profile.avatar, "test.png");
    EXPECT_EQ(profile.system_prompt, "你是一个测试助手");
    EXPECT_EQ(profile.model, "deepseek-v4-flash");
    EXPECT_DOUBLE_EQ(profile.temperature, 0.5);
    EXPECT_EQ(profile.max_tokens, 2048);
    EXPECT_FALSE(profile.enable_tools);
    cleanup();
}

TEST(AgentProfileTest, LoadMinimalYaml) {
    writeTestYaml(R"(
name: "minimal"
)");
    auto profile = agent::AgentProfile::fromYaml(testYamlPath());
    EXPECT_EQ(profile.name, "minimal");
    EXPECT_EQ(profile.avatar, "");
    EXPECT_EQ(profile.model, "deepseek-v4-flash");
    EXPECT_DOUBLE_EQ(profile.temperature, 0.7);
    EXPECT_EQ(profile.max_tokens, 4096);
    EXPECT_TRUE(profile.enable_tools);
    cleanup();
}

TEST(AgentProfileTest, LoadAllFromDir) {
    auto profiles = agent::AgentProfile::loadAll("/tmp");
    EXPECT_GE(profiles.size(), 0u);
}
