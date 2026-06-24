#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <atomic>

#include <pty.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstring>

#include <boost/asio.hpp>
#include <boost/json.hpp>

struct TermSession
{
    int master_fd = -1;
    pid_t child_pid = -1;

    // ---- Async I/O ----
    std::unique_ptr<boost::asio::posix::stream_descriptor> pty_stream;
    app_output_fn output_cb = nullptr;
    void* output_udata = nullptr;
    std::array<char, 4096> read_buf{};
    std::string pending_output;   // accumulated for flush

    // ---- Send one output message via callback ----
    void push_output(const std::string& text)
    {
        if (!output_cb || text.empty()) return;
        boost::json::object msg;
        msg["type"] = "output";
        msg["text"] = text;
        output_cb(output_udata, boost::json::serialize(msg).c_str());
    }

    // ---- Get global PTY I/O context (singleton) ----
    static boost::asio::io_context& pty_io()
    {
        static boost::asio::io_context io;
        static auto work = boost::asio::make_work_guard(io);
        static std::thread t([] { io.run(); });
        return io;
    }

    // ---- Start async read loop ----
    void start_async_read()
    {
        if (!pty_stream) return;
        auto self = this;
        pty_stream->async_read_some(
            boost::asio::buffer(read_buf),
            [self](boost::system::error_code ec, std::size_t n) {
                if (ec || n == 0) return;
                std::string piece(self->read_buf.data(), n);
                self->pending_output += piece;
                self->push_output(std::move(piece));
                self->start_async_read();
            });
    }

    // ---- Flush accumulated output (for stdout polls) ----
    std::string flushOutput()
    {
        std::string out;
        std::swap(out, pending_output);
        return out;
    }

    // ---- Synchronous helpers (shared by sync + async paths) ----

    bool start(const std::string& cmd)
    {
        close();

        struct winsize ws;
        ws.ws_row = 24;
        ws.ws_col = 80;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;

        pid_t pid = forkpty(&master_fd, nullptr, nullptr, &ws);
        if (pid == 0) {
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

        child_pid = pid;

        int flags = fcntl(master_fd, F_GETFL, 0);
        fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

        return true;
    }

    void start_with_async(const std::string& cmd)
    {
        if (!start(cmd)) return;
        pty_stream = std::make_unique<boost::asio::posix::stream_descriptor>(
            pty_io(), master_fd);
        start_async_read();
    }

    std::string readOutput()
    {
        std::string result;
        char buf[4096];
        usleep(30000);
        for (int i = 0; i < 5; i++) {
            bool got = false;
            while (true) {
                ssize_t n = read(master_fd, buf, sizeof(buf));
                if (n > 0) { result.append(buf, n); got = true; }
                else break;
            }
            if (!got) {
                if (!result.empty()) break;
                usleep(40000);
            } else {
                usleep(20000);
            }
        }
        return result;
    }

    std::string readPending()
    {
        std::string result;
        char buf[4096];
        while (true) {
            ssize_t n = read(master_fd, buf, sizeof(buf));
            if (n > 0) result.append(buf, n);
            else break;
        }
        return result;
    }

    void writeInput(const std::string& data)
    {
        if (master_fd >= 0 && !data.empty())
            write(master_fd, data.data(), data.size());
    }

    void setSize(int rows, int cols)
    {
        if (master_fd < 0) return;
        struct winsize ws;
        ioctl(master_fd, TIOCGWINSZ, &ws);
        ws.ws_row = rows;
        ws.ws_col = cols;
        ioctl(master_fd, TIOCSWINSZ, &ws);
    }

    bool isAlive()
    {
        if (child_pid <= 0) return false;
        int status;
        pid_t r = waitpid(child_pid, &status, WNOHANG);
        if (r == child_pid) {
            child_pid = -1;
            return false;
        }
        return r == 0;
    }

    void close()
    {
        if (pty_stream) {
            pty_stream->cancel();
            pty_stream->release();
            pty_stream.reset();
        }
        if (child_pid > 0) {
            kill(child_pid, SIGTERM);
            usleep(50000);
            kill(child_pid, SIGKILL);
            waitpid(child_pid, nullptr, WNOHANG);
            child_pid = -1;
        }
        if (master_fd >= 0) {
            ::close(master_fd);
            master_fd = -1;
        }
    }

    ~TermSession() { close(); }
};
