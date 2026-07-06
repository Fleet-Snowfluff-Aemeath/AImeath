#include "agent_chat_participant.hpp"

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <thread>
#include <iostream>
#include <chrono>
#include <ctime>

#include "llm_client.hpp"
#include "llm_utils.hpp"
#include "config.hpp"
#include "tool_registry.hpp"

static boost::json::array s_toolDefs;

namespace agent {

#define AGENT_CHAT_LOG(level, msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto t = std::chrono::system_clock::to_time_t(now); \
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
            now.time_since_epoch()) % 1000; \
        char buf[32]; \
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t)); \
        std::cerr << "[" << buf << "." << ms.count() << "] [agent-chat] " << msg << std::endl; \
    } while(0)

void AgentChatParticipant::onUserMessage(
    const std::string& /*text*/,
    const std::string& /*sender_name*/,
    const std::vector<boost::json::object>& room_history)
{
    if (cancelled_) return;

    boost::json::array msgs;

    {
        boost::json::object sys;
        sys["role"] = "system";
        sys["content"] = profile_.system_prompt;
        msgs.push_back(std::move(sys));
    }

    for (auto& m : room_history) {
        boost::json::object copy;
        if (m.contains("role")) copy["role"] = m.at("role");
        if (m.contains("content")) copy["content"] = m.at("content");
        msgs.push_back(std::move(copy));
    }

    std::string body = llm::build_chat_body(msgs, profile_.model, true, true,
        profile_.temperature, profile_.max_tokens);

    if (profile_.enable_tools) {
        if (s_toolDefs.empty())
            s_toolDefs = agent::loadToolsFromYaml("module/agent/config/tools.yml");
        llm::inject_tools(body, true, profile_.tools, s_toolDefs);
    }

    std::string apiKey = Config::instance().deepSeekApiKey();
    if (apiKey.empty()) return;

    void* io_ctx_local = io_ctx_ptr_;
    boost::asio::io_context* io = static_cast<boost::asio::io_context*>(io_ctx_local);

    auto cancelled_ptr = &cancelled_;
    auto self = shared_from_this();

    auto makeCallback = [this](boost::json::object msg) {
        ResponseCallback cb;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cb = response_cb_;
        }
        if (cb) cb(std::move(msg));
    };

    auto pushStream = [this](boost::json::object ev) {
        if (!ev.contains("sender_name")) ev["sender_name"] = profile_.name;
        if (!ev.contains("sender_avatar")) ev["sender_avatar"] = profile_.avatar;
        StreamCallback cb;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cb = stream_cb_;
        }
        if (cb) cb(std::move(ev));
    };

    auto pushResponse = [this](boost::json::object msg) {
        ResponseCallback cb;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cb = response_cb_;
        }
        if (cb) cb(std::move(msg));
    };

    if (!io) {
        pushStream(boost::json::object{{"type", "stream_start"}});
        std::thread([self, body = std::move(body), apiKey = std::move(apiKey), cancelled_ptr, pushStream, pushResponse]() {
            boost::asio::io_context io_local;
            auto client = std::make_shared<LlmClient>(io_local);
            std::string fullResponse;

            client->start("api.deepseek.com", "443", body, "Bearer " + apiKey,
                [cancelled_ptr, pushStream](LlmEvent ev) {
                    if (cancelled_ptr->load()) return;
                    if (ev.type == "delta" || ev.type == "reasoning") {
                        boost::json::object obj;
                        obj["type"] = ev.type;
                        obj["text"] = ev.text;
                        pushStream(std::move(obj));
                    }
                },
                [cancelled_ptr, &fullResponse](std::string response, std::string, int, int) {
                    if (cancelled_ptr->load()) return;
                    fullResponse = std::move(response);
                });

            io_local.run();

            pushStream(boost::json::object{{"type", "stream_end"}});
            if (!cancelled_ptr->load() && !fullResponse.empty()) {
                boost::json::object am;
                am["role"] = "assistant";
                am["sender_name"] = self->profile_.name;
                am["sender_avatar"] = self->profile_.avatar;
                am["content"] = fullResponse;
                pushResponse(std::move(am));
            }
        }).detach();
        return;
    }

    pushStream(boost::json::object{{"type", "stream_start"}});

    auto stream = std::make_shared<LlmClient>(*io);

    stream->start("api.deepseek.com", "443", body, "Bearer " + apiKey,
        [cancelled_ptr, pushStream](LlmEvent ev) {
            if (cancelled_ptr->load()) return;
            if (ev.type == "delta" || ev.type == "reasoning") {
                boost::json::object obj;
                obj["type"] = ev.type;
                obj["text"] = ev.text;
                pushStream(std::move(obj));
            }
        },
        [cancelled_ptr, pushStream, pushResponse, self](std::string response, std::string, int, int) {
            if (cancelled_ptr->load()) return;
            pushStream(boost::json::object{{"type", "stream_end"}});
            if (!response.empty()) {
                boost::json::object am;
                am["role"] = "assistant";
                am["sender_name"] = self->profile_.name;
                am["sender_avatar"] = self->profile_.avatar;
                am["content"] = response;
                pushResponse(std::move(am));
            }
        });
}

std::shared_ptr<IAgentChat> AgentProfileManager::createAgent(const AgentProfile& profile) {
    return std::make_shared<AgentChatParticipant>(profile);
}

std::shared_ptr<IAgentChat> createAgentFromProfile(const std::string& yamlPath) {
    auto profile = AgentProfile::fromYaml(yamlPath);
    return AgentProfileManager::createAgent(profile);
}

std::vector<std::shared_ptr<IAgentChat>> loadAgentsFromDir(const std::string& dir) {
    std::vector<std::shared_ptr<IAgentChat>> agents;
    auto profiles = AgentProfile::loadAll(dir);
    for (auto& p : profiles) {
        agents.push_back(AgentProfileManager::createAgent(p));
    }
    return agents;
}

} // namespace agent
