#include <gtest/gtest.h>
#include "llm_client.hpp"
#include "llm_utils.hpp"

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <yaml-cpp/yaml.h>
#include <set>

namespace {

boost::json::array& llmTestTools() {
    static boost::json::array tools;
    if (tools.empty()) {
        YAML::Node config = YAML::LoadFile("../../agent/config/tools.yml");
        for (auto t : config["tools"]) {
            boost::json::object tool;
            tool["type"] = "function";
            boost::json::object func;
            func["name"] = t["name"].as<std::string>();
            func["description"] = t["description"].as<std::string>();
            boost::json::object params;
            params["type"] = "object";
            boost::json::object props;
            if (t["params"]["properties"])
                for (auto prop : t["params"]["properties"]) {
                    boost::json::object p;
                    p["type"] = prop.second["type"].as<std::string>();
                    if (prop.second["description"])
                        p["description"] = prop.second["description"].as<std::string>();
                    props[prop.first.as<std::string>()] = std::move(p);
                }
            params["properties"] = std::move(props);
            boost::json::array required;
            if (t["params"]["required"])
                for (auto r : t["params"]["required"])
                    required.push_back(boost::json::string(r.as<std::string>()));
            params["required"] = std::move(required);
            func["parameters"] = std::move(params);
            tool["function"] = std::move(func);
            tools.push_back(std::move(tool));
        }
    }
    return tools;
}

}

// ====== Llm data structs ======

TEST(LlmEventTest, DefaultValues)
{
    LlmEvent ev;
    EXPECT_TRUE(ev.type.empty());
    EXPECT_TRUE(ev.text.empty());
    EXPECT_TRUE(ev.msg.empty());
    EXPECT_TRUE(ev.finish_reason.empty());
    EXPECT_TRUE(ev.tool_calls.empty());
    EXPECT_EQ(ev.usage.prompt_tokens, 0);
}

TEST(LlmUsageTest, DefaultValues)
{
    LlmUsage usage;
    EXPECT_EQ(usage.prompt_tokens, 0);
    EXPECT_EQ(usage.completion_tokens, 0);
    EXPECT_EQ(usage.total_tokens, 0);
    EXPECT_EQ(usage.prompt_cache_hit_tokens, 0);
    EXPECT_EQ(usage.prompt_cache_miss_tokens, 0);
    EXPECT_EQ(usage.reasoning_tokens, 0);
}

TEST(LlmToolCallTest, DefaultValues)
{
    LlmToolCall tc;
    EXPECT_TRUE(tc.id.empty());
    EXPECT_TRUE(tc.type.empty());
    EXPECT_TRUE(tc.function_name.empty());
    EXPECT_TRUE(tc.function_arguments.empty());
}

// ====== LlmClient ======

TEST(LlmClientTest, CreateDestroy)
{
    boost::asio::io_context io;
    {
        auto client = std::make_shared<LlmClient>(io);
        EXPECT_NE(client, nullptr);
    }
}

TEST(LlmClientTest, MultipleCreateDestroy)
{
    boost::asio::io_context io;
    for (int i = 0; i < 10; ++i)
    {
        auto client = std::make_shared<LlmClient>(io);
        EXPECT_NE(client, nullptr);
    }
}

TEST(LlmClientTest, CancelWithoutStart)
{
    boost::asio::io_context io;
    {
        auto client = std::make_shared<LlmClient>(io);
        client->cancel();
    }
}

TEST(LlmClientTest, DoubleCancel)
{
    boost::asio::io_context io;
    auto client = std::make_shared<LlmClient>(io);
    EXPECT_NO_THROW(client->cancel());
    EXPECT_NO_THROW(client->cancel());
}

TEST(LlmClientTest, CancelAfterStartBeforeResolve)
{
    boost::asio::io_context io;
    auto client = std::make_shared<LlmClient>(io);
    client->start("api.deepseek.com", "443", "{}", "",
        [](LlmEvent) {}, [](std::string, std::string, int, int) {});
    client->cancel();
    io.run_for(std::chrono::milliseconds(100));
}

TEST(LlmClientTest, StartWithInvalidHost)
{
    boost::asio::io_context io;
    bool event_called = false;
    auto client = std::make_shared<LlmClient>(io);
    client->start("nonexistent.invalid", "443", "{}", "",
        [&](LlmEvent ev) {
            if (ev.type == "stream_end") event_called = true;
        },
        [](std::string, std::string, int, int) {},
        std::chrono::seconds(2));
    io.run_for(std::chrono::seconds(5));
    EXPECT_TRUE(event_called);
}

