#include <gtest/gtest.h>
#include "netconn.hpp"
#include "toolbox.hpp"
#include "threadmgr.hpp"
#include "logger.hpp"

// ====== URL Parsing ======

TEST(ToolboxTest, ParseHttpUrl)
{
    auto url = parseUrl("http://example.com/path");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 80);
    EXPECT_EQ(url.path, "/path");
    EXPECT_FALSE(url.is_ws);
}

TEST(ToolboxTest, ParseHttpUrlDefaultPath)
{
    auto url = parseUrl("http://example.com");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 80);
    EXPECT_EQ(url.path, "/");
}

TEST(ToolboxTest, ParseHttpUrlCustomPort)
{
    auto url = parseUrl("http://example.com:8080/api");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 8080);
    EXPECT_EQ(url.path, "/api");
}

TEST(ToolboxTest, ParseWsUrl)
{
    auto url = parseUrl("ws://chat.example.com/room");
    EXPECT_EQ(url.host, "chat.example.com");
    EXPECT_EQ(url.port, 80);
    EXPECT_EQ(url.path, "/room");
    EXPECT_TRUE(url.is_ws);
}

TEST(ToolboxTest, ParseHttpsUrl)
{
    auto url = parseUrl("https://secure.com/api/v1");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/api/v1");
}

TEST(ToolboxTest, ParseWssUrl)
{
    auto url = parseUrl("wss://secure.com/chat");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/chat");
    EXPECT_TRUE(url.is_ws);
}

TEST(ToolboxTest, ParseInvalidUrl)
{
    auto url = parseUrl("not-a-url");
    EXPECT_TRUE(url.host.empty());
}

TEST(ToolboxTest, ParseWssUrlCustomPort)
{
    auto url = parseUrl("wss://secure.com:9443/chat");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 9443);
    EXPECT_EQ(url.path, "/chat");
    EXPECT_TRUE(url.is_ws);
}

TEST(ToolboxTest, ParseWssUrlDefaultPath)
{
    auto url = parseUrl("wss://secure.com");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/");
    EXPECT_TRUE(url.is_ws);
}

TEST(ToolboxTest, ParseWsUrlCustomPort)
{
    auto url = parseUrl("ws://chat.example.com:1234/room");
    EXPECT_EQ(url.host, "chat.example.com");
    EXPECT_EQ(url.port, 1234);
    EXPECT_EQ(url.path, "/room");
    EXPECT_TRUE(url.is_ws);
}

TEST(ToolboxTest, JsonError)
{
    std::string e = jsonError("test error");
    EXPECT_NE(e.find("error"), std::string::npos);
    EXPECT_NE(e.find("test error"), std::string::npos);
}

TEST(ToolboxTest, JsonOk)
{
    EXPECT_EQ(jsonOk(), "{\"type\":\"ok\"}");
}

// ====== JSON parse ======

TEST(ToolboxTest, JsonParseStr)
{
    std::string msg = R"({"action":"new_game","game":"snake","width":20})";
    EXPECT_EQ(jsonParseStr(msg, "action"), "new_game");
    EXPECT_EQ(jsonParseStr(msg, "game"), "snake");
    EXPECT_EQ(jsonParseStr(msg, "width"), "20");
    EXPECT_TRUE(jsonParseStr(msg, "missing").empty());
}

TEST(ToolboxTest, JsonParseInt)
{
    std::string msg = R"({"width":20,"height":15})";
    EXPECT_EQ(jsonParseInt(msg, "width"), 20);
    EXPECT_EQ(jsonParseInt(msg, "height"), 15);
    EXPECT_EQ(jsonParseInt(msg, "missing"), 0);
}

TEST(ToolboxTest, JsonParseStrWithEscape)
{
    std::string msg = R"({"msg":"hello\"world"})";
    EXPECT_EQ(jsonParseStr(msg, "msg"), "hello\"world");
}

// ====== HttpClient ======

TEST(HttpClientTest, GetAsyncInvalidUrl)
{
    ThreadPool pool(2);
    HttpClient client(pool);

    std::atomic<bool> got_error{false};
    std::string error_msg;

    client.getAsync("not-a-url", {
        []() {},
        [](const std::string&, bool) {},
        [&](const std::string& msg) {
            error_msg = msg;
            got_error.store(true);
        }
    });

    for (int i = 0; i < 50; ++i)
    {
        if (got_error.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(got_error.load());
    EXPECT_NE(error_msg.find("invalid URL"), std::string::npos);
}

TEST(HttpClientTest, PostAsyncInvalidUrl)
{
    ThreadPool pool(2);
    HttpClient client(pool);

    std::atomic<bool> got_error{false};

    client.postAsync("not-a-url", "body", "text/plain", {
        []() {},
        [](const std::string&, bool) {},
        [&](const std::string&) { got_error.store(true); }
    });

    for (int i = 0; i < 50; ++i)
    {
        if (got_error.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(got_error.load());
}

TEST(HttpClientTest, InitialState)
{
    ThreadPool pool(2);
    HttpClient client(pool);
    EXPECT_EQ(client.state(), ConnState::CLOSED);
}

TEST(HttpClientTest, SetTimeout)
{
    ThreadPool pool(2);
    HttpClient client(pool);
    client.withTimeout(std::chrono::milliseconds(1000));
    EXPECT_EQ(client.state(), ConnState::CLOSED);
}

TEST(HttpClientTest, ConstructWithLogger)
{
    ThreadPool pool(2);
    Logger logger(Logger::DEBUG);
    HttpClient client(pool);
    client.withLogger(&logger);
    EXPECT_EQ(client.state(), ConnState::CLOSED);
}
