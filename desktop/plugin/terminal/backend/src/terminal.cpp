#include "plugin.hpp"
#include "terminal.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <array>
#include <thread>

#include <boost/json.hpp>

#include "pty_session.hpp"

namespace json = boost::json;

static char* makeReply(const json::array& arr)
{
    auto s = json::serialize(arr);
    auto* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (buf) std::memcpy(buf, s.data(), s.size() + 1);
    return buf;
}

struct TerminalState
{
    std::unique_ptr<PtySession> session;
    plugin_output_fn output_cb = nullptr;
    void* output_udata = nullptr;

    void pushOutput(const std::string& text)
    {
        if (!output_cb || text.empty()) return;
        json::object msg;
        msg["type"] = "output";
        msg["text"] = text;
        output_cb(output_udata, json::serialize(msg).c_str());
    }
};

// 全局 io_context（TermSession 原有模式）
struct TerminalIoContext
{
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;
    std::thread thread;

    TerminalIoContext()
        : work(boost::asio::make_work_guard(io)),
          thread([this] { io.run(); })
    {}

    ~TerminalIoContext()
    {
        work.reset();
        io.stop();
        if (thread.joinable())
            thread.join();
    }
};

static boost::asio::io_context& terminalIo()
{
    static TerminalIoContext ctx;
    return ctx.io;
}

extern "C"
{

void* plugin_create(const char* config_json)
{
    (void)config_json;
    auto* state = new TerminalState();
    state->session = std::make_unique<PtySession>(terminalIo());
    return state;
}

void plugin_destroy(void* p)
{
    auto* state = static_cast<TerminalState*>(p);
    state->session->close();
    delete state;
}

void plugin_free_string(char* s)
{
    std::free(s);
}

int plugin_is_done(void* p)
{
    auto* state = static_cast<TerminalState*>(p);
    return state->session->isAlive() ? 0 : 1;
}

void plugin_on_input(void* p, const char* input_json)
{
    auto* state = static_cast<TerminalState*>(p);

    auto sendErr = [state](const std::string& msg) {
        state->pushOutput("ERROR: " + msg);
    };

    try
    {
        auto val = json::parse(input_json);
        if (!val.is_object()) { sendErr("not an object"); return; }
        auto& obj = val.as_object();

        auto it = obj.find("action");
        if (it == obj.end() || !it->value().is_string()) { sendErr("missing action"); return; }
        std::string action(it->value().as_string());

        if (action == "exec")
        {
            auto cmdIt = obj.find("cmd");
            if (cmdIt == obj.end() || !cmdIt->value().is_string()) { sendErr("missing cmd"); return; }
            std::string cmd(cmdIt->value().as_string());
            state->session->startAsync(cmd);
        }
        else if (action == "stdin")
        {
            auto dataIt = obj.find("data");
            if (dataIt == obj.end() || !dataIt->value().is_string()) { sendErr("missing data"); return; }
            state->session->write(std::string(dataIt->value().as_string()));
        }
        else if (action == "stdout")
        {
            // 异步读取实时推送，无需轮询
        }
        else if (action == "resize")
        {
            int rows = 24, cols = 80;
            auto rit = obj.find("rows");
            if (rit != obj.end() && rit->value().is_int64())
                rows = (int)rit->value().as_int64();
            auto cit = obj.find("cols");
            if (cit != obj.end() && cit->value().is_int64())
                cols = (int)cit->value().as_int64();
            state->session->resize(rows, cols);
        }
        else
        {
            sendErr("unknown action: " + action);
        }
    }
    catch (const std::exception& e)
    {
        sendErr(e.what());
    }
}

void plugin_set_output(void* p, plugin_output_fn cb, void* userdata)
{
    auto* state = static_cast<TerminalState*>(p);
    state->output_cb = cb;
    state->output_udata = userdata;

    state->session->setOutput([state](const std::string& text) {
        state->pushOutput(text);
    });
}

char* plugin_process(void* p, const char* input_json)
{
    auto* state = static_cast<TerminalState*>(p);

    try
    {
        auto val = json::parse(input_json);
        if (!val.is_object())
            return makeReply(json::array{json::object{{"type","error"},{"msg","not an object"}}});

        auto& obj = val.as_object();
        auto it = obj.find("action");
        if (it == obj.end() || !it->value().is_string())
            return makeReply(json::array{json::object{{"type","error"},{"msg","missing action"}}});

        std::string action(it->value().as_string());

        if (action == "exec")
        {
            auto cmdIt = obj.find("cmd");
            if (cmdIt == obj.end() || !cmdIt->value().is_string())
                return makeReply(json::array{json::object{{"type","error"},{"msg","missing cmd"}}});

            std::string cmd(cmdIt->value().as_string());
            if (!state->session->start(cmd))
                return makeReply(json::array{json::object{{"type","error"},{"msg","forkpty failed"}}});

            usleep(150000);
            return makeReply(json::array{json::object{{"type","output"},{"text",""}}});
        }
        else if (action == "exec_sync")
        {
            auto cmdIt = obj.find("command");
            if (cmdIt == obj.end() || !cmdIt->value().is_string())
                return makeReply(json::array{json::object{{"type","error"},{"msg","missing command"}}});

            std::string cmd(cmdIt->value().as_string());
            std::string output;
            std::array<char, 4096> buf;

            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe)
                return makeReply(json::array{json::object{{"type","error"},{"msg","popen failed"}}});

            while (fgets(buf.data(), buf.size(), pipe))
                output += buf.data();
            int rc = pclose(pipe);
            if (rc != 0 && output.empty())
                return makeReply(json::array{json::object{{"type","error"},{"msg","command failed: " + cmd}}});

            return makeReply(json::array{json::object{{"type","output"},{"text",std::move(output)}}});
        }
        else if (action == "stdin")
        {
            auto dataIt = obj.find("data");
            if (dataIt == obj.end() || !dataIt->value().is_string())
                return makeReply(json::array{json::object{{"type","error"},{"msg","missing data"}}});

            state->session->write(std::string(dataIt->value().as_string()));
            return makeReply(json::array{json::object{{"type","output"},{"text",""}}});
        }
        else if (action == "stdout")
        {
            return makeReply(json::array{json::object{{"type","output"},{"text",""}}});
        }
        else if (action == "resize")
        {
            int rows = 24, cols = 80;
            auto rit = obj.find("rows");
            if (rit != obj.end() && rit->value().is_int64())
                rows = (int)rit->value().as_int64();
            auto cit = obj.find("cols");
            if (cit != obj.end() && cit->value().is_int64())
                cols = (int)cit->value().as_int64();
            state->session->resize(rows, cols);
            return makeReply(json::array{});
        }

        return makeReply(json::array{json::object{{"type","error"},{"msg","unknown action: "+action}}});
    }
    catch (const std::exception& e)
    {
        return makeReply(json::array{json::object{{"type","error"},{"msg",std::string(e.what())}}});
    }
}

char* plugin_get_info(void)
{
    const char* json = "{\"name\":\"terminal\",\"display_name\":\"终端\",\"type\":\"terminal\"}";
    char* buf = (char*)malloc(strlen(json) + 1);
    if (buf) memcpy(buf, json, strlen(json) + 1);
    return buf;
}

} // extern "C"
