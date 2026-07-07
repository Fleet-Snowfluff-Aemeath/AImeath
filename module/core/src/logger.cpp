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

Logger::LogStream Logger::log(Level level, const std::string& tag)
{
    if (level < m_level.load(std::memory_order_relaxed))
        return LogStream(m_os, m_mtx, "", false);
    return LogStream(m_os, m_mtx, formatPrefix(level, tag), true);
}

std::string Logger::formatPrefix(Level level)
{
    thread_local char buf[128];
    int n = std::snprintf(buf, sizeof(buf), "[%s] [%s] ",
        timestamp().c_str(), levelName(level));
    return std::string(buf, static_cast<size_t>(n));
}

std::string Logger::formatPrefix(Level level, const std::string& tag)
{
    thread_local char buf[128];
    int n = std::snprintf(buf, sizeof(buf), "[%s] [%s|%s] ",
        timestamp().c_str(), levelName(level), tag.c_str());
    return std::string(buf, static_cast<size_t>(n));
}

const char* Logger::levelName(Level level)
{
    static const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    return (level >= 0 && level <= ERROR) ? names[level] : "?";
}

std::string Logger::timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    ::localtime_r(&t, &tm_buf);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    int n = std::snprintf(buf + 19, sizeof(buf) - 19, ".%03d", static_cast<int>(ms.count()));

    return std::string(buf, 19 + static_cast<size_t>(n));
}
