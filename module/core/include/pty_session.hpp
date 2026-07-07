#pragma once

#include <string>
#include <memory>
#include <array>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>

#include <functional>

class PtySession : private boost::noncopyable
{
public:
    explicit PtySession(boost::asio::io_context& io);
    ~PtySession();

    bool start(const std::string& cmd);
    void startAsync(const std::string& cmd);
    void write(const std::string& data);
    void resize(int rows, int cols);
    bool isAlive();
    void close();

    void setOutput(std::function<void(const std::string&)> cb);

private:
    void startReadLoop();
    void pushOutput(const std::string& text);

    int m_master_fd = -1;
    pid_t m_child_pid = -1;
    std::unique_ptr<boost::asio::posix::stream_descriptor> m_pty_stream;
    std::function<void(const std::string&)> m_output_cb;
    std::array<char, 4096> m_read_buf{};

    boost::asio::io_context& m_io;
};
