#pragma once

#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include <vector>

namespace boost::asio {
class io_context;
}

struct LlmToolCall {
    std::string id;
    std::string type;
    std::string function_name;
    std::string function_arguments;
};

struct LlmUsage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
    int prompt_cache_hit_tokens = 0;
    int prompt_cache_miss_tokens = 0;
    int reasoning_tokens = 0;
};

struct LlmEvent {
    std::string type;
    std::string text;
    std::string msg;
    std::string finish_reason;
    std::vector<LlmToolCall> tool_calls;
    LlmUsage usage;
};

struct LlmResponse {
    int status = 0;
    std::string body;
    std::string error;
};

using LlmEventFn = std::function<void(LlmEvent)>;
using LlmDoneFn = std::function<void(std::string full_response, std::string full_reasoning,
                                     int prompt_tokens, int completion_tokens)>;

class LlmClient
{
public:
    explicit LlmClient(boost::asio::io_context& io);
    ~LlmClient();

    void start(const std::string& host, const std::string& port,
               const std::string& body, const std::string& auth,
               LlmEventFn on_event, LlmDoneFn on_done,
               std::chrono::seconds timeout = std::chrono::seconds(120),
               const std::string& target = "/chat/completions",
               std::chrono::seconds connect_timeout = std::chrono::seconds(30));

    void cancel();

    static void enable_ssl_verify(bool enabled);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

class LlmRequest
{
public:
    static LlmResponse post(const std::string& host, const std::string& port,
                            const std::string& target, const std::string& body,
                            const std::string& auth,
                            std::chrono::seconds timeout = std::chrono::seconds(120),
                            std::chrono::seconds connect_timeout = std::chrono::seconds(30));

    static LlmResponse get(const std::string& host, const std::string& port,
                           const std::string& target, const std::string& auth,
                           std::chrono::seconds timeout = std::chrono::seconds(30),
                           std::chrono::seconds connect_timeout = std::chrono::seconds(10));
};
