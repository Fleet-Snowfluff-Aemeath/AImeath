#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include "vfs.hpp"

class VfsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_tmp = fs::temp_directory_path() / "aimeath_vfs_test";
        fs::create_directories(m_tmp);
        fs::create_directories(m_tmp / "subdir");

        std::ofstream(m_tmp / "hello.txt") << "hello world";
        std::ofstream(m_tmp / "subdir" / "nested.txt") << "nested file content";

        m_vfs.reset(new VirtualFileSystem(m_tmp.string()));
    }

    void TearDown() override
    {
        m_vfs.reset();
        std::error_code ec;
        fs::remove_all(m_tmp, ec);
    }

    fs::path m_tmp;
    std::unique_ptr<VirtualFileSystem> m_vfs;
};

TEST_F(VfsTest, RootAccessible)
{
    EXPECT_FALSE(m_vfs->root().empty());
    EXPECT_NE(m_vfs->root().find("aimeath_vfs_test"), std::string::npos);
}

TEST_F(VfsTest, ListDirReturnsEntries)
{
    auto entries = m_vfs->listDir("/");
    ASSERT_GE(entries.size(), 2u);

    bool hasHello = false, hasSubdir = false;
    for (auto& e : entries)
    {
        if (e.name == "hello.txt") { hasHello = true; EXPECT_EQ(e.kind, "file"); }
        if (e.name == "subdir")    { hasSubdir = true; EXPECT_EQ(e.kind, "dir"); }
    }
    EXPECT_TRUE(hasHello);
    EXPECT_TRUE(hasSubdir);
}

TEST_F(VfsTest, ListDirSortsDirsFirst)
{
    auto entries = m_vfs->listDir("/");
    bool foundFile = false;
    for (auto& e : entries)
    {
        if (e.kind == "file") foundFile = true;
        if (foundFile) EXPECT_EQ(e.kind, "file");
    }
}

TEST_F(VfsTest, ListDirSubdirectory)
{
    auto entries = m_vfs->listDir("/subdir");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].name, "nested.txt");
    EXPECT_EQ(entries[0].kind, "file");
}

TEST_F(VfsTest, ListDirNonExistentReturnsEmpty)
{
    auto entries = m_vfs->listDir("/nonexistent");
    EXPECT_TRUE(entries.empty());
}

TEST_F(VfsTest, ReadFileContent)
{
    std::string content = m_vfs->readFile("/hello.txt");
    EXPECT_EQ(content, "hello world");
}

TEST_F(VfsTest, ReadFileSubdirectory)
{
    std::string content = m_vfs->readFile("/subdir/nested.txt");
    EXPECT_EQ(content, "nested file content");
}

TEST_F(VfsTest, ReadFileNonExistentReturnsEmpty)
{
    std::string content = m_vfs->readFile("/no_such_file.txt");
    EXPECT_TRUE(content.empty());
}

TEST_F(VfsTest, Exists)
{
    EXPECT_TRUE(m_vfs->exists("/hello.txt"));
    EXPECT_TRUE(m_vfs->exists("/subdir"));
    EXPECT_FALSE(m_vfs->exists("/nonexistent"));
}

TEST_F(VfsTest, MkdirCreatesDirectory)
{
    EXPECT_TRUE(m_vfs->mkdir("/newdir"));
    EXPECT_TRUE(fs::exists(m_tmp / "newdir"));
}

TEST_F(VfsTest, MkdirRecursive)
{
    EXPECT_TRUE(m_vfs->mkdir("/a/b/c"));
    EXPECT_TRUE(fs::exists(m_tmp / "a" / "b" / "c"));
}

TEST_F(VfsTest, WriteFileCreatesContent)
{
    m_vfs->writeFile("/written.txt", "test content");
    std::string content = m_vfs->readFile("/written.txt");
    EXPECT_EQ(content, "test content");
}

TEST_F(VfsTest, WriteFileCreatesParentDirs)
{
    m_vfs->writeFile("/deep/nested/file.txt", "deep content");
    EXPECT_TRUE(fs::exists(m_tmp / "deep" / "nested" / "file.txt"));
}

TEST_F(VfsTest, RemoveFile)
{
    EXPECT_TRUE(m_vfs->exists("/hello.txt"));
    EXPECT_TRUE(m_vfs->remove("/hello.txt"));
    EXPECT_FALSE(m_vfs->exists("/hello.txt"));
}

TEST_F(VfsTest, RemoveNonExistentReturnsFalse)
{
    EXPECT_FALSE(m_vfs->remove("/no_such_file.txt"));
}

TEST_F(VfsTest, PathTraversalIsBlocked)
{
    auto entries = m_vfs->listDir("/../outside");
    EXPECT_TRUE(entries.empty());
}

TEST_F(VfsTest, FileUrlStartsWithHomePrefix)
{
    auto entries = m_vfs->listDir("/");
    ASSERT_GE(entries.size(), 1u);
    for (auto& e : entries)
    {
        if (e.name == "hello.txt")
        {
            EXPECT_EQ(e.url, "/home/hello.txt");
        }
    }
}

TEST_F(VfsTest, SubdirFileUrlHasHomePrefix)
{
    auto entries = m_vfs->listDir("/home/subdir");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].url, "/home/subdir/nested.txt");
}

TEST_F(VfsTest, RootFileUrlExists)
{
    auto entries = m_vfs->listDir("/");
    ASSERT_GE(entries.size(), 1u);
    for (auto& e : entries)
    {
        EXPECT_TRUE(e.url.rfind("/home", 0) == 0);
        EXPECT_FALSE(e.url.empty());
    }
}

TEST_F(VfsTest, ListDirSubpathWithoutHomePrefixWorks)
{
    auto entries = m_vfs->listDir("/home/subdir");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].name, "nested.txt");
}

TEST_F(VfsTest, ListDirWithTrailingSlash)
{
    auto entries = m_vfs->listDir("/home/subdir/");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].name, "nested.txt");
}

TEST_F(VfsTest, ListDirDeeplyNested)
{
    m_vfs->mkdir("/home/a/b/c");
    std::ofstream(m_tmp / "a" / "b" / "c" / "deep.txt") << "deep";
    auto entries = m_vfs->listDir("/home/a/b/c");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].name, "deep.txt");
}
