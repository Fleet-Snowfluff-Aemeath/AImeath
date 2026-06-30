#include <gtest/gtest.h>
#include "ws_server.hpp"
#include <boost/asio.hpp>

TEST(WsServerConstantsTest, DefaultPort)
{
    EXPECT_EQ(DEFAULT_PORT, 3001);
}

TEST(WsServerConstantsTest, DefaultIoThreads)
{
    EXPECT_EQ(DEFAULT_IO_THREADS, 4);
}

TEST(WsServerConstantsTest, DefaultFallbackThreads)
{
    EXPECT_EQ(DEFAULT_FALLBACK_THREADS, 4);
}

TEST(WsServerKeyNamespaceTest, AppKey)
{
    EXPECT_STREQ(key::APP, "app");
}

TEST(WsServerKeyNamespaceTest, TextKey)
{
    EXPECT_STREQ(key::TEXT, "text");
}

TEST(WsServerKeyNamespaceTest, GameKey)
{
    EXPECT_STREQ(key::GAME, "game");
}

TEST(WsServerAppnameNamespaceTest, ChatApp)
{
    EXPECT_STREQ(appname::CHAT, "chat");
}

TEST(WsServerAppnameNamespaceTest, SnakeApp)
{
    EXPECT_STREQ(appname::SNAKE, "snake");
}

TEST(WsServerListenerTest, ConstructAndShutdown)
{
    asio::io_context io;
    Logger logger(Logger::WARN);
    AppModuleCache cache;

    auto listener = std::make_shared<Listener>(io, logger, cache, nullptr, 0);
    EXPECT_NO_THROW(listener->shutdown());
}

TEST(WsServerListenerTest, CustomPort)
{
    asio::io_context io;
    Logger logger(Logger::WARN);
    AppModuleCache cache;

    auto listener = std::make_shared<Listener>(io, logger, cache, nullptr, 8080);
    EXPECT_NO_THROW(listener->shutdown());
}

TEST(WsServerSessionTest, ConstructSession)
{
    asio::io_context io;
    Logger logger(Logger::WARN);
    AppModuleCache cache;
    ThreadPool fallback(1);

    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    tcp::socket socket1(io);
    tcp::socket socket2(io);

    acceptor.async_accept(socket2, [](boost::system::error_code) {});

    boost::system::error_code ec;
    socket1.connect(acceptor.local_endpoint(), ec);
    ASSERT_FALSE(ec) << ec.message();

    socket2.close();

    auto session = std::make_shared<Session>(
        std::move(socket1), logger, cache, &fallback, &io, DEFAULT_PORT);
    EXPECT_NO_THROW(session->start());
    fallback.shutdown();
}

TEST(WsServerSessionTest, ConstructSessionWithNoFallbackPool)
{
    asio::io_context io;
    Logger logger(Logger::WARN);
    AppModuleCache cache;

    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    tcp::socket socket1(io);
    tcp::socket socket2(io);

    acceptor.async_accept(socket2, [](boost::system::error_code) {});

    boost::system::error_code ec;
    socket1.connect(acceptor.local_endpoint(), ec);
    ASSERT_FALSE(ec) << ec.message();

    socket2.close();

    auto session = std::make_shared<Session>(
        std::move(socket1), logger, cache, nullptr, &io, DEFAULT_PORT);
    EXPECT_NO_THROW(session->start());
}