TEST(LlmClientTest, StartWithEmptyBody)
{
    boost::asio::io_context io;
    bool event_called = false;
    auto client = std::make_shared<LlmClient>(io);
    client->start("nonexistent.invalid", "443", "", "",
        [&](LlmEvent ev) {
            if (ev.type == "stream_end") event_called = true;
        },
        [](std::string, std::string, int, int) {},
        std::chrono::seconds(2));
    io.run_for(std::chrono::seconds(5));
    EXPECT_TRUE(event_called);
}

TEST(LlmClientTest, SslVerifyFlag)
{
    EXPECT_NO_THROW(LlmClient::enable_ssl_verify(true));
    EXPECT_NO_THROW(LlmClient::enable_ssl_verify(false));
    LlmClient::enable_ssl_verify(true);
}

TEST(LlmClientTest, StartWithCustomTarget)
{
    boost::asio::io_context io;
    bool event_called = false;
    auto client = std::make_shared<LlmClient>(io);
    client->start("nonexistent.invalid", "443", "{}", "",
        [&](LlmEvent ev) {
            if (ev.type == "stream_end") event_called = true;
        },
        [](std::string, std::string, int, int) {},
        std::chrono::seconds(2),
        "/v1/chat/completions",
        std::chrono::seconds(10));
    io.run_for(std::chrono::seconds(5));
    EXPECT_TRUE(event_called);
}

TEST(LlmClientTest, StartWithConnectTimeout)
{
    boost::asio::io_context io;
    bool event_called = false;
    auto client = std::make_shared<LlmClient>(io);
    client->start("nonexistent.invalid", "443", "{}", "",
        [&](LlmEvent ev) {
            if (ev.type == "stream_end") event_called = true;
        },
        [](std::string, std::string, int, int) {},
        std::chrono::seconds(2),
        "/chat/completions",
        std::chrono::seconds(1));
    io.run_for(std::chrono::seconds(5));
    EXPECT_TRUE(event_called);
}

TEST(LlmClientTest, DefaultConstructible)
{
    LlmClient::enable_ssl_verify(true);
    boost::asio::io_context io;
    {
        auto client = std::make_shared<LlmClient>(io);
        EXPECT_NE(client, nullptr);
        LlmClient::enable_ssl_verify(false);
    }
    {
        auto client = std::make_shared<LlmClient>(io);
        EXPECT_NE(client, nullptr);
    }
    LlmClient::enable_ssl_verify(true);
}

// ====== LlmRequest ======

TEST(LlmRequestTest, PostToInvalidHost)
{
    LlmClient::enable_ssl_verify(false);
    auto resp = LlmRequest::post("nonexistent.invalid", "443",
        "/chat/completions", "{\"model\":\"test\"}", "",
        std::chrono::seconds(2), std::chrono::seconds(1));
    EXPECT_TRUE(!resp.error.empty() || resp.status == 0);
    LlmClient::enable_ssl_verify(true);
}

TEST(LlmRequestTest, PostWithEmptyBody)
{
    LlmClient::enable_ssl_verify(false);
    auto resp = LlmRequest::post("nonexistent.invalid", "443",
        "/chat/completions", "", "",
        std::chrono::seconds(2), std::chrono::seconds(1));
    EXPECT_TRUE(!resp.error.empty() || resp.status == 0);
    LlmClient::enable_ssl_verify(true);
}

TEST(LlmRequestTest, GetToInvalidHost)
{
    LlmClient::enable_ssl_verify(false);
    auto resp = LlmRequest::get("nonexistent.invalid", "443",
        "/v1/models", "",
        std::chrono::seconds(2), std::chrono::seconds(1));
    EXPECT_TRUE(!resp.error.empty() || resp.status == 0);
    LlmClient::enable_ssl_verify(true);
}

TEST(LlmRequestTest, GetWithCustomTarget)
{
    LlmClient::enable_ssl_verify(false);
    auto resp = LlmRequest::get("nonexistent.invalid", "443",
        "/v1/engines", "",
        std::chrono::seconds(2), std::chrono::seconds(1));
    EXPECT_TRUE(!resp.error.empty() || resp.status == 0);
    LlmClient::enable_ssl_verify(true);
}

TEST(LlmRequestTest, ResponseFields)
{
    LlmResponse resp;
    EXPECT_EQ(resp.status, 0);
    EXPECT_TRUE(resp.body.empty());
    EXPECT_TRUE(resp.error.empty());

    resp.status = 200;
    resp.body = "{\"ok\":true}";
    EXPECT_EQ(resp.status, 200);
    EXPECT_EQ(resp.body, "{\"ok\":true}");
}

