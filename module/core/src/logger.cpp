#include "logger.hpp"
#include <ctime>

Logger::Logger(Level level)
    : m_os(&std::cerr), m_level(level) {}

Logger::Logger(const std::string& filepath, Level level)
    : m_level(level)
{
    auto fs = std::make_unique<std::ofstream>(filepath);
    if (!fs->is_open())
        m_os = &std::cerr;
    else
        m_os = fs.get();
    m_file_stream = std::move(fs);
}

Logger::Logger(std::ostream& os, Level level)
    : m_os(&os), m_level(level) {}

void Logger::setLevel(Level level) { m_level.store(level, std::memory_order_relaxed); }

Logger::Level Logger::getLevel() const { return m_level.load(std::memory_order_relaxed); }

Logger::LogStream Logger::log(Level level)
{
    if (level < m_level.load(std::memory_order_relaxed))
        return LogStream(m_os, m_mtx, "", false);

    return LogStream(m_os, m_mtx, formatPrefix(level), true);
}

std::string Logger::formatPrefix(Level level)
{
    thread_local char buf[128];
    int n = std::snprintf(buf, sizeof(buf), "[%s] [%s] ", timestamp().c_str(), levelName(level));
    return std::string(buf, static_cast<size_t>(n));
}

const char* Logger::levelName(Level level)
{
    static const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    return (level >= 0 && level <= ERROR) ? names[level] : "?";
}

const std::string& Logger::timestamp()
{
    thread_local std::time_t last_t = 0;
    thread_local std::string cached;
    std::time_t t = std::time(nullptr);
    if (t != last_t)
    {
        char buf[24];
        std::tm tm_buf;
        ::localtime_r(&t, &tm_buf);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        cached = buf;
        last_t = t;
    }
    return cached;
}
