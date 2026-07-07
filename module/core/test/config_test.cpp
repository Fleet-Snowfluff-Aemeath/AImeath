#include <gtest/gtest.h>
#include "config.hpp"
#include "ws_server.hpp"
#include "app_mod.hpp"
#include <boost/asio.hpp>
#include <boost/json.hpp>

// ====== Config ======

TEST(ConfigTest, GetIntUsesDefault)
{
    auto& cfg = Config::instance();
    EXPECT_EQ(cfg.getInt("__nonexistent_key__", 42), 42);
    EXPECT_EQ(cfg.getInt("__nonexistent_key__", -1), -1);
}

TEST(ConfigTest, GetStringUsesDefault)
{
    auto& cfg = Config::instance();
    EXPECT_EQ(cfg.getString("__nonexistent_key__", "fallback"), "fallback");
}

// ====== SessionManager (limited â€?full Session needs WS upgrade) ======

class SessionManagerTest : public ::testing::Test
{
protected:
    asio::io_context io;
    Logger logger = Logger(Logger::WARN);
    AppModuleCache cache;
    ThreadPool fallback{1};
    SessionManager& reg = SessionManager::instance();

    std::shared_ptr<Session> createSession(const std::string& wid = "")
    {
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
        tcp::socket socket1(io);
        tcp::socket socket2(io);
        acceptor.async_accept(socket2, [](boost::system::error_code) {});
        boost::system::error_code ec;
        socket1.connect(acceptor.local_endpoint(), ec);
        socket2.close();
        auto sess = std::make_shared<Session>(
            std::move(socket1), logger, cache, &fallback, &io, DEFAULT_PORT);
        if (!wid.empty()) sess->set_window_id(wid);
        return sess;
    }

    void TearDown() override {}
};

TEST_F(SessionManagerTest, FindNonExistentReturnsNull)
{
    EXPECT_EQ(reg.findSession("__nonexistent__", 0), nullptr);
}

TEST_F(SessionManagerTest, FindAllSessionsNonExistent)
{
    EXPECT_TRUE(reg.findAllSessions("__nonexistent__").empty());
}

TEST_F(SessionManagerTest, RegisterAndUnregisterSession)
{
    auto sess = createSession("win_a");
    std::string sid = sess->session_id();
    reg.registerSession("test_app", sess);
    // unregister and verify no crash â€?actual find requires is_open() which needs WS upgrade
    reg.unregisterSession("test_app", sess.get());
    reg.unregisterWindow("win_a");
}

TEST_F(SessionManagerTest, RegisterAndUnregisterWindow)
{
    auto sess = createSession("win_b");
    reg.registerWindow("win_b", sess->session_id(), "test_app");
    reg.unregisterWindow("win_b");
}

TEST_F(SessionManagerTest, ListActiveWindowsOnEmpty)
{
    auto windows = reg.listActiveWindows();
    EXPECT_TRUE(windows.empty());
}

TEST_F(SessionManagerTest, MultipleUnregisterDoesNotCrash)
{
    auto sess = createSession();
    reg.registerSession("multi", sess);
    reg.unregisterSession("multi", sess.get());
    reg.unregisterSession("multi", sess.get());
}

TEST_F(SessionManagerTest, ListSessionsInitiallyEmpty)
{
    EXPECT_TRUE(reg.listSessions().empty());
}

TEST(ConfigTest, IoThreadsUsesDefaultWhenNotInJson)
{
    auto& cfg = Config::instance();
    int val = cfg.ioThreads();
    EXPECT_GE(val, 4);
}

TEST(ConfigTest, FallbackThreadsUsesDefaultWhenNotInJson)
{
    auto& cfg = Config::instance();
    int val = cfg.fallbackThreads();
    EXPECT_GE(val, 4);
}

TEST(ConfigTest, MaxConnectionsDefaultsToZero)
{
    auto& cfg = Config::instance();
    EXPECT_EQ(cfg.maxConnections(), 0);
}

TEST(ConfigTest, PingIntervalDefaultsTo30)
{
    auto& cfg = Config::instance();
    EXPECT_EQ(cfg.pingIntervalSec(), 30);
}

TEST(ConfigTest, DefaultIoThreadsAtLeast4)
{
    EXPECT_GE(Config::defaultIoThreads(), 4);
}
