#include "pty_session.hpp"

#include <pty.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>

PtySession::PtySession(boost::asio::io_context& io)
    : m_io(io)
{
}

PtySession::~PtySession()
{
    close();
}

bool PtySession::start(const std::string& cmd)
{
    close();

    struct winsize ws;
    ws.ws_row = 24;
    ws.ws_col = 80;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    pid_t pid = forkpty(&m_master_fd, nullptr, nullptr, &ws);
    if (pid == 0)
    {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        if (!cmd.empty())
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        else
            execl("/bin/sh", "sh", "--norc", nullptr);
        _exit(127);
    }
    if (pid < 0) return false;

    m_child_pid = pid;

    int flags = fcntl(m_master_fd, F_GETFL, 0);
    fcntl(m_master_fd, F_SETFL, flags | O_NONBLOCK);

    return true;
}

void PtySession::startAsync(const std::string& cmd)
{
    if (!start(cmd)) return;
    m_pty_stream = std::make_unique<boost::asio::posix::stream_descriptor>(
        m_io, m_master_fd);
    startReadLoop();
}

void PtySession::startReadLoop()
{
    if (!m_pty_stream) return;
    auto self = this;
    m_pty_stream->async_read_some(
        boost::asio::buffer(m_read_buf),
        [self](boost::system::error_code ec, std::size_t n) {
            if (ec || n == 0) return;
            self->pushOutput(std::string(self->m_read_buf.data(), n));
            self->startReadLoop();
        });
}

void PtySession::pushOutput(const std::string& text)
{
    if (!m_output_cb || text.empty()) return;
    m_output_cb(m_output_udata, text.c_str());
}

void PtySession::write(const std::string& data)
{
    if (m_master_fd >= 0 && !data.empty())
        (void)::write(m_master_fd, data.data(), data.size());
}

void PtySession::resize(int rows, int cols)
{
    if (m_master_fd < 0) return;
    struct winsize ws;
    ioctl(m_master_fd, TIOCGWINSZ, &ws);
    ws.ws_row = rows;
    ws.ws_col = cols;
    ioctl(m_master_fd, TIOCSWINSZ, &ws);
}

bool PtySession::isAlive()
{
    if (m_child_pid <= 0) return false;
    int status;
    pid_t r = waitpid(m_child_pid, &status, WNOHANG);
    if (r == m_child_pid)
    {
        m_child_pid = -1;
        return false;
    }
    return r == 0;
}

void PtySession::close()
{
    if (m_pty_stream)
    {
        m_pty_stream->cancel();
        m_pty_stream->release();
        m_pty_stream.reset();
    }
    if (m_child_pid > 0)
    {
        kill(m_child_pid, SIGTERM);
        usleep(50000);
        kill(m_child_pid, SIGKILL);
        waitpid(m_child_pid, nullptr, WNOHANG);
        m_child_pid = -1;
    }
    if (m_master_fd >= 0)
    {
        ::close(m_master_fd);
        m_master_fd = -1;
    }
}

void PtySession::setOutput(app_output_fn cb, void* udata)
{
    m_output_cb = cb;
    m_output_udata = udata;
}
