#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include "logger.hpp"
#include "threadmgr.hpp"

// ====== 基本功能 ======

TEST(LoggerTest, LogOutputToStream)
{
    std::ostringstream oss;
    Logger log(oss, Logger::DEBUG);
    log.info() << "hello";
    std::string s = oss.str();
    EXPECT_NE(s.find("hello"), std::string::npos);
    EXPECT_NE(s.find("[INFO]"), std::string::npos);
}

TEST(LoggerTest, AllLevelsOutputWhenDebug)
{
    std::ostringstream oss;
    Logger log(oss, Logger::DEBUG);
    log.debug() << "d";
    log.info() << "i";
    log.warn() << "w";
    log.error() << "e";
    std::string s = oss.str();
    EXPECT_NE(s.find("[DEBUG]"), std::string::npos);
    EXPECT_NE(s.find("[INFO]"), std::string::npos);
    EXPECT_NE(s.find("[WARN]"), std::string::npos);
    EXPECT_NE(s.find("[ERROR]"), std::string::npos);
}

TEST(LoggerTest, LevelFiltering)
{
    std::ostringstream oss;
    Logger log(oss, Logger::WARN);
    log.debug() << "should be filtered";
    log.info() << "should be filtered";
    log.warn() << "visible";
    log.error() << "visible too";
    std::string s = oss.str();
    EXPECT_EQ(s.find("should be filtered"), std::string::npos);
    EXPECT_NE(s.find("visible"), std::string::npos);
    EXPECT_NE(s.find("visible too"), std::string::npos);
}

TEST(LoggerTest, DynamicLevelChange)
{
    std::ostringstream oss;
    Logger log(oss, Logger::INFO);
    log.info() << "visible";
    log.setLevel(Logger::ERROR);
    log.warn() << "filtered now";
    log.error() << "visible again";
    std::string s = oss.str();
    EXPECT_NE(s.find("visible"), std::string::npos);
    EXPECT_EQ(s.find("filtered now"), std::string::npos);
    EXPECT_NE(s.find("visible again"), std::string::npos);
}

TEST(LoggerTest, MultipleLoggers)
{
    std::ostringstream oss1, oss2;
    Logger log1(oss1);
    Logger log2(oss2);
    log1.info() << "first";
    log2.info() << "second";
    EXPECT_NE(oss1.str().find("first"), std::string::npos);
    EXPECT_NE(oss2.str().find("second"), std::string::npos);
}

// ====== 边界条件 ======

TEST(LoggerTest, EmptyMessage)
{
    std::ostringstream oss;
    Logger log(oss);
    log.info() << "";
    EXPECT_FALSE(oss.str().empty());
}

TEST(LoggerTest, VeryLongMessage)
{
    std::ostringstream oss;
    Logger log(oss);
    std::string big(10000, 'x');
    log.info() << big;
    std::string s = oss.str();
    EXPECT_NE(s.find(big), std::string::npos);
}

TEST(LoggerTest, StreamManipulator)
{
    std::ostringstream oss;
    Logger log(oss, Logger::DEBUG);
    log.info() << std::hex << 255;
    std::string s = oss.str();
    EXPECT_NE(s.find("ff"), std::string::npos);
}

TEST(LoggerTest, LevelBoundary)
{
    std::ostringstream oss;
    Logger log(oss, Logger::DEBUG);
    log.debug() << "lowest";
    log.setLevel(Logger::ERROR);
    log.error() << "highest";
    log.setLevel(Logger::DEBUG);
    log.debug() << "back to debug";
    std::string s = oss.str();
    EXPECT_NE(s.find("lowest"), std::string::npos);
    EXPECT_NE(s.find("highest"), std::string::npos);
    EXPECT_NE(s.find("back to debug"), std::string::npos);
}

TEST(LoggerTest, FileOutput)
{
    std::string path = "test_logger_file_output.log";
    {
        std::ofstream ofs(path);
        ASSERT_TRUE(ofs.is_open());
        Logger file_log(ofs);
        file_log.info() << "file line";
    }
    std::ifstream ifs(path);
    ASSERT_TRUE(ifs.is_open());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("file line"), std::string::npos);
    std::remove(path.c_str());
}

// ====== 异常场景 ======

TEST(LoggerTest, DefaultOutputToCerr)
{
    std::ostringstream oss;
    std::streambuf* old_buf = std::cerr.rdbuf(oss.rdbuf());
    {
        Logger log;
        log.info() << "cerr test";
    }
    std::cerr.rdbuf(old_buf);
    EXPECT_NE(oss.str().find("cerr test"), std::string::npos);
    EXPECT_NE(oss.str().find("[INFO]"), std::string::npos);
}

TEST(LoggerTest, FileOutputByPath)
{
    std::string path = "test_logger_path.log";
    {
        Logger file_log(path);
        file_log.info() << "file path constructor test";
    }
    std::ifstream ifs(path);
    ASSERT_TRUE(ifs.is_open());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("file path constructor test"), std::string::npos);
    std::remove(path.c_str());
}

// ====== 压力测试 ======

TEST(LoggerTest, ConcurrentLogging)
{
    std::ostringstream oss;
    Logger log(oss);
    ThreadPool pool(4);
    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
        pool.submit([&, i]() { log.info() << "concurrent msg " << i; });
    pool.wait_all();
    std::string s = oss.str();
    int lines = 0;
    size_t pos = 0;
    while ((pos = s.find('\n', pos)) != std::string::npos)
    {
        ++lines;
        ++pos;
    }
    EXPECT_EQ(lines, N);
}
