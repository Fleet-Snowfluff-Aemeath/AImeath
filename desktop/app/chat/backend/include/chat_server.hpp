#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <memory>

#include <boost/json.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "app_api.hpp"
#include "iface_mod.hpp"
#include "message_queue.hpp"

class LlmClient;

namespace agent {
class IAgentChat;
}

struct AppInstance
{
    AppModule mod;
    AppPtr handle;
    std::string appName;
};

struct ChatApp : std::enable_shared_from_this<ChatApp>
{
    std::vector<boost::json::object> history;
    std::vector<boost::json::object> pending_outputs;
    MessageQueue<std::string> input_queue;
    std::mutex mtx;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> streaming{false};
    int round = 0;
    int consecutive_tool_rounds = 0;

    app_output_fn output_cb = nullptr;
    void* output_udata = nullptr;
    void* io_ctx_ptr = nullptr;

    std::shared_ptr<LlmClient> current_stream;
    bool done = false;

    std::shared_ptr<ChatApp> self_holder;

    IModuleCache* mod_cache = nullptr;
    std::map<std::string, AppInstance> instances;

    std::string chatId;
    bool isGroupChat = true;

    std::vector<std::shared_ptr<agent::IAgentChat>> agents;

    std::string current_sender_name = "AI助手";
    std::string current_sender_avatar = "/res/C220748556D18ADBC61177B1A5A8151D.png";
    std::string user_display_name;

    void push_output(boost::json::value val);
};

void chatServe(boost::beast::websocket::stream<boost::beast::tcp_stream>& ws,
               const std::string& first_msg);
