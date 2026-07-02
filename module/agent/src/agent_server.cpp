#include "agent_server.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/json.hpp>

#include "llm_client.hpp"
#include "llm_utils.hpp"
#include "config.hpp"

namespace asio = boost::asio;

namespace agent {

#define AGENT_LOG(level, msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto t = std::chrono::system_clock::to_time_t(now); \
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
            now.time_since_epoch()) % 1000; \
        char buf[32]; \
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t)); \
        std::cerr << "[" << buf << "." << ms.count() << "] [agent] " << level << " " << msg << std::endl; \
    } while(0)

static boost::json::object buildSystemMsg()
{
    boost::json::object msg;
    msg["role"] = "system";
    msg["content"] =
        "You are an AI Agent assistant. You can help users by opening and controlling applications.\n"
        "When a user asks you to do something, use the available tools to execute actions.\n"
        "After each tool execution, briefly explain what you did in Chinese.\n"
        "Available tools: open_app (open an application), control_app (send commands to an app), "
        "close_app (close an application), get_app_state (query app status), "
        "chat_send (send a message to chat), file_list (list directory contents), "
        "file_read (read a file), terminal_exec (execute a terminal command), "
        "list_active_windows (list all open windows with their session IDs).\n"
        "Keep responses concise and friendly.";
    return msg;
}

