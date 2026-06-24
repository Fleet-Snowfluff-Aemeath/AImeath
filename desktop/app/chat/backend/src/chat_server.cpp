#include <app_api.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <utility>

#include <boost/json.hpp>

#include "netconn.hpp"

// ---- ChatApp state ----

struct ChatApp
{
    std::vector<boost::json::object> history;
    std::vector<boost::json::object> pending_outputs; // legacy fallback
    std::mutex mtx;
    std::atomic<bool> cancelled{false};
    std::thread stream_thread;

    // Async output
    app_output_fn output_cb = nullptr;
    void* output_udata = nullptr;

    bool done = false;

    /// Push one output message via callback (async) or into pending (legacy).
    void push_output(boost::json::value val)
    {
        if (val.is_object()) {
            auto& o = val.as_object();
            auto it = o.find("type");
            if (it != o.end() && it->value().is_string() &&
                it->value().as_string() == "stream_end")
                done = true;
        }

        if (output_cb) {
            std::string s = boost::json::serialize(val);
            output_cb(output_udata, s.c_str());
        } else {
            std::lock_guard<std::mutex> lock(mtx);
            pending_outputs.push_back(val.as_object());
        }
    }
};

// ---- DeepSeek streaming ----

static std::string loadDeepSeekApiKey()
{
    std::ifstream f("config.json");
    if (!f.is_open()) {
        std::cerr << "WARNING: config.json not found" << std::endl;
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    try {
        auto val = boost::json::parse(content);
        auto& obj = val.as_object();
        if (obj.contains("deepseek_api_key"))
            return obj["deepseek_api_key"].as_string().c_str();
    } catch (const std::exception& e) {
        std::cerr << "WARNING: failed to parse config.json: " << e.what() << std::endl;
    }
    return "";
}

using PushEvent = std::function<void(boost::json::object)>;

static void callDeepSeek(
    const std::vector<boost::json::object>& history,
    PushEvent push_event,
    std::string& out_full_response,
    std::string& out_full_reasoning)
{
    out_full_response.clear();
    out_full_reasoning.clear();

    boost::json::array msgs;
    for (const auto& m : history) msgs.push_back(m);

    boost::json::object body;
    body["model"] = "deepseek-v4-flash";
    body["messages"] = msgs;
    body["stream"] = true;
    body["thinking"] = {{"type", "enabled"}, {"budget_tokens", 4096}};

    std::string apiKey = loadDeepSeekApiKey();
    if (apiKey.empty()) {
        boost::json::object end;
        end["type"] = "stream_end";
        end["msg"]  = "missing deepseek_api_key in config.json";
        push_event(std::move(end));
        return;
    }

    std::string leftover;

    auto on_chunk = [&](const std::string& chunk) {
        leftover += chunk;
        size_t pos;
        while ((pos = leftover.find("\n\n")) != std::string::npos) {
            std::string event = leftover.substr(0, pos);
            leftover.erase(0, pos + 2);
            std::istringstream ss(event);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("data: ", 0) != 0) continue;
                std::string data = line.substr(6);
                if (data == "[DONE]") continue;
                try {
                    auto val = boost::json::parse(data);
                    auto& choices = val.as_object().at("choices").as_array();
                    if (choices.empty()) continue;
                    auto& delta = choices[0].as_object().at("delta").as_object();

                    if (delta.contains("reasoning_content") && delta.at("reasoning_content").is_string()) {
                        std::string reasoning = delta.at("reasoning_content").as_string().c_str();
                        if (!reasoning.empty()) {
                            out_full_reasoning += reasoning;
                            boost::json::object push_val;
                            push_val["type"] = "reasoning";
                            push_val["text"] = std::move(reasoning);
                            push_event(std::move(push_val));
                        }
                    }
                    if (delta.contains("content") && delta.at("content").is_string()) {
                        std::string content = delta.at("content").as_string().c_str();
                        if (!content.empty()) {
                            out_full_response += content;
                            boost::json::object push_val;
                            push_val["type"] = "delta";
                            push_val["text"] = std::move(content);
                            push_event(std::move(push_val));
                        }
                    }
                } catch (const std::exception&) {}
            }
        }
    };

    try {
        httpsPostStream("api.deepseek.com", "443", "/chat/completions",
            boost::json::serialize(body), "application/json",
            "Bearer " + apiKey, on_chunk);
        // Process remaining data
        size_t pos;
        while ((pos = leftover.find("\n\n")) != std::string::npos) {
            std::string event = leftover.substr(0, pos);
            leftover.erase(0, pos + 2);
            std::istringstream ss(event);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("data: ", 0) != 0) continue;
                std::string data = line.substr(6);
                if (data == "[DONE]") continue;
                try {
                    auto val = boost::json::parse(data);
                    auto& choices = val.as_object().at("choices").as_array();
                    if (choices.empty()) continue;
                    auto& delta = choices[0].as_object().at("delta").as_object();

                    if (delta.contains("reasoning_content") && delta.at("reasoning_content").is_string()) {
                        std::string reasoning = delta.at("reasoning_content").as_string().c_str();
                        if (!reasoning.empty()) {
                            out_full_reasoning += reasoning;
                            boost::json::object push_val;
                            push_val["type"] = "reasoning";
                            push_val["text"] = std::move(reasoning);
                            push_event(std::move(push_val));
                        }
                    }
                    if (delta.contains("content") && delta.at("content").is_string()) {
                        std::string content = delta.at("content").as_string().c_str();
                        if (!content.empty()) {
                            out_full_response += content;
                            boost::json::object push_val;
                            push_val["type"] = "delta";
                            push_val["text"] = std::move(content);
                            push_event(std::move(push_val));
                        }
                    }
                } catch (const std::exception&) {}
            }
        }
    } catch (const std::exception& e) {
        std::string what = e.what();
        if (what.find("eof") == std::string::npos &&
            what.find("end of stream") == std::string::npos) {
            boost::json::object end_val;
            end_val["type"] = "stream_end";
            end_val["msg"] = std::move(what);
            push_event(std::move(end_val));
            return;
        }
    }

    boost::json::object end;
    end["type"] = "stream_end";
    push_event(std::move(end));
}

