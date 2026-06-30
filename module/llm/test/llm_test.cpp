#include <gtest/gtest.h>
#include "llm_client.hpp"

#include <boost/asio.hpp>

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
