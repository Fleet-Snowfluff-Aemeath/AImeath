#include "app_api.hpp"
#include "terminal.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <array>
#include <thread>

#include <boost/json.hpp>

namespace json = boost::json;

static char* makeReply(const json::array& arr)
{
    auto s = json::serialize(arr);
    auto* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (buf) std::memcpy(buf, s.data(), s.size() + 1);
    return buf;
}

// ============================================================
//  C ABI
// ============================================================

extern "C"
{

void* app_create(const char* config_json)
{
    (void)config_json;
    auto* ses = new TermSession();
    return ses;
}

void app_destroy(void* p)
{
    auto* ses = static_cast<TermSession*>(p);
    ses->close();
    delete ses;
}

void app_free_string(char* s)
{
    std::free(s);
}

int app_is_done(void* p)
{
    auto* ses = static_cast<TermSession*>(p);
    return ses->isAlive() ? 0 : 1;
}

// ---- 异步 API ----

void app_on_input(void* p, const char* input_json)
{
    auto* ses = static_cast<TermSession*>(p);

    auto sendErr = [ses](const std::string& msg) {
        ses->push_output("ERROR: " + msg);
    };

    try
    {
        auto val = json::parse(input_json);
        if (!val.is_object()) {
            sendErr("not an object");
            return;
        }
        auto& obj = val.as_object();

        auto it = obj.find("action");
        if (it == obj.end() || !it->value().is_string()) {
            sendErr("missing action");
            return;
        }
        std::string action(it->value().as_string());

        if (action == "exec")
        {
            auto cmdIt = obj.find("cmd");
            if (cmdIt == obj.end() || !cmdIt->value().is_string()) {
                sendErr("missing cmd");
                return;
            }
            std::string cmd(cmdIt->value().as_string());

            if (ses->child_pid > 0) {
                sendErr("terminal already running");
                return;
            }

            ses->start_with_async(cmd);
            // Prompt 由异步 PTY 读取自动推送，无需手动等待
        }
        else if (action == "stdin")
        {
            auto dataIt = obj.find("data");
            if (dataIt == obj.end() || !dataIt->value().is_string()) {
                sendErr("missing data");
                return;
            }
            std::string data(dataIt->value().as_string());
            ses->writeInput(data);
            // Async reads will push output, no need to block here
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
            ses->setSize(rows, cols);
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

void app_set_output(void* p, app_output_fn cb, void* userdata)
{
    auto* ses = static_cast<TermSession*>(p);
    ses->output_cb = cb;
    ses->output_udata = userdata;
}

// ---- 同步 API（遗留兼容） ----

char* app_process(void* p, const char* input_json)
{
    auto* ses = static_cast<TermSession*>(p);

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

            if (!ses->start(cmd))
                return makeReply(json::array{json::object{{"type","error"},{"msg","forkpty failed"}}});

            usleep(150000);
            std::string output = ses->readOutput();
            return makeReply(json::array{json::object{
                {"type", "output"},
                {"text", output}
            }});
        }
        else if (action == "stdin")
        {
            auto dataIt = obj.find("data");
            if (dataIt == obj.end() || !dataIt->value().is_string())
                return makeReply(json::array{json::object{{"type","error"},{"msg","missing data"}}});

            std::string data(dataIt->value().as_string());
            ses->writeInput(data);
            std::string output = ses->readOutput();
            return makeReply(json::array{json::object{
                {"type", "output"},
                {"text", output}
            }});
        }
        else if (action == "stdout")
        {
            std::string output = ses->readPending();
            return makeReply(json::array{json::object{
                {"type", "output"},
                {"text", output}
            }});
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
            ses->setSize(rows, cols);
            return makeReply(json::array{});
        }

        return makeReply(json::array{json::object{{"type","error"},{"msg","unknown action: "+action}}});
    }
    catch (const std::exception& e)
    {
        return makeReply(json::array{json::object{{"type","error"},{"msg",std::string(e.what())}}});
    }
}

} // extern "C"
