#include <gtest/gtest.h>
#include "netconn.hpp"
#include "wsutil.hpp"
#include "threadmgr.hpp"
#include "logger.hpp"

// ====== URL Parsing ======

TEST(WsUtilTest, ParseHttpUrl)
{
    auto url = parseUrl("http://example.com/path");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 80);
    EXPECT_EQ(url.path, "/path");
    EXPECT_FALSE(url.is_ws);
}

TEST(WsUtilTest, ParseHttpUrlDefaultPath)
{
    auto url = parseUrl("http://example.com");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 80);
    EXPECT_EQ(url.path, "/");
}

TEST(WsUtilTest, ParseHttpUrlCustomPort)
{
    auto url = parseUrl("http://example.com:8080/api");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 8080);
    EXPECT_EQ(url.path, "/api");
}

TEST(WsUtilTest, ParseWsUrl)
{
    auto url = parseUrl("ws://chat.example.com/room");
    EXPECT_EQ(url.host, "chat.example.com");
    EXPECT_EQ(url.port, 80);
    EXPECT_EQ(url.path, "/room");
    EXPECT_TRUE(url.is_ws);
}

TEST(WsUtilTest, ParseHttpsUrl)
{
    auto url = parseUrl("https://secure.com/api/v1");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/api/v1");
}

TEST(WsUtilTest, ParseWssUrl)
{
    auto url = parseUrl("wss://secure.com/chat");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/chat");
    EXPECT_TRUE(url.is_ws);
}

TEST(WsUtilTest, ParseInvalidUrl)
{
    auto url = parseUrl("not-a-url");
    EXPECT_TRUE(url.host.empty());
}

TEST(WsUtilTest, ParseWssUrlCustomPort)
{
    auto url = parseUrl("wss://secure.com:9443/chat");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 9443);
    EXPECT_EQ(url.path, "/chat");
    EXPECT_TRUE(url.is_ws);
}

TEST(WsUtilTest, ParseWssUrlDefaultPath)
{
    auto url = parseUrl("wss://secure.com");
    EXPECT_EQ(url.host, "secure.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/");
    EXPECT_TRUE(url.is_ws);
}

TEST(WsUtilTest, ParseWsUrlCustomPort)
{
    auto url = parseUrl("ws://chat.example.com:1234/room");
    EXPECT_EQ(url.host, "chat.example.com");
    EXPECT_EQ(url.port, 1234);
    EXPECT_EQ(url.path, "/room");
    EXPECT_TRUE(url.is_ws);
}

TEST(WsUtilTest, JsonError)
{
    std::string e = jsonError("test error");
    EXPECT_NE(e.find("error"), std::string::npos);
    EXPECT_NE(e.find("test error"), std::string::npos);
}

TEST(WsUtilTest, JsonOk)
{
    EXPECT_EQ(jsonOk(), "{\"type\":\"ok\"}");
}

// ====== JSON parse ======

TEST(WsUtilTest, JsonParseStr)
{
    std::string msg = R"({"action":"new_game","game":"snake","width":20})";
    EXPECT_EQ(jsonParseStr(msg, "action"), "new_game");
    EXPECT_EQ(jsonParseStr(msg, "game"), "snake");
    EXPECT_EQ(jsonParseStr(msg, "width"), "20");
    EXPECT_TRUE(jsonParseStr(msg, "missing").empty());
}

TEST(WsUtilTest, JsonParseInt)
{
    std::string msg = R"({"width":20,"height":15})";
    EXPECT_EQ(jsonParseInt(msg, "width"), 20);
    EXPECT_EQ(jsonParseInt(msg, "height"), 15);
    EXPECT_EQ(jsonParseInt(msg, "missing"), 0);
}

TEST(WsUtilTest, JsonParseStrWithEscape)
{
    std::string msg = R"({"msg":"hello\"world"})";
    EXPECT_EQ(jsonParseStr(msg, "msg"), "hello\"world");
}

// ====== Async HTTP (error paths) ======

TEST(NetConnTest, HttpGetAsyncInvalidUrl)
{
    ThreadPool pool(2);
    NetConn conn(pool);

    std::atomic<bool> got_error{false};
    std::string error_msg;

    conn.httpGetAsync("not-a-url",
        []() {},
        [](const std::string&, bool) {},
        [&](const std::string& msg) {
            error_msg = msg;
            got_error.store(true);
        });

    for (int i = 0; i < 50; ++i)
    {
        if (got_error.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(got_error.load());
    EXPECT_NE(error_msg.find("invalid URL"), std::string::npos);
}

TEST(NetConnTest, HttpPostAsyncInvalidUrl)
{
    ThreadPool pool(2);
    NetConn conn(pool);

    std::atomic<bool> got_error{false};

    conn.httpPostAsync("not-a-url", "body", "text/plain",
        []() {},
        [](const std::string&, bool) {},
        [&](const std::string&) {
            got_error.store(true);
        });

    for (int i = 0; i < 50; ++i)
    {
        if (got_error.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(got_error.load());
}

// ====== State Management ======

TEST(NetConnTest, InitialState)
{
    ThreadPool pool(2);
    NetConn conn(pool);
    EXPECT_EQ(conn.state(), NetConn::State::CLOSED);
}

TEST(NetConnTest, SetConnectTimeout)
{
    ThreadPool pool(2);
    NetConn conn(pool);
    conn.setConnectTimeout(std::chrono::milliseconds(1000));
    EXPECT_EQ(conn.state(), NetConn::State::CLOSED);
}

// ====== ThreadPool + Logger reuse ======

TEST(NetConnTest, ConstructWithLogger)
{
    ThreadPool pool(2);
    Logger logger(Logger::DEBUG);
    NetConn conn(pool, &logger);
    EXPECT_EQ(conn.state(), NetConn::State::CLOSED);
}
