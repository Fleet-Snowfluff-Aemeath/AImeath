#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "pty_session.hpp"

class PtySessionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_io = std::make_unique<boost::asio::io_context>();
        m_work = std::make_unique<boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type>>(m_io->get_executor());
        m_thread = std::make_unique<std::thread>([this] { m_io->run(); });
        m_session = std::make_unique<PtySession>(*m_io);
    }

    void TearDown() override
    {
        m_session.reset();
        m_work.reset();
        m_io->stop();
        if (m_thread && m_thread->joinable())
            m_thread->join();
    }

    std::unique_ptr<boost::asio::io_context> m_io;
    std::unique_ptr<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> m_work;
    std::unique_ptr<std::thread> m_thread;
    std::unique_ptr<PtySession> m_session;
};

TEST_F(PtySessionTest, StartSimpleCommand)
{
    EXPECT_TRUE(m_session->start("echo hello"));
    EXPECT_TRUE(m_session->isAlive());
}

TEST_F(PtySessionTest, StartAsyncSimpleCommand)
{
    std::string output;
    m_session->setOutput(
        [](void* udata, const char* text) {
            auto* s = static_cast<std::string*>(udata);
            *s += text;
        },
        &output);

    m_session->startAsync("echo test_output");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // 异步读取应该在子进程退出前捕获到输出
    // 注意: 子进程可能很快退出，输出未必完整捕获
    EXPECT_FALSE(output.empty());
}

TEST_F(PtySessionTest, WriteAndCheckAlive)
{
    EXPECT_TRUE(m_session->start("cat"));
    EXPECT_TRUE(m_session->isAlive());
    m_session->write("test input\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // cat 仍在运行
}

TEST_F(PtySessionTest, ResizeDoesNotCrash)
{
    EXPECT_TRUE(m_session->start("sleep 1"));
    m_session->resize(30, 100);
    EXPECT_TRUE(m_session->isAlive());
}

TEST_F(PtySessionTest, CloseTerminatesChild)
{
    EXPECT_TRUE(m_session->start("sleep 10"));
    EXPECT_TRUE(m_session->isAlive());
    m_session->close();
    EXPECT_FALSE(m_session->isAlive());
}

TEST_F(PtySessionTest, IsAliveAfterExit)
{
    EXPECT_TRUE(m_session->start("true"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(m_session->isAlive());
}

TEST_F(PtySessionTest, WriteToClosedSessionDoesNotCrash)
{
    m_session->close();
    m_session->write("data");
    m_session->resize(24, 80);
    // 不崩溃即为通过
}
