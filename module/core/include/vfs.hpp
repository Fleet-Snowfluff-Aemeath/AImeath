#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <mutex>

#include <boost/noncopyable.hpp>

namespace fs = std::filesystem;

struct FileEntry
{
    std::string name;
    std::string kind;
    uint64_t    size;
    std::string modified;
    std::string url;
};

class VirtualFileSystem : private boost::noncopyable
{
public:
    explicit VirtualFileSystem(const std::string& root_path);

    std::string root() const;

    std::vector<FileEntry> listDir(const std::string& path) const;
    std::string readFile(const std::string& path) const;
    void writeFile(const std::string& path, const std::string& content);
    bool mkdir(const std::string& path);
    bool remove(const std::string& path);
    bool exists(const std::string& path) const;

private:
    fs::path resolvePath(const std::string& path) const;
    std::string fileUrl(const fs::path& full) const;
    static std::string modifiedTime(const fs::path& p);

    fs::path m_root;
    mutable std::mutex m_mtx;
};
