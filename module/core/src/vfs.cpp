#include "vfs.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <sys/stat.h>

VirtualFileSystem::VirtualFileSystem(const std::string& root_path)
    : m_root(fs::absolute(fs::path(root_path)))
{
}

std::string VirtualFileSystem::root() const
{
    return m_root.string();
}

fs::path VirtualFileSystem::resolvePath(const std::string& path) const
{
    std::string rel;
    if (path == "/" || path == "/home" || path == "/home/")
        rel = "";
    else if (path.rfind("/home/", 0) == 0)
        rel = path.substr(6); // 去掉 "/home/" 前缀，保留剩余路径
    else if (!path.empty() && path[0] == '/')
        rel = path.substr(1);
    else
        rel = path;

    fs::path full = m_root / rel;
    std::error_code ec;
    full = fs::weakly_canonical(full, ec);
    if (ec) return {};

    fs::path root_canon = fs::weakly_canonical(m_root, ec);
    if (ec) return {};

    std::string s_full = full.string();
    std::string s_root = root_canon.string();

    if (s_full.size() < s_root.size() ||
        s_full.compare(0, s_root.size(), s_root) != 0 ||
        (s_full.size() > s_root.size() && s_full[s_root.size()] != '/'))
        return {};

    return full;
}

std::string VirtualFileSystem::fileUrl(const fs::path& full) const
{
    std::error_code ec;
    if (fs::equivalent(full, m_root, ec))
        return "/home";
    auto rel = fs::relative(full, m_root, ec);
    if (ec) return "";
    return "/home/" + rel.generic_string();
}

std::string VirtualFileSystem::modifiedTime(const fs::path& p)
{
    struct stat st;
    if (::stat(p.string().c_str(), &st) != 0) return "";
    char buf[32];
    std::tm tm;
    localtime_r(&st.st_mtime, &tm);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

std::vector<FileEntry> VirtualFileSystem::listDir(const std::string& path) const
{
    fs::path dir = resolvePath(path);
    if (dir.empty()) return {};

    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return {};

    std::vector<FileEntry> entries;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        FileEntry fe;
        fe.name = e.path().filename().string();
        fe.kind = e.is_directory(ec) ? "dir" : "file";
        if (ec) fe.kind = "file";
        fe.size = fe.kind == "dir" ? 0 : e.file_size(ec);
        if (ec) fe.size = 0;
        ec.clear();
        fe.modified = modifiedTime(e.path());
        fe.url = fileUrl(e.path());
        entries.push_back(std::move(fe));
    }

    std::sort(entries.begin(), entries.end(),
        [](const FileEntry& a, const FileEntry& b) {
            if (a.kind != b.kind) return a.kind == "dir";
            return a.name < b.name;
        });

    return entries;
}

std::string VirtualFileSystem::readFile(const std::string& path) const
{
    fs::path file = resolvePath(path);
    if (file.empty()) return "";

    try {
        std::ifstream in(file, std::ios::binary);
        if (!in) return "";
        std::ostringstream oss;
        oss << in.rdbuf();
        return oss.str();
    } catch (...) {
        return "";
    }
}

void VirtualFileSystem::writeFile(const std::string& path, const std::string& content)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    fs::path file = resolvePath(path);
    if (file.empty()) return;

    try {
        if (file.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(file.parent_path(), ec);
        }
        std::ofstream out(file, std::ios::binary);
        if (out)
            out.write(content.data(), content.size());
    } catch (...) {}
}

bool VirtualFileSystem::mkdir(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    fs::path dir = resolvePath(path);
    if (dir.empty()) return false;

    std::error_code ec;
    return fs::create_directories(dir, ec);
}

bool VirtualFileSystem::remove(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    fs::path target = resolvePath(path);
    if (target.empty()) return false;

    std::error_code ec;
    return fs::remove(target, ec);
}

bool VirtualFileSystem::exists(const std::string& path) const
{
    fs::path p = resolvePath(path);
    if (p.empty()) return false;
    std::error_code ec;
    return fs::exists(p, ec);
}
