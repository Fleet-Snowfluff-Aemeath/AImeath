#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <yaml-cpp/yaml.h>

namespace agent {

class IAgentChat;

struct AgentProfile {
    std::string name;
    std::string avatar;
    std::string system_prompt;
    std::string model = "deepseek-v4-flash";
    double temperature = 0.7;
    int max_tokens = 4096;
    bool enable_tools = true;
    std::vector<std::string> tools;

    static AgentProfile fromYaml(const std::string& path) {
        YAML::Node config = YAML::LoadFile(path);
        AgentProfile profile;
        if (config["name"]) profile.name = config["name"].as<std::string>();
        if (config["avatar"]) profile.avatar = config["avatar"].as<std::string>();
        if (config["system_prompt"]) profile.system_prompt = config["system_prompt"].as<std::string>();
        if (config["model"]) profile.model = config["model"].as<std::string>();
        if (config["temperature"]) profile.temperature = config["temperature"].as<double>();
        if (config["max_tokens"]) profile.max_tokens = config["max_tokens"].as<int>();
        if (config["tools"]) {
            profile.enable_tools = true;
            for (auto t : config["tools"])
                profile.tools.push_back(t.as<std::string>());
        } else if (config["enable_tools"]) {
            profile.enable_tools = config["enable_tools"].as<bool>();
        }
        return profile;
    }

    static std::vector<AgentProfile> loadAll(const std::string& dir) {
        std::vector<AgentProfile> profiles;
        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".yaml" || entry.path().extension() == ".yml") {
                try {
                    profiles.push_back(fromYaml(entry.path().string()));
                } catch (...) {}
            }
        }
        return profiles;
    }
};

class AgentProfileManager {
public:
    static std::shared_ptr<IAgentChat> createAgent(const AgentProfile& profile);
};

} // namespace agent