// ---- Command handling ----

static boost::json::array handleCommand(const std::string& text)
{
    std::string s = text.substr(1);
    size_t endPos = s.find_last_not_of(" \t");
    if (endPos != std::string::npos) s = s.substr(0, endPos + 1);
    size_t sp = s.find_first_of(" \t");
    std::string cmd = (sp == std::string::npos) ? s : s.substr(0, sp);
    std::string arg;
    if (sp != std::string::npos) {
        arg = s.substr(sp + 1);
        size_t first = arg.find_first_not_of(" \t");
        if (first != std::string::npos) arg = arg.substr(first);
    }

    boost::json::object embed;
    embed["type"] = "embed";

    if (s == "图片") {
        embed["kind"] = "image";
        embed["url"]  = "/res/C220748556D18ADBC61177B1A5A8151D.png";
        embed["title"]= "照片";
    } else if (s == "视频") {
        embed["kind"] = "video";
        embed["url"]  = "/res/result.mp4";
        embed["title"]= "视频";
    } else if (s == "音乐") {
        embed["kind"] = "audio";
        embed["url"]  = "/res/超级敏感.wav";
        embed["title"]= "超级敏感";
    } else if (cmd == "游戏" || s.rfind("游戏", 0) == 0) {
        std::string game;
        if (cmd == "游戏") game = arg;
        else if (s.size() > 6) game = s.substr(6);
        if (game.empty()) game = "snake";
        embed["kind"] = "game";
        embed["name"] = game;
        embed["url"]  = "/" + game;
        embed["title"]= "游戏：" + game;
    } else {
        embed["kind"] = "text";
        embed["text"] = "未知命令: /" + cmd;
    }

    boost::json::array result;
    result.push_back(std::move(embed));
    return result;
}

// ---- Streaming helpers ----

/// Drain legacy pending_outputs (only used when output_cb is not set).
static void drainAndPush(ChatApp* app)
{
    std::lock_guard<std::mutex> lock(app->mtx);
    for (auto& obj : app->pending_outputs)
        app->push_output(std::move(obj));
    app->pending_outputs.clear();
}

/// Spawn background thread for DeepSeek, push stream_start immediately.
static void handleUserMessageAsync(ChatApp* app, const std::string& text)
{
    boost::json::object user_msg;
    user_msg["role"] = "user";
    user_msg["content"] = text;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        app->history.push_back(std::move(user_msg));
    }

    std::vector<boost::json::object> history_copy;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        history_copy = app->history;
    }

    ChatApp* app_ptr = app;
    std::thread t([app_ptr, history_copy]() {
        auto push_event = [app_ptr](boost::json::object obj) {
            app_ptr->push_output(std::move(obj));
        };

        std::string full_response, full_reasoning;
        callDeepSeek(history_copy, push_event, full_response, full_reasoning);

        if (!full_response.empty()) {
            std::lock_guard<std::mutex> lock(app_ptr->mtx);
            if (app_ptr->cancelled) return;
            boost::json::object assistant_msg;
            assistant_msg["role"] = "assistant";
            assistant_msg["content"] = std::move(full_response);
            app_ptr->history.push_back(std::move(assistant_msg));
        }
    });

    {
        std::lock_guard<std::mutex> lock(app->mtx);
        if (app->stream_thread.joinable())
            app->stream_thread.detach();
        app->stream_thread = std::move(t);
    }

    // Push stream_start via callback so client begins polling
    boost::json::object start;
    start["type"] = "stream_start";
    app->push_output(std::move(start));
}

