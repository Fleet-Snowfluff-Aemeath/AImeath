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

void AgentChatParticipant::doLlmRound(const boost::json::array& msgs, int round) {
    if (cancelled_) {
        boost::json::object end;
        end["type"] = "stream_end";
        pushStream(std::move(end));
        return;
    }

    std::string body = llm::build_chat_body(msgs, profile_.model, true, true,
        profile_.temperature, profile_.max_tokens);

    if (profile_.enable_tools) {
        if (s_toolDefs.empty())
            s_toolDefs = agent::loadToolsFromYaml(std::string(PROJ_ROOT) + "/module/agent/config/tools.yml");
        llm::inject_tools(body, true, profile_.tools, s_toolDefs);
    }

    std::string apiKey = Config::instance().deepSeekApiKey();
    if (apiKey.empty()) return;

    auto cancelled_ptr = &cancelled_;
    auto self = shared_from_this();

    std::thread t([self, body = std::move(body), apiKey = std::move(apiKey), cancelled_ptr, round, msgs]() {
        boost::asio::io_context io_local;
        auto client = std::make_shared<LlmClient>(io_local);
        std::string fullResponse;
        std::string fullReasoning;
        auto toolCalls = std::make_shared<std::vector<LlmToolCall>>();

        client->start("api.deepseek.com", "443", body, "Bearer " + apiKey,
            [cancelled_ptr, self, toolCalls](LlmEvent ev) {
                if (cancelled_ptr->load()) return;
                if (!ev.tool_calls.empty()) {
                    toolCalls->insert(toolCalls->end(), ev.tool_calls.begin(), ev.tool_calls.end());
                }
                if (ev.type == "delta" || ev.type == "reasoning") {
                    boost::json::object obj;
                    obj["type"] = ev.type;
                    obj["text"] = ev.text;
                    self->pushStream(std::move(obj));
                }
            },
            [cancelled_ptr, self, toolCalls, round, msgs](std::string response, std::string reasoning, int, int) {
                if (cancelled_ptr->load()) return;
                auto merged = llm::merge_tool_calls(*toolCalls);
                if (!merged.empty()) {
                    boost::json::array newMsgs = msgs;
                    {
                        boost::json::object am;
                        am["role"] = "assistant";
                        if (!response.empty()) am["content"] = response;
                        if (!reasoning.empty()) am["reasoning_content"] = reasoning;
                        boost::json::array tcArr;
                        for (auto& kv : merged) {
                            auto& tc = kv.second;
                            boost::json::object tcObj;
                            tcObj["id"] = tc.id;
                            tcObj["type"] = "function";
                            tcObj["function"] = boost::json::object{{"name", tc.function_name}, {"arguments", tc.function_arguments}};
                            tcArr.push_back(std::move(tcObj));
                        }
                        am["tool_calls"] = std::move(tcArr);
                        newMsgs.push_back(std::move(am));
                    }
                    for (auto& kv : merged) {
                        auto& tc = kv.second;
                        std::string result = "{\"success\":true}";
                        ToolExecutor executor;
                        {
                            std::lock_guard<std::mutex> lock(self->mtx_);
                            executor = self->tool_executor_;
                        }
                        if (executor) result = executor(tc.function_name, tc.function_arguments);
                        boost::json::object tr;
                        tr["role"] = "tool";
                        tr["tool_call_id"] = tc.id;
                        tr["content"] = result;
                        newMsgs.push_back(std::move(tr));
                    }
                    self->doLlmRound(newMsgs, round + 1);
                } else {
                    self->pushStream(boost::json::object{{"type", "stream_end"}});
                    if (!response.empty()) {
                        boost::json::object am;
                        am["role"] = "assistant";
                        am["sender_name"] = self->profile_.name;
                        am["sender_avatar"] = self->profile_.avatar;
                        am["content"] = response;
                        if (!reasoning.empty()) am["reasoning_content"] = reasoning;
                        self->pushResponse(std::move(am));
                    }
                }
            });
        io_local.run();
    });
    t.detach();
}

void AgentChatParticipant::pushStream(boost::json::object ev) {
    if (!ev.contains("sender_name")) ev["sender_name"] = profile_.name;
    if (!ev.contains("sender_avatar")) ev["sender_avatar"] = profile_.avatar;
    StreamCallback cb;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        cb = stream_cb_;
    }
    if (cb) cb(std::move(ev));
}

void AgentChatParticipant::pushResponse(boost::json::object msg) {
    ResponseCallback cb;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        cb = response_cb_;
    }
    if (cb) cb(std::move(msg));
}

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

    pushStream(boost::json::object{{"type", "stream_start"}});
    doLlmRound(msgs, 1);
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