TEST(LlmRequestTest, ResponseCopy)
{
    LlmResponse resp;
    resp.status = 500;
    resp.body = "error body";
    resp.error = "connection failed";

    LlmResponse copy = resp;
    EXPECT_EQ(copy.status, 500);
    EXPECT_EQ(copy.body, "error body");
    EXPECT_EQ(copy.error, "connection failed");

    copy.status = 200;
    EXPECT_EQ(resp.status, 500);
}

TEST(LlmRequestTest, SslVerifyToggleAffectsRequest)
{
    LlmClient::enable_ssl_verify(false);
    LlmResponse resp;
    EXPECT_NO_THROW({
        resp = LlmRequest::get("nonexistent.invalid", "443",
            "/v1/models", "",
            std::chrono::seconds(1), std::chrono::seconds(1));
    });
    LlmClient::enable_ssl_verify(true);
}

// ====== llm_utils ======

TEST(LlmUtilsTest, BuildChatBody)
{
    boost::json::array msgs;
    boost::json::object msg;
    msg["role"] = "user";
    msg["content"] = "hello";
    msgs.push_back(msg);

    std::string body = llm::build_chat_body(msgs, "test-model", true, false);
    auto parsed = boost::json::parse(body);
    auto& obj = parsed.as_object();
    EXPECT_EQ(obj["model"].as_string(), std::string("test-model"));
    EXPECT_TRUE(obj["stream"].as_bool());
    EXPECT_FALSE(obj.contains("thinking"));
}

TEST(LlmUtilsTest, BuildChatBodyDefaultModel)
{
    boost::json::array msgs;
    boost::json::object msg;
    msg["role"] = "user";
    msg["content"] = "hi";
    msgs.push_back(msg);

    std::string body = llm::build_chat_body(msgs);
    auto parsed = boost::json::parse(body);
    EXPECT_EQ(parsed.as_object()["model"].as_string(), std::string("deepseek-v4-flash"));
}

TEST(LlmUtilsTest, BuildChatBodyWithThinking)
{
    boost::json::array msgs;
    boost::json::object msg;
    msg["role"] = "user";
    msg["content"] = "hello";
    msgs.push_back(msg);

    std::string body = llm::build_chat_body(msgs, "test", true, true);
    auto parsed = boost::json::parse(body);
    auto& obj = parsed.as_object();
    EXPECT_TRUE(obj.contains("thinking"));
}

TEST(LlmUtilsTest, BuildChatBodyNoStream)
{
    boost::json::array msgs;
    std::string body = llm::build_chat_body(msgs, "test", false, false);
    auto parsed = boost::json::parse(body);
    EXPECT_FALSE(parsed.as_object()["stream"].as_bool());
}

TEST(LlmUtilsTest, BuildChatBodyMultipleMessages)
{
    boost::json::array msgs;
    {
        boost::json::object sys;
        sys["role"] = "system";
        sys["content"] = "You are a helpful assistant.";
        msgs.push_back(sys);
    }
    {
        boost::json::object user;
        user["role"] = "user";
        user["content"] = "Hello";
        msgs.push_back(user);
    }
    {
        boost::json::object assistant;
        assistant["role"] = "assistant";
        assistant["content"] = "Hi! How can I help?";
        msgs.push_back(assistant);
    }

    std::string body = llm::build_chat_body(msgs, "test", true, false);
    auto parsed = boost::json::parse(body);
    auto& msgs_out = parsed.as_object()["messages"].as_array();
    EXPECT_EQ(msgs_out.size(), 3u);
    EXPECT_EQ(msgs_out[0].as_object()["role"].as_string(), std::string("system"));
    EXPECT_EQ(msgs_out[1].as_object()["role"].as_string(), std::string("user"));
    EXPECT_EQ(msgs_out[2].as_object()["role"].as_string(), std::string("assistant"));
}

TEST(LlmUtilsTest, BuildChatBodyEmptyMessages)
{
    boost::json::array msgs;
    std::string body = llm::build_chat_body(msgs, "test", false, false);
    auto parsed = boost::json::parse(body);
    EXPECT_TRUE(parsed.as_object()["messages"].as_array().empty());
}

TEST(LlmUtilsTest, GetToolsFromYaml)
{
    auto& tools = llmTestTools();
    EXPECT_GE(tools.size(), 4u);

    std::set<std::string> names;
    for (auto& t : tools) {
        auto& obj = t.as_object();
        EXPECT_EQ(obj["type"].as_string(), std::string("function"));
        names.insert(obj["function"].as_object()["name"].as_string().c_str());
    }
    EXPECT_TRUE(names.count("open_app"));
    EXPECT_TRUE(names.count("control_app"));
    EXPECT_TRUE(names.count("close_app"));
    EXPECT_TRUE(names.count("get_app_state"));
}

