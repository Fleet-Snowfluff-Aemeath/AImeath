#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include <boost/json.hpp>
#include <boost/asio.hpp>

#include "agent_api.hpp"
#include "message_queue.hpp"
#include "app_manager.hpp"

class LlmClient;

typedef void (*app_output_fn)(void* userdata, const char* json);

namespace agent {

using ToolHandler = std::function<boost::json::value(const boost::json::value& args)>;

struct ToolDef {
    std::string name;
    std::string description;
    boost::json::object parameters;
    ToolHandler handler;
};

class AgentServer : public IAgent, public std::enable_shared_from_this<AgentServer>
{
public:
    AgentServer();

    void setOutput(app_output_fn cb, void* udata);
    void setIoContext(void* io_ctx);
    void onInput(const std::string& json);
    std::string process(const std::string& json);
    bool isDone() const;
    void destroy();

    bool openApp(const std::string& name, const std::string& paramsJson) override;
    bool controlApp(const std::string& name, const std::string& commandJson) override;
    bool closeApp(const std::string& name) override;
    void stop() override;

    bool chatSend(const std::string& text) override;
    bool fileList(const std::string& path) override;
    bool fileRead(const std::string& path) override;
    bool fileWrite(const std::string& path, const std::string& content) override;
    bool fileMkdir(const std::string& path) override;
    bool fileRemove(const std::string& path) override;
    bool terminalExec(const std::string& command) override;
    bool terminalStdin(const std::string& data) override;

    std::shared_ptr<AgentServer> selfHolder_;

private:
    void pushOutput(boost::json::value val);
    void handleUserMessage(const std::string& text);
    void handleToolCalls(const std::vector<boost::json::value>& tool_calls);
    boost::json::value executeTool(const std::string& name, const boost::json::value& args);
    void processNextInQueue();
    static boost::json::array buildTools();
    void registerBuiltinTools();

    std::vector<boost::json::object> history_;
    MessageQueue<std::string> inputQueue_;
    std::mutex mtx_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> streaming_{false};
    int round_ = 0;

    app_output_fn outputCb_ = nullptr;
    void* outputUdata_ = nullptr;
    void* ioCtxPtr_ = nullptr;

    std::shared_ptr<::LlmClient> currentStream_;
    std::unordered_map<std::string, ToolDef> tools_;

    bool done_ = false;

    uint64_t subHandle_ = 0;
    void ensureSubscribed();
    void onAppStateChange(const std::string& appName, const boost::json::value& state);
    void injectStateIntoHistory(const std::string& appName, const boost::json::value& state);
};

} // namespace agent
