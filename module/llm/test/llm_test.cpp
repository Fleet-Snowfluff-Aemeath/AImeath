#include <gtest/gtest.h>
#include "llm_client.hpp"
#include "llm_utils.hpp"

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <set>

// ====== LlmClient ======

TEST(LlmClientTest, CreateDestroy)
{
    boost::asio::io_context io;
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

TEST(LlmRequestTest, GetToInvalidHost)
{
    LlmClient::enable_ssl_verify(false);
    auto resp = LlmRequest::get("nonexistent.invalid", "443",
        "/v1/models", "",
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

TEST(LlmUtilsTest, GetDefaultTools)
{
    auto tools = llm::get_default_tools();
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

TEST(LlmUtilsTest, InjectTools)
{
    std::string body = R"({"model":"test","messages":[]})";
    llm::inject_tools(body, true);
    auto parsed = boost::json::parse(body);
    auto& obj = parsed.as_object();
    EXPECT_TRUE(obj.contains("tools"));
    EXPECT_EQ(obj["tool_choice"].as_string(), std::string("auto"));
}

TEST(LlmUtilsTest, InjectToolsDisabled)
{
    std::string body = R"({"model":"test","messages":[]})";
    std::string original = body;
    llm::inject_tools(body, false);
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