boost::json::array AgentServer::buildTools()
{
    boost::json::array tools;

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "open_app";
        f["description"] = "打开一个应用程序窗口. Use this when the user asks to open or launch an app.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object appProp;
        appProp["type"] = "string";
        appProp["description"] = "应用名称, 可选: snake, gomoku, pacman, go, chat, terminal, filemanager";
        props["app"] = appProp;
        boost::json::object wProp;
        wProp["type"] = "integer";
        wProp["description"] = "棋盘宽度 (游戏类应用, default 20)";
        props["width"] = wProp;
        boost::json::object hProp;
        hProp["type"] = "integer";
        hProp["description"] = "棋盘高度 (游戏类应用, default 20)";
        props["height"] = hProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("app"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "control_app";
        f["description"] =
            "向已打开的应用发送操作指令. Use this to control a running app, "
            "e.g., move in a game. Direction values: 0=up, 1=down, 2=left, 3=right. "
            "For go: -1=pass, -2=resign.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object appProp;
        appProp["type"] = "string";
        appProp["description"] = "目标应用名称";
        props["app"] = appProp;
        boost::json::object valProp;
        valProp["type"] = "integer";
        valProp["description"] = "操作值 (游戏方向等)";
        props["value"] = valProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("app"));
        required.push_back(boost::json::string("value"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "close_app";
        f["description"] = "关闭一个已打开的应用窗口. 可用 list_active_windows 获取 window_id 来指定关闭哪一个.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object appProp;
        appProp["type"] = "string";
        appProp["description"] = "要关闭的应用名称";
        props["app"] = appProp;
        boost::json::object widProp;
        widProp["type"] = "string";
        widProp["description"] = "可选, 指定要关闭的窗口 ID (从 list_active_windows 获取). 不指定则关闭该应用最新的窗口.";
        props["window_id"] = widProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("app"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "get_app_state";
        f["description"] = "查询一个应用的当前状态, 如游戏分数、界面等.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object appProp;
        appProp["type"] = "string";
        appProp["description"] = "应用名称";
        props["app"] = appProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("app"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "chat_send";
        f["description"] = "向聊天应用发送一条消息.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object textProp;
        textProp["type"] = "string";
        textProp["description"] = "要发送的消息内容";
        props["text"] = textProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("text"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "file_list";
        f["description"] = "列出指定目录中的文件和文件夹.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object pathProp;
        pathProp["type"] = "string";
        pathProp["description"] = "目录路径, 默认为 '/'";
        props["path"] = pathProp;
        params["properties"] = props;
        boost::json::array required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "file_read";
        f["description"] = "读取指定文件的内容.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object pathProp;
        pathProp["type"] = "string";
        pathProp["description"] = "文件路径";
        props["path"] = pathProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("path"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "terminal_exec";
        f["description"] = "在终端中执行一条命令并获取输出.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        boost::json::object cmdProp;
        cmdProp["type"] = "string";
        cmdProp["description"] = "要执行的命令";
        props["command"] = cmdProp;
        params["properties"] = props;
        boost::json::array required;
        required.push_back(boost::json::string("command"));
        params["required"] = required;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    {
        boost::json::object t;
        t["type"] = "function";
        boost::json::object f;
        f["name"] = "list_active_windows";
        f["description"] = "列出当前所有活跃的应用窗口及其 session 信息.";
        boost::json::object params;
        params["type"] = "object";
        boost::json::object props;
        params["properties"] = props;
        f["parameters"] = params;
        t["function"] = f;
        tools.push_back(t);
    }

    return tools;
}

void AgentServer::registerBuiltinTools()
{
    tools_["open_app"] = {"open_app", "打开应用", {}, nullptr};
    tools_["control_app"] = {"control_app", "操控应用", {}, nullptr};
    tools_["close_app"] = {"close_app", "关闭应用", {}, nullptr};
    tools_["get_app_state"] = {"get_app_state", "查询应用状态", {}, nullptr};
    tools_["chat_send"] = {"chat_send", "向聊天应用发送消息", {}, nullptr};
    tools_["file_list"] = {"file_list", "列出目录文件", {}, nullptr};
    tools_["file_read"] = {"file_read", "读取文件内容", {}, nullptr};
    tools_["terminal_exec"] = {"terminal_exec", "在终端执行命令", {}, nullptr};
    tools_["list_active_windows"] = {"list_active_windows", "列出活跃窗口", {}, nullptr};
}

AgentServer::AgentServer()
{
    registerBuiltinTools();
    boost::json::object sys = buildSystemMsg();
    history_.push_back(sys);
}

void AgentServer::ensureSubscribed()
{
    if (subHandle_ != 0) return;
    try {
        std::weak_ptr<AgentServer> weakSelf = selfHolder_;
        subHandle_ = AppManager::instance().subscribe(
            [weakSelf](const std::string& appName, const boost::json::value& state) {
                auto s = weakSelf.lock();
                if (s) s->onAppStateChange(appName, state);
            });
    } catch (...) {}
}

void AgentServer::setOutput(app_output_fn cb, void* udata)
{
    ensureSubscribed();
    std::lock_guard<std::mutex> lock(mtx_);
    outputCb_ = cb;
    outputUdata_ = udata;
}

void AgentServer::setIoContext(void* ioCtx)
{
    ioCtxPtr_ = ioCtx;
}

void AgentServer::onInput(const std::string& json)
{
    try {
        auto val = boost::json::parse(json);
        if (!val.is_object()) return;
        auto& obj = val.as_object();

        auto actionIt = obj.find("action");
        if (actionIt != obj.end() && actionIt->value().is_string())
        {
            auto action = actionIt->value().as_string();
            if (action == "stop") {
                AGENT_LOG("[in]", "stop request");
                stop();
                boost::json::object end;
                end["type"] = "stream_end";
                end["msg"] = "stopped";
                pushOutput(std::move(end));
                return;
            }
        }

        auto textIt = obj.find("text");
        if (textIt != obj.end() && textIt->value().is_string())
        {
            std::string text(textIt->value().as_string());
            AGENT_LOG("[in]", "text: \"" << text.substr(0, 30)
                 << (text.size() > 30 ? "..." : "") << "\""
                 << " (streaming=" << streaming_ << ")");
            if (streaming_) {
                inputQueue_.push(text);
                AGENT_LOG("[q]", "queued (size=" << inputQueue_.size() << ")");
            } else {
                streaming_ = true;
                ++round_;
                handleUserMessage(text);
            }
        }
    } catch (...) {}
}

void AgentServer::handleUserMessage(const std::string& text)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        boost::json::object um;
        um["role"] = "user";
        um["content"] = text;
        history_.push_back(std::move(um));
    }

    std::vector<boost::json::object> historyCopy;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        historyCopy = history_;
    }

    boost::json::array msgs;
    for (auto& m : historyCopy) msgs.push_back(m);

    std::string body = llm::build_chat_body(msgs);
    boost::json::array tools = buildTools();

    boost::json::value parsed = boost::json::parse(body);
    if (parsed.is_object()) {
        parsed.as_object()["tools"] = tools;
        parsed.as_object()["tool_choice"] = boost::json::string("auto");
        body = boost::json::serialize(parsed);
    }

    std::string apiKey = Config::instance().deepSeekApiKey();
    if (apiKey.empty()) {
        boost::json::object end;
        end["type"] = "stream_end";
        end["msg"] = "missing deepseek_api_key in config.json";
        pushOutput(std::move(end));
        return;
    }

    asio::io_context* io = static_cast<asio::io_context*>(ioCtxPtr_);
    if (!io) {
        auto self = shared_from_this();
        std::thread t([self, body = std::move(body), apiKey]() {
            asio::io_context io;
            auto client = std::make_shared<::LlmClient>(io);
            std::string fullResponse;
            std::vector<LlmToolCall> toolCalls;

            client->start("api.deepseek.com", "443", body, "Bearer " + apiKey,
                [&](::LlmEvent ev) {
                    if (self->cancelled_) { client->cancel(); return; }
                    if (!ev.tool_calls.empty())
                        toolCalls.insert(toolCalls.end(), ev.tool_calls.begin(), ev.tool_calls.end());
                    boost::json::object obj;
                    obj["type"] = ev.type;
                    if (ev.type == "delta" || ev.type == "reasoning")
                        obj["text"] = std::move(ev.text);
                    if (!ev.msg.empty())
                        obj["msg"] = std::move(ev.msg);
                    if (!ev.tool_calls.empty()) {
                        boost::json::array tcArr;
                        for (auto& tc : ev.tool_calls) {
                            boost::json::object tcObj;
                            tcObj["id"] = tc.id;
                            tcObj["function_name"] = tc.function_name;
                            tcObj["function_arguments"] = tc.function_arguments;
                            tcArr.push_back(std::move(tcObj));
                        }
                        obj["tool_calls"] = std::move(tcArr);
                    }
                    self->pushOutput(std::move(obj));
                },
                [&](std::string response, std::string, int, int) {
                    fullResponse = std::move(response);
                });

            io.run();

            if (!self->cancelled_) {
                boost::json::object am;
                am["role"] = "assistant";
                if (!fullResponse.empty()) am["content"] = fullResponse;

                if (!toolCalls.empty()) {
                    auto merged = llm::merge_tool_calls(toolCalls);
                    std::vector<LlmToolCall> mergedList;
                    for (auto& kv : merged)
                        mergedList.push_back(kv.second);

                    boost::json::array tcArr;
                    for (auto& tc : mergedList) {
                        boost::json::object tcObj;
                        tcObj["id"] = tc.id;
                        tcObj["type"] = "function";
                        tcObj["function"] = boost::json::object{
                            {"name", tc.function_name},
                            {"arguments", tc.function_arguments}
                        };
                        tcArr.push_back(std::move(tcObj));
                    }
                    am["tool_calls"] = std::move(tcArr);
                }

                {
                    std::lock_guard<std::mutex> lock(self->mtx_);
                    self->history_.push_back(std::move(am));
                }

                if (!toolCalls.empty()) {
                    auto merged = llm::merge_tool_calls(toolCalls);
                    std::vector<LlmToolCall> mergedList;
                    for (auto& kv : merged)
                        mergedList.push_back(kv.second);
                    std::vector<boost::json::value> tcValues;
                    for (auto& tc : mergedList) {
                        boost::json::object tcj;
                        tcj["id"] = tc.id;
                        tcj["function_name"] = tc.function_name;
                        tcj["function_arguments"] = tc.function_arguments;
                        tcValues.push_back(std::move(tcj));
                    }
                    self->handleToolCalls(tcValues);
                    self->streaming_ = true;
                    ++self->round_;
                    std::vector<boost::json::object> hc;
                    {
                        std::lock_guard<std::mutex> lock(self->mtx_);
                        hc = self->history_;
                    }
                    boost::json::array msgs2;
                    for (auto& m : hc) msgs2.push_back(m);
                    std::string body2 = llm::build_chat_body(msgs2);
                    boost::json::value parsed2 = boost::json::parse(body2);
                    if (parsed2.is_object()) {
                        boost::json::array tools2 = buildTools();
                        parsed2.as_object()["tools"] = tools2;
                        parsed2.as_object()["tool_choice"] = "auto";
                        body2 = boost::json::serialize(parsed2);
                    }
                    boost::json::object start2;
                    start2["type"] = "stream_start";
                    self->pushOutput(std::move(start2));
                    // follow-up LLM call: pass the updated body directly
                    std::string bodyFollow = body2;
                    std::string apiKeyFollow = Config::instance().deepSeekApiKey();
                    if (!apiKeyFollow.empty()) {
                        std::string fullResp;
                        std::vector<LlmToolCall> followTcs;
                        asio::io_context io2;
                        auto client2 = std::make_shared<::LlmClient>(io2);
                        client2->start("api.deepseek.com", "443", bodyFollow, "Bearer " + apiKeyFollow,
                            [&](LlmEvent ev) {
                                if (self->cancelled_) { client2->cancel(); return; }
                                if (!ev.tool_calls.empty())
                                    followTcs.insert(followTcs.end(), ev.tool_calls.begin(), ev.tool_calls.end());
                            },
                            [&](std::string r, std::string, int, int) {
                                fullResp = std::move(r);
                            });
                        io2.run();
                        if (!self->cancelled_ && (!fullResp.empty() || !followTcs.empty())) {
                            boost::json::object am2;
                            am2["role"] = "assistant";
                            if (!fullResp.empty()) am2["content"] = std::move(fullResp);
                            if (!followTcs.empty()) {
                                auto merged2 = llm::merge_tool_calls(followTcs);
                                std::vector<LlmToolCall> mergedList2;
                                for (auto& kv2 : merged2) mergedList2.push_back(kv2.second);
                                boost::json::array tcArr2;
                                for (auto& tc2 : mergedList2) {
                                    tcArr2.push_back(boost::json::object{
                                        {"id", tc2.id}, {"type", "function"},
                                        {"function", {{"name", tc2.function_name}, {"arguments", tc2.function_arguments}}}
                                    });
                                }
                                am2["tool_calls"] = std::move(tcArr2);
                            }
                            {
                                std::lock_guard<std::mutex> lock(self->mtx_);
                                self->history_.push_back(std::move(am2));
                            }
                            if (!followTcs.empty()) {
                                auto merged2 = llm::merge_tool_calls(followTcs);
                                std::vector<LlmToolCall> mergedList2;
                                for (auto& kv2 : merged2) mergedList2.push_back(kv2.second);
                                std::vector<boost::json::value> tcValues2;
                                for (auto& tc2 : mergedList2) {
                                    tcValues2.push_back(boost::json::object{
                                        {"id", tc2.id}, {"function_name", tc2.function_name},
                                        {"function_arguments", tc2.function_arguments}
                                    });
                                }
                                self->handleToolCalls(tcValues2);
                                // Max 3 tool rounds, then stop
                            }
                        }
                    }
                    self->streaming_ = false;
                    self->processNextInQueue();
                } else {
                    self->streaming_ = false;
                    self->processNextInQueue();
                }
            }
        });
        t.detach();
    } else {
        auto toolCalls = std::make_shared<std::vector<LlmToolCall>>();
        std::weak_ptr<AgentServer> weakSelf = shared_from_this();
        auto pushEvent = [weakSelf, toolCalls](::LlmEvent ev) {
            auto self = weakSelf.lock();
            if (!self) return;
            if (!ev.tool_calls.empty())
                toolCalls->insert(toolCalls->end(), ev.tool_calls.begin(), ev.tool_calls.end());
            boost::json::object obj;
            obj["type"] = ev.type;
            if (ev.type == "delta" || ev.type == "reasoning")
                obj["text"] = std::move(ev.text);
            if (!ev.msg.empty())
                obj["msg"] = std::move(ev.msg);
            if (!ev.tool_calls.empty()) {
                boost::json::array tcArr;
                for (auto& tc : ev.tool_calls) {
                    boost::json::object tcObj;
                    tcObj["id"] = tc.id;
                    tcObj["function_name"] = tc.function_name;
                    tcObj["function_arguments"] = tc.function_arguments;
                    tcArr.push_back(std::move(tcObj));
                }
                obj["tool_calls"] = std::move(tcArr);
            }
            self->pushOutput(std::move(obj));
        };

        std::weak_ptr<AgentServer> weakSelf2 = shared_from_this();
        auto onDone = [weakSelf2, toolCalls](std::string response, std::string, int, int) {
            auto self = weakSelf2.lock();
            if (!self || self->cancelled_) return;

            boost::json::object am;
            am["role"] = "assistant";
            if (!response.empty()) am["content"] = std::move(response);

            if (!toolCalls->empty()) {
                auto merged = llm::merge_tool_calls(*toolCalls);
                std::vector<LlmToolCall> mergedList;
                for (auto& kv : merged)
                    mergedList.push_back(kv.second);

                boost::json::array tcArr;
                for (auto& tc : mergedList) {
                    boost::json::object tcObj;
                    tcObj["id"] = tc.id;
                    tcObj["type"] = "function";
                    tcObj["function"] = boost::json::object{
                        {"name", tc.function_name},
                        {"arguments", tc.function_arguments}
                    };
                    tcArr.push_back(std::move(tcObj));
                }
                am["tool_calls"] = std::move(tcArr);
            }

            {
                std::lock_guard<std::mutex> lock(self->mtx_);
                self->history_.push_back(std::move(am));
            }

            if (!toolCalls->empty()) {
                auto merged = llm::merge_tool_calls(*toolCalls);
                std::vector<LlmToolCall> mergedList;
                for (auto& kv : merged)
                    mergedList.push_back(kv.second);
                std::vector<boost::json::value> tcValues;
                for (auto& tc : mergedList) {
                    boost::json::object tcj;
                    tcj["id"] = tc.id;
                    tcj["function_name"] = tc.function_name;
                    tcj["function_arguments"] = tc.function_arguments;
                    tcValues.push_back(std::move(tcj));
                }
                self->handleToolCalls(tcValues);
                // Follow-up LLM call via thread (simpler in async context)
                auto weakFollow = weakSelf2;
                std::thread followThread([weakFollow]() {
                    auto s = weakFollow.lock();
                    if (!s || s->cancelled_) return;
                    asio::io_context ioF;
                    std::vector<boost::json::object> hcF;
                    {
                        std::lock_guard<std::mutex> lock(s->mtx_);
                        hcF = s->history_;
                    }
                    boost::json::array msgsF;
                    for (auto& m : hcF) msgsF.push_back(m);
                    std::string bodyF = llm::build_chat_body(msgsF);
                    boost::json::value parsedF = boost::json::parse(bodyF);
                    if (parsedF.is_object()) {
                        boost::json::array toolsF = buildTools();
                        parsedF.as_object()["tools"] = toolsF;
                        parsedF.as_object()["tool_choice"] = "auto";
                        bodyF = boost::json::serialize(parsedF);
                    }
                    std::string fullRespF;
                    std::vector<LlmToolCall> followTcs;
                    auto clientF = std::make_shared<::LlmClient>(ioF);
                    clientF->start("api.deepseek.com", "443", bodyF, "Bearer " + Config::instance().deepSeekApiKey(),
                        [&](LlmEvent ev) {
                            if (s->cancelled_) { clientF->cancel(); return; }
                            if (!ev.tool_calls.empty())
                                followTcs.insert(followTcs.end(), ev.tool_calls.begin(), ev.tool_calls.end());
                        },
                        [&](std::string r, std::string, int, int) {
                            fullRespF = std::move(r);
                        });
                    ioF.run();
                    if (!s->cancelled_ && (!fullRespF.empty() || !followTcs.empty())) {
                        boost::json::object amF;
                        amF["role"] = "assistant";
                        if (!fullRespF.empty()) amF["content"] = std::move(fullRespF);
                        if (!followTcs.empty()) {
                            auto mF = llm::merge_tool_calls(followTcs);
                            boost::json::array tcArrF;
                            for (auto& kvF : mF) {
                                tcArrF.push_back(boost::json::object{
                                    {"id", kvF.second.id}, {"type", "function"},
                                    {"function", {{"name", kvF.second.function_name}, {"arguments", kvF.second.function_arguments}}}
                                });
                            }
                            amF["tool_calls"] = std::move(tcArrF);
                        }
                        {
                            std::lock_guard<std::mutex> lock(s->mtx_);
                            s->history_.push_back(std::move(amF));
                        }
                        if (!followTcs.empty()) {
                            auto mF = llm::merge_tool_calls(followTcs);
                            std::vector<boost::json::value> tcVF;
                            for (auto& kvF : mF) {
                                tcVF.push_back(boost::json::object{
                                    {"id", kvF.second.id}, {"function_name", kvF.second.function_name},
                                    {"function_arguments", kvF.second.function_arguments}
                                });
                            }
                            s->handleToolCalls(tcVF);
                        }
                    }
                    s->streaming_ = false;
                    s->processNextInQueue();
                });
                followThread.detach();
            } else {
                self->streaming_ = false;
                self->processNextInQueue();
            }
        };

        auto stream = std::make_shared<::LlmClient>(*io);
        currentStream_ = stream;
        stream->start("api.deepseek.com", "443",
            body, "Bearer " + apiKey,
            std::move(pushEvent), std::move(onDone));
    }

    boost::json::object start;
    start["type"] = "stream_start";
    pushOutput(std::move(start));
}

boost::json::value AgentServer::executeTool(const std::string& name, const boost::json::value& args)
{
    AGENT_LOG("[tool]", "execute: " << name);

    boost::json::object result;
    result["success"] = true;

    auto& a = args.as_object();

    if (name == "open_app") {
        std::string appName = a.at("app").as_string().c_str();
        std::string config = boost::json::serialize(args);
        AppManager::instance().openApp(appName, config);
        boost::json::object agentMsg;
        agentMsg["type"] = "agent";
        agentMsg["action"] = "open_app";
        agentMsg["app"] = appName;
        pushOutput(std::move(agentMsg));
        result["msg"] = "opened " + appName;
    } else if (name == "control_app") {
        std::string appName = a.at("app").as_string().c_str();
        std::string cmdStr;
        if (a.contains("coord")) {
            auto& coord = a.at("coord").as_array();
            int row = coord[0].as_int64();
            int col = coord[1].as_int64();
            int value = row * 20 + col;
            boost::json::object cmd;
            cmd["action"] = "tick";
            cmd["value"] = value;
            cmdStr = boost::json::serialize(cmd);
        } else if (a.contains("value")) {
            boost::json::object cmd;
            cmd["action"] = "tick";
            cmd["value"] = a.at("value").as_int64();
            cmdStr = boost::json::serialize(cmd);
        } else {
            cmdStr = boost::json::serialize(args);
        }

        bool found = false;
        if (AppManager::instance().getAppState(appName).is_null()) {
            auto mod = Config::instance().chatCachePtr()
                ? reinterpret_cast<IModuleCache*>(static_cast<uintptr_t>(Config::instance().chatCachePtr()))->load(appName)
                : AppModule{};
            if (mod) {
                auto handle = mod.create("{}");
                if (handle) {
                    char* raw = mod.app_process(handle.get(), cmdStr.c_str());
                    if (raw) {
                        result["result"] = boost::json::parse(raw);
                        mod.app_free_string(raw);
                    }
                    found = true;
                }
            }
        } else {
            auto r = AppManager::instance().controlApp(appName, cmdStr);
            if (!r.is_null()) {
                result["result"] = r;
                found = true;
            }
        }

        if (!found) {
            result["success"] = false;
            result["msg"] = "no active session for " + appName;
        }

        boost::json::object agentMsg;
        agentMsg["type"] = "agent";
        agentMsg["action"] = "control_app";
        agentMsg["app"] = appName;
        pushOutput(std::move(agentMsg));
    } else if (name == "close_app") {
        std::string appName = a.at("app").as_string().c_str();
        AppManager::instance().closeApp(appName);
        boost::json::object agentMsg;
        agentMsg["type"] = "agent";
        agentMsg["action"] = "close_app";
        agentMsg["app"] = appName;
        if (a.contains("window_id") && a.at("window_id").is_string())
            agentMsg["window_id"] = a.at("window_id");
        pushOutput(std::move(agentMsg));
        result["msg"] = "closed " + appName;
    } else if (name == "get_app_state") {
        std::string appName = a.at("app").as_string().c_str();
        auto state = AppManager::instance().getAppState(appName);
        if (!state.is_null()) {
            result["state"] = state;
        } else {
            result["success"] = false;
            result["msg"] = "no active session for " + appName;
        }
    } else if (name == "chat_send") {
        std::string text = a.at("text").as_string().c_str();
        boost::json::object cmd;
        cmd["text"] = text;
        auto r = AppManager::instance().controlApp("chat", boost::json::serialize(cmd));
        result["result"] = r.is_null() ? boost::json::value("sent") : r;
    } else if (name == "file_list") {
        std::string path = a.contains("path") ? a.at("path").as_string().c_str() : "/";
        boost::json::object cmd;
        cmd["action"] = "list";
        cmd["path"] = path;
        auto r = AppManager::instance().controlApp("filemanager", boost::json::serialize(cmd));
        if (!r.is_null()) result["entries"] = r;
        else result["success"] = false;
    } else if (name == "file_read") {
        std::string path = a.at("path").as_string().c_str();
        boost::json::object cmd;
        cmd["action"] = "read";
        cmd["path"] = path;
        auto r = AppManager::instance().controlApp("filemanager", boost::json::serialize(cmd));
        if (!r.is_null()) result["file"] = r;
        else result["success"] = false;
    } else if (name == "terminal_exec") {
        std::string command = a.at("command").as_string().c_str();
        boost::json::object cmd;
        cmd["action"] = "exec";
        cmd["command"] = command;
        auto r = AppManager::instance().controlApp("terminal", boost::json::serialize(cmd));
        if (!r.is_null()) result["output"] = r;
        else result["success"] = false;
    } else if (name == "list_active_windows") {
        auto windows = AppManager::instance().listActiveWindows();
        result["windows"] = windows;
        result["count"] = static_cast<int64_t>(windows.size());
    } else {
        result["success"] = false;
        result["msg"] = "unknown tool: " + name;
    }

    return boost::json::value(std::move(result));
}

void AgentServer::handleToolCalls(const std::vector<boost::json::value>& tool_calls)
{
    AGENT_LOG("[tool]", "handling " << tool_calls.size() << " tool calls");

    for (auto& tc : tool_calls) {
        auto& obj = tc.as_object();
        std::string id = obj.at("id").as_string().c_str();
        std::string funcName = obj.at("function_name").as_string().c_str();
        std::string funcArgs = obj.at("function_arguments").as_string().c_str();

        boost::json::value result;
        try {
            auto args = boost::json::parse(funcArgs);
            result = executeTool(funcName, args);
        } catch (const std::exception& e) {
            boost::json::object err;
            err["success"] = false;
            err["msg"] = std::string("error: ") + e.what();
            result = boost::json::value(std::move(err));
        }

        boost::json::object tr;
        tr["role"] = "tool";
        tr["tool_call_id"] = id;
        tr["content"] = boost::json::serialize(result);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            history_.push_back(std::move(tr));
        }
    }
}

void AgentServer::onAppStateChange(const std::string& appName, const boost::json::value& state)
{
    AGENT_LOG("[state]", "app: " << appName << " state changed");
    injectStateIntoHistory(appName, state);
}

void AgentServer::injectStateIntoHistory(const std::string& appName, const boost::json::value& state)
{
    std::lock_guard<std::mutex> lock(mtx_);
    boost::json::object sysMsg;
    sysMsg["role"] = "system";
    std::string content = "应用 " + appName + " 状态变化: " + boost::json::serialize(state);
    sysMsg["content"] = std::move(content);
    history_.push_back(std::move(sysMsg));
    AGENT_LOG("[state]", "injected state for " << appName << " into history");
}

void AgentServer::processNextInQueue()
{
    if (cancelled_) return;
    std::string next;
    if (!inputQueue_.try_pop(next)) {
        AGENT_LOG("[q]", "empty, idle (round " << round_ << ")");
        return;
    }
    AGENT_LOG("[q]", "dequeue (round " << round_ << ", " << inputQueue_.size() << " left)");
    streaming_ = true;
    ++round_;
    handleUserMessage(next);
}

void AgentServer::pushOutput(boost::json::value val)
{
    app_output_fn cb = nullptr;
    void* udata = nullptr;
    std::string s;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        cb = outputCb_;
        udata = outputUdata_;
        if (cb) s = boost::json::serialize(val);
    }
    if (cb) cb(udata, s.c_str());
}

bool AgentServer::openApp(const std::string& name, const std::string& paramsJson)
{
    boost::json::object out;
    out["type"] = "agent";
    out["action"] = "open_app";
    out["app"] = name;
    try {
        out["params"] = boost::json::parse(paramsJson);
    } catch (...) {
        out["params"] = boost::json::object{};
    }
    pushOutput(std::move(out));
    return true;
}

bool AgentServer::controlApp(const std::string& name, const std::string& commandJson)
{
    boost::json::object out;
    out["type"] = "agent";
    out["action"] = "control_app";
    out["app"] = name;
    try {
        out["command"] = boost::json::parse(commandJson);
    } catch (...) {
        out["command"] = boost::json::object{};
    }
    pushOutput(std::move(out));
    return true;
}

bool AgentServer::closeApp(const std::string& name)
{
    boost::json::object out;
    out["type"] = "agent";
    out["action"] = "close_app";
    out["app"] = name;
    pushOutput(std::move(out));
    return true;
}

void AgentServer::stop()
{
    cancelled_ = true;
    if (currentStream_) currentStream_->cancel();
    streaming_ = false;
    inputQueue_.clear();
}

bool AgentServer::isDone() const
{
    return done_;
}

void AgentServer::destroy()
{
    cancelled_ = true;
    stop();
    if (subHandle_) {
        AppManager::instance().unsubscribe(subHandle_);
        subHandle_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        outputCb_ = nullptr;
        outputUdata_ = nullptr;
    }
    currentStream_.reset();
    selfHolder_.reset();
}

std::string AgentServer::process(const std::string& json)
{
    boost::json::array outputs;
    try {
        auto val = boost::json::parse(json);
        if (!val.is_object()) goto done;
        auto& obj = val.as_object();

        auto textIt = obj.find("text");
        if (textIt != obj.end() && textIt->value().is_string())
        {
            std::string text(textIt->value().as_string());
            if (text[0] == '/') {
                boost::json::object embed;
                embed["type"] = "embed";
                embed["kind"] = "text";
                embed["text"] = "命令: " + text;
                outputs.push_back(embed);
            } else {
                onInput(json);
                boost::json::object start;
                start["type"] = "stream_start";
                outputs.push_back(std::move(start));
                boost::json::object msg;
                msg["type"] = "delta";
                msg["text"] = "Agent is processing your request...";
                outputs.push_back(std::move(msg));
            }
        }
    } catch (...) {}

done:
    return boost::json::serialize(outputs);
}

// ============================================================
//  C ABI
// ============================================================

} // namespace agent

extern "C" {

void* app_create(const char* configJson)
{
    (void)configJson;
    auto ptr = std::make_shared<agent::AgentServer>();
    ptr->selfHolder_ = ptr;
    return ptr.get();
}

void app_destroy(void* p)
{
    static_cast<agent::AgentServer*>(p)->destroy();
}

void app_set_output(void* p, app_output_fn cb, void* userdata)
{
    static_cast<agent::AgentServer*>(p)->setOutput(cb, userdata);
}

void app_set_io_context(void* p, void* ioCtx)
{
    static_cast<agent::AgentServer*>(p)->setIoContext(ioCtx);
}

void app_on_input(void* p, const char* inputJson)
{
    static_cast<agent::AgentServer*>(p)->onInput(inputJson);
}

char* app_process(void* p, const char* inputJson)
{
    std::string result = static_cast<agent::AgentServer*>(p)->process(inputJson);
    char* buf = static_cast<char*>(std::malloc(result.size() + 1));
    if (buf) std::memcpy(buf, result.data(), result.size() + 1);
    return buf;
}

void app_free_string(char* str)
{
    std::free(str);
}

int app_is_done(void* p)
{
    return static_cast<agent::AgentServer*>(p)->isDone() ? 1 : 0;
}

} // extern "C"