// ---- Legacy helpers (for app_process) ----

static boost::json::array drainPollArray(ChatApp* app)
{
    std::lock_guard<std::mutex> lock(app->mtx);
    boost::json::array arr;
    for (auto& obj : app->pending_outputs)
        arr.push_back(std::move(obj));
    app->pending_outputs.clear();

    if (!arr.empty()) {
        auto& last = arr[arr.size() - 1];
        if (last.is_object()) {
            auto& obj = last.as_object();
            auto it = obj.find("type");
            if (it != obj.end() && it->value().is_string() &&
                it->value().as_string() == "stream_end")
                app->done = true;
        }
    }
    return arr;
}

static boost::json::array handleUserMessageLegacy(ChatApp* app, const std::string& text)
{
    boost::json::object user_msg;
    user_msg["role"] = "user";
    user_msg["content"] = text;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        app->history.push_back(std::move(user_msg));
    }

    std::vector<boost::json::object> history_copy;
    {
        std::lock_guard<std::mutex> lock(app->mtx);
        history_copy = app->history;
    }

    ChatApp* app_ptr = app;
    std::thread t([app_ptr, history_copy]() {
        auto push_event = [app_ptr](boost::json::object obj) {
            std::lock_guard<std::mutex> lock(app_ptr->mtx);
            if (app_ptr->cancelled) return;
            app_ptr->pending_outputs.push_back(std::move(obj));
        };

        std::string full_response, full_reasoning;
        callDeepSeek(history_copy, push_event, full_response, full_reasoning);

        if (!full_response.empty()) {
            std::lock_guard<std::mutex> lock(app_ptr->mtx);
            if (app_ptr->cancelled) return;
            boost::json::object assistant_msg;
            assistant_msg["role"] = "assistant";
            assistant_msg["content"] = std::move(full_response);
            app_ptr->history.push_back(std::move(assistant_msg));
        }
    });

    {
        std::lock_guard<std::mutex> lock(app->mtx);
        if (app->stream_thread.joinable())
            app->stream_thread.detach();
        app->stream_thread = std::move(t);
    }

    boost::json::array result;
    boost::json::object start;
    start["type"] = "stream_start";
    result.push_back(std::move(start));
    return result;
}

// ---- Unified App C ABI ----

extern "C"
{

void* app_create(const char* config_json)
{
    (void)config_json;
    return new ChatApp();
}

void app_destroy(void* p)
{
    auto* app = static_cast<ChatApp*>(p);
    app->cancelled = true;
    if (app->stream_thread.joinable())
        app->stream_thread.detach();
    delete app;
}

// ---- Async API ----

void app_on_input(void* p, const char* input_json)
{
    auto* app = static_cast<ChatApp*>(p);

    try {
        auto val = boost::json::parse(input_json);
        if (!val.is_object()) return;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string() &&
            action_it->value().as_string() == "poll") {
            drainAndPush(app);
            return;
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                auto arr = handleCommand(text);
                for (auto& item : arr)
                    app->push_output(std::move(item));
            } else {
                handleUserMessageAsync(app, text);
            }
        }
    } catch (...) {}
}

void app_set_output(void* p, app_output_fn cb, void* userdata)
{
    auto* app = static_cast<ChatApp*>(p);
    app->output_cb = cb;
    app->output_udata = userdata;
}

// ---- Legacy sync API ----

char* app_process(void* p, const char* input_json)
{
    auto* app = static_cast<ChatApp*>(p);
    boost::json::array outputs;
    std::string msg(input_json);

    try {
        auto val = boost::json::parse(msg);
        if (!val.is_object()) goto done;
        auto& obj = val.as_object();

        auto action_it = obj.find("action");
        if (action_it != obj.end() && action_it->value().is_string() &&
            action_it->value().as_string() == "poll") {
            outputs = drainPollArray(app);
            goto done;
        }

        auto text_it = obj.find("text");
        if (text_it != obj.end() && text_it->value().is_string()) {
            std::string text(text_it->value().as_string());
            if (text[0] == '/') {
                outputs = handleCommand(text);
            } else {
                outputs = handleUserMessageLegacy(app, text);
            }
        }
    } catch (...) {}

done:
    std::string result = boost::json::serialize(outputs);
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
    return static_cast<ChatApp*>(p)->done ? 1 : 0;
}

} // extern "C"