TEST(LlmUtilsTest, GetToolsCount)
{
    auto& tools = llmTestTools();
    EXPECT_GE(tools.size(), 12u);
}

TEST(LlmUtilsTest, InjectTools)
{
    std::string body = R"({"model":"test","messages":[]})";
    llm::inject_tools(body, true, {}, llmTestTools());
    auto parsed = boost::json::parse(body);
    auto& obj = parsed.as_object();
    EXPECT_TRUE(obj.contains("tools"));
    EXPECT_EQ(obj["tool_choice"].as_string(), std::string("auto"));
}

TEST(LlmUtilsTest, InjectToolsDisabled)
{
    std::string body = R"({"model":"test","messages":[]})";
    std::string original = body;
    llm::inject_tools(body, false, {}, llmTestTools());
    EXPECT_EQ(body, original);
}

TEST(LlmUtilsTest, InjectToolsOnNonObject)
{
    std::string body = R"("just a string, not an object")";
    std::string original = body;
    llm::inject_tools(body, true, {}, llmTestTools());
    EXPECT_EQ(body, original);
}

TEST(LlmUtilsTest, MergeToolCallsBasic)
{
    std::vector<LlmToolCall> chunks;
    LlmToolCall tc;
    tc.id = "call_1";
    tc.function_name = "open_app";
    tc.function_arguments = "{\"app\":\"snake\"}";
    chunks.push_back(tc);

    auto merged = llm::merge_tool_calls(chunks);
    EXPECT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged["call_1"].function_name, "open_app");
}

TEST(LlmUtilsTest, MergeToolCallsChunked)
{
    std::vector<LlmToolCall> chunks;

    LlmToolCall tc1;
    tc1.id = "call_1";
    tc1.function_name = "open_app";
    tc1.function_arguments = "{\"app\":";
    chunks.push_back(tc1);

    LlmToolCall tc2;
    tc2.function_name = "open_app";
    tc2.function_arguments = "\"snake\"}";
    chunks.push_back(tc2);

    auto merged = llm::merge_tool_calls(chunks);
    EXPECT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged["call_1"].function_arguments, "{\"app\":\"snake\"}");
}

TEST(LlmUtilsTest, MergeToolCallsMultipleIds)
{
    std::vector<LlmToolCall> chunks;

    LlmToolCall tc1;
    tc1.id = "call_a";
    tc1.function_name = "open_app";
    tc1.function_arguments = "{\"app\":\"snake\"}";
    chunks.push_back(tc1);

    LlmToolCall tc2;
    tc2.id = "call_b";
    tc2.function_name = "close_app";
    tc2.function_arguments = "{\"app\":\"snake\"}";
    chunks.push_back(tc2);

    auto merged = llm::merge_tool_calls(chunks);
    EXPECT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged["call_a"].function_name, "open_app");
    EXPECT_EQ(merged["call_b"].function_name, "close_app");
}

TEST(LlmUtilsTest, MergeToolCallsEmpty)
{
    std::vector<LlmToolCall> chunks;
    auto merged = llm::merge_tool_calls(chunks);
    EXPECT_TRUE(merged.empty());
}

TEST(LlmUtilsTest, MergeToolCallsWithoutNameInFirstChunk)
{
    std::vector<LlmToolCall> chunks;

    LlmToolCall tc1;
    tc1.id = "call_x";
    tc1.function_arguments = "{\"x\":";
    chunks.push_back(tc1);

    LlmToolCall tc2;
    tc2.id = "call_x";
    tc2.function_name = "list_apps";
    tc2.function_arguments = "1}";
    chunks.push_back(tc2);

    auto merged = llm::merge_tool_calls(chunks);
    EXPECT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged["call_x"].function_name, "list_apps");
    EXPECT_EQ(merged["call_x"].function_arguments, "{\"x\":1}");
}

TEST(LlmUtilsTest, MergeToolCallsStress)
{
    std::vector<LlmToolCall> chunks;
    constexpr int N = 500;
    for (int i = 0; i < N; ++i)
    {
        LlmToolCall tc;
        tc.id = "call_" + std::to_string(i % 20);
        tc.function_name = "func" + std::to_string(i % 20);
        tc.function_arguments = "x";
        chunks.push_back(tc);
    }

    auto merged = llm::merge_tool_calls(chunks);
    EXPECT_EQ(merged.size(), 20u);
    EXPECT_EQ(merged["call_0"].function_arguments.size(), (N / 20) + (N % 20 > 0 ? 1 : 0));
}
