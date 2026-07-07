#pragma once

#include "agent_chat_api.hpp"
#include "agent_profile.hpp"
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <string>

namespace agent {

class AgentChatParticipant : public IAgentChat, public std::enable_shared_from_this<AgentChatParticipant> {
public:
    explicit AgentChatParticipant(const AgentProfile& profile) : profile_(profile) {}

    const std::string& getName() const override { return profile_.name; }
    const std::string& getAvatar() const override { return profile_.avatar; }
    const std::string& getModel() const override { return profile_.model; }
    const std::string& getSystemPrompt() const override { return profile_.system_prompt; }
    const std::vector<std::string>& getTools() const override { return profile_.tools; }

    void onUserMessage(
        const std::string& text,
        const std::string& sender_name,
        const std::vector<boost::json::object>& room_history
    ) override;

    void setResponseCallback(ResponseCallback cb) override {
        std::lock_guard<std::mutex> lock(mtx_);
        response_cb_ = std::move(cb);
    }

    void setStreamCallback(StreamCallback cb) override {
        std::lock_guard<std::mutex> lock(mtx_);
        stream_cb_ = std::move(cb);
    }

    void setToolExecutor(ToolExecutor executor) override {
        std::lock_guard<std::mutex> lock(mtx_);
        tool_executor_ = std::move(executor);
    }

    void setIoContext(void* io_ctx) override {
        io_ctx_ptr_ = io_ctx;
    }

    void stop() override {
        cancelled_ = true;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            response_cb_ = nullptr;
            stream_cb_ = nullptr;
            tool_executor_ = nullptr;
        }
    }

private:
    AgentProfile profile_;
    std::mutex mtx_;
    std::atomic<bool> cancelled_{false};
    ResponseCallback response_cb_;
    StreamCallback stream_cb_;
    ToolExecutor tool_executor_;
    void* io_ctx_ptr_ = nullptr;
    void doLlmRound(const boost::json::array& msgs, int round);
    void pushStream(boost::json::object ev);
    void pushResponse(boost::json::object msg);
};

} // namespace agent
