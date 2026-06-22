#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <boost/noncopyable.hpp>

class Logger : private boost::noncopyable
{
public:
    enum Level { DEBUG = 0, INFO, WARN, ERROR };

    explicit Logger(Level level = INFO);
    explicit Logger(const std::string& filepath, Level level = INFO);
    explicit Logger(std::ostream& os, Level level = INFO);

    void setLevel(Level level);
    Level getLevel() const;

    class LogStream
    {
    public:
        LogStream(std::ostream* os, std::mutex& mtx, const std::string& prefix, bool active)
            : m_os(active ? os : nullptr), m_mtx(active ? &mtx : nullptr)
        {
            if (active)
                m_ss << prefix;
        }

        ~LogStream()
        {
            if (m_os)
            {
                m_ss << '\n';
                std::lock_guard<std::mutex> lock(*m_mtx);
                *m_os << m_ss.str();
            }
        }

        LogStream(LogStream&& other) noexcept
            : m_os(other.m_os), m_mtx(other.m_mtx)
            , m_ss(std::move(other.m_ss))
        {
            other.m_os = nullptr;
            other.m_mtx = nullptr;
        }

        template<typename T>
        LogStream& operator<<(const T& val)
        {
            if (m_os) m_ss << val;
            return *this;
        }

        LogStream& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            if (m_os) m_ss << manip;
            return *this;
        }

        LogStream(const LogStream&) = delete;
        LogStream& operator=(const LogStream&) = delete;

    private:
        std::ostream* m_os = nullptr;
        std::mutex* m_mtx = nullptr;
        std::ostringstream m_ss;
    };

    LogStream log(Level level);
    LogStream debug() { return log(DEBUG); }
    LogStream info()  { return log(INFO); }
    LogStream warn()  { return log(WARN); }
    LogStream error() { return log(ERROR); }

private:
    static const std::string& timestamp();
    static const char* levelName(Level level);
    static std::string formatPrefix(Level level);

    std::ostream* m_os;
    std::atomic<Level> m_level;
    mutable std::mutex m_mtx;
    std::unique_ptr<std::ofstream> m_file_stream;
};
