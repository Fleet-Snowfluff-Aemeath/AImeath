#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <memory>

#include <boost/json.hpp>

#include "plugin.hpp"
#include "message_queue.hpp"

namespace agent {
class IAgentChat;
}

struct ChatPlugin : std::enable_shared_from_this<ChatPlugin>
{
    std::vector<boost::json::object> history;
    std::vector<boost::json::object> pending_outputs;
    MessageQueue<std::string> input_queue;
    std::mutex mtx;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> streaming{false};
    int round = 0;

    plugin_output_fn output_cb = nullptr;
    void* output_udata = nullptr;
    void* io_ctx_ptr = nullptr;

    bool done = false;

    std::shared_ptr<ChatPlugin> self_holder;

    IPluginCache* mod_cache = nullptr;
    std::map<std::string, PluginInstance> instances;

    std::string chatId;

    std::vector<std::shared_ptr<agent::IAgentChat>> agents;

    std::string current_sender_name = "匿名";
    std::string current_sender_avatar = " ";
    std::string user_display_name;

    void push_output(boost::json::value val);
};
