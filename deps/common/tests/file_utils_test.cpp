#include "utilities/file_utils.h"

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using common::util::exists;
using common::util::file_size;
using common::util::is_directory;
using common::util::is_regular_file;
using common::util::is_symlink;
using common::util::list_directory;
using common::util::read_file;
using common::util::read_file_bytes;
using common::util::write_file;
using common::util::write_file_atomic;
using common::util::write_file_bytes;

namespace fs = std::filesystem;

namespace {

// 测试用的临时目录：建在系统临时目录下（不依赖运行时的工作目录），
// 析构时整个删掉，不留残留。
class TempDir {
 public:
  TempDir() {
    const std::string pattern =
        (fs::temp_directory_path() / "moban_file_utils_test.XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    const char* created = ::mkdtemp(buffer.data());
    if (created == nullptr) {
      throw std::runtime_error("cannot create temporary directory");
    }
    path_ = created;
  }

  ~TempDir() {
    // 清理失败不影响测试结论，忽略即可。
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  [[nodiscard]] const std::string& path() const { return path_; }
  [[nodiscard]] std::string file(const std::string& name) const {
    return (fs::path(path_) / name).string();
  }

 private:
  std::string path_;
};

mode_t FileMode(const std::string& path) {
  struct stat st {};
  EXPECT_EQ(0, ::stat(path.c_str(), &st)) << path;
  return st.st_mode & 07777;
}

// 断言异常消息里带上了路径和失败原因。
void ExpectSystemError(const std::string& path, const std::string& action,
                        const std::string& reason, const std::function<void()>& fn) {
  try {
    fn();
    FAIL() << "expected std::system_error";
  } catch (const std::system_error& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find(path), std::string::npos) << message;
    EXPECT_NE(message.find(action), std::string::npos) << message;
    if (!reason.empty()) {
      EXPECT_NE(message.find(reason), std::string::npos) << message;
    }
  }
}

}  // namespace

class FileUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override { temp_dir_ = std::make_unique<TempDir>(); }

  [[nodiscard]] std::string file(const std::string& name) const {
    return temp_dir_->file(name);
  }
  [[nodiscard]] std::string dir() const { return temp_dir_->path(); }

  std::unique_ptr<TempDir> temp_dir_;
};

// ==================== 判断 ====================

TEST_F(FileUtilsTest, DetectsExistingFileAndDirectory) {
  const std::string path = file("a.txt");
  write_file(path, "hello");

  EXPECT_TRUE(exists(path));
  EXPECT_TRUE(is_regular_file(path));
  EXPECT_FALSE(is_directory(path));

  EXPECT_TRUE(exists(dir()));
  EXPECT_TRUE(is_directory(dir()));
  EXPECT_FALSE(is_regular_file(dir()));
}

TEST_F(FileUtilsTest, MissingPathDoesNotExist) {
  EXPECT_FALSE(exists(file("nope.txt")));
  EXPECT_FALSE(is_directory(file("nope.txt")));
  EXPECT_FALSE(is_regular_file(file("nope.txt")));
  EXPECT_FALSE(is_symlink(file("nope.txt")));
}

TEST_F(FileUtilsTest, DetectsSymlinkWithoutFollowingIt) {
  const std::string target = file("target.txt");
  const std::string link = file("link.txt");
  write_file(target, "hello");
  ASSERT_EQ(0, ::symlink(target.c_str(), link.c_str()));

  EXPECT_TRUE(is_symlink(link));
  EXPECT_FALSE(is_symlink(target));
  // 判断存在性、类型时跟随链接：链接指向普通文件，它就是个普通文件。
  EXPECT_TRUE(exists(link));
  EXPECT_TRUE(is_regular_file(link));
}

TEST_F(FileUtilsTest, DanglingSymlinkIsSymlinkButDoesNotExist) {
  const std::string link = file("dangling.txt");
  ASSERT_EQ(0, ::symlink(file("missing.txt").c_str(), link.c_str()));

  // 链接本身在，但目标不在：这是两个不同的问题，答案也不同。
  EXPECT_TRUE(is_symlink(link));
  EXPECT_FALSE(exists(link));
  EXPECT_FALSE(is_regular_file(link));
}

TEST_F(FileUtilsTest, SymlinkToDirectoryIsDirectory) {
  const std::string link = file("dir_link");
  ASSERT_EQ(0, ::symlink(dir().c_str(), link.c_str()));

  EXPECT_TRUE(is_directory(link));
  EXPECT_FALSE(is_regular_file(link));
}

// ==================== 大小 ====================

TEST_F(FileUtilsTest, ReportsFileSize) {
  const std::string path = file("a.txt");
  write_file(path, "0123456789");

  EXPECT_EQ(10u, file_size(path));
}

TEST_F(FileUtilsTest, EmptyFileHasZeroSize) {
  const std::string path = file("empty.txt");
  write_file(path, "");

  EXPECT_EQ(0u, file_size(path));
}

TEST_F(FileUtilsTest, FileSizeOfMissingFileThrowsWithPath) {
  const std::string path = file("missing.txt");
  ExpectSystemError(path, "cannot get size of", "", [&] { (void)file_size(path); });
}

// ==================== 读 ====================

TEST_F(FileUtilsTest, ReadsFileContent) {
  const std::string path = file("a.txt");
  write_file(path, "hello world");

  EXPECT_EQ("hello world", read_file(path));
}

TEST_F(FileUtilsTest, ReadsEmptyFile) {
  const std::string path = file("empty.txt");
  write_file(path, "");

  EXPECT_EQ("", read_file(path));
}

TEST_F(FileUtilsTest, ReadsContentWithEmbeddedNullBytes) {
  const std::string path = file("binary.bin");
  const std::string content("a\0b\0c", 5);
  write_file(path, content);

  EXPECT_EQ(content, read_file(path));
}

TEST_F(FileUtilsTest, ReadsFileAsBytes) {
  const std::string path = file("binary.bin");
  const std::string content("a\0b", 3);
  write_file(path, content);

  const std::vector<uint8_t> bytes = read_file_bytes(path);
  ASSERT_EQ(3u, bytes.size());
  EXPECT_EQ('a', bytes[0]);
  EXPECT_EQ(0u, bytes[1]);
  EXPECT_EQ('b', bytes[2]);
}

TEST_F(FileUtilsTest, ReadsEmptyFileAsBytes) {
  const std::string path = file("empty.bin");
  write_file(path, "");

  EXPECT_TRUE(read_file_bytes(path).empty());
}

TEST_F(FileUtilsTest, ReadMissingFileThrowsWithPath) {
  const std::string path = file("missing.txt");
  ExpectSystemError(path, "cannot open file", "No such file or directory",
                     [&] { (void)read_file(path); });
}

// 非普通文件也要能读：设备文件和 /proc 下的伪文件都是合法的读取目标。
TEST_F(FileUtilsTest, ReadsDeviceFile) {
  EXPECT_EQ("", read_file("/dev/null"));
}

TEST_F(FileUtilsTest, ReadsProcFile) {
  const std::string content = read_file("/proc/self/status");
  EXPECT_FALSE(content.empty());
  EXPECT_NE(std::string::npos, content.find("Pid:"));
}

TEST_F(FileUtilsTest, ReadDirectoryThrows) {
  ExpectSystemError(dir(), "cannot read file", "Is a directory",
                     [&] { (void)read_file(dir()); });
}

// 权限不足时错误信息要说明真实原因，不能只说一个笼统的 I/O 错误。
TEST_F(FileUtilsTest, ReadWithoutPermissionReportsPermissionDenied) {
  if (::geteuid() == 0) GTEST_SKIP() << "root 可以读任何文件";

  const std::string path = file("secret.txt");
  write_file(path, "secret");
  ASSERT_EQ(0, ::chmod(path.c_str(), 0));

  ExpectSystemError(path, "cannot open file", "Permission denied",
                     [&] { (void)read_file(path); });
}

// ==================== 写 ====================

TEST_F(FileUtilsTest, WriteCreatesFile) {
  const std::string path = file("a.txt");
  write_file(path, "hello");

  EXPECT_TRUE(exists(path));
  EXPECT_EQ("hello", read_file(path));
}

TEST_F(FileUtilsTest, WriteOverwritesExistingFile) {
  const std::string path = file("a.txt");
  write_file(path, "old content");
  write_file(path, "new");

  EXPECT_EQ("new", read_file(path));
  EXPECT_EQ(3u, file_size(path));
}

TEST_F(FileUtilsTest, WriteBytesCreatesFile) {
  const std::string path = file("binary.bin");
  const std::vector<uint8_t> content{'a', 0, 'b'};
  write_file_bytes(path, content);

  const std::vector<uint8_t> read_back = read_file_bytes(path);
  EXPECT_EQ(content, read_back);
}

TEST_F(FileUtilsTest, WriteToMissingDirectoryThrowsWithPath) {
  const std::string path = file("no_such_dir/a.txt");
  ExpectSystemError(path, "cannot open file", "No such file or directory",
                     [&] { write_file(path, "hello"); });
}

TEST_F(FileUtilsTest, WriteKeepsExistingFilePermissions) {
  const std::string path = file("a.txt");
  write_file(path, "hello");
  ASSERT_EQ(0, ::chmod(path.c_str(), 0600));

  write_file(path, "world");
  EXPECT_EQ(0600, FileMode(path));
}

// ==================== 原子写 ====================

TEST_F(FileUtilsTest, AtomicWriteCreatesFile) {
  const std::string path = file("a.txt");
  write_file_atomic(path, "hello");

  EXPECT_EQ("hello", read_file(path));
}

TEST_F(FileUtilsTest, AtomicWriteReplacesExistingContent) {
  const std::string path = file("a.txt");
  write_file(path, "old content");
  write_file_atomic(path, "new");

  EXPECT_EQ("new", read_file(path));
}

TEST_F(FileUtilsTest, AtomicWriteLeavesNoTemporaryFilesBehind) {
  const std::string path = file("a.txt");
  write_file_atomic(path, "hello");

  std::vector<std::string> names = list_directory(dir());
  std::sort(names.begin(), names.end());
  ASSERT_EQ(1u, names.size()) << "temporary file was not cleaned up";
  EXPECT_EQ("a.txt", names[0]);
}

TEST_F(FileUtilsTest, AtomicWriteToMissingDirectoryThrowsWithPath) {
  const std::string path = file("no_such_dir/a.txt");
  ExpectSystemError(path, "cannot create temporary file for",
                     "No such file or directory",
                     [&] { write_file_atomic(path, "hello"); });
}

// 目标文件名很长时，临时文件名也必须留在 NAME_MAX 之内。
TEST_F(FileUtilsTest, AtomicWriteSupportsLongFileNames) {
  const std::string name(240, 'n');
  const std::string path = file(name);
  write_file_atomic(path, "hello");

  EXPECT_EQ("hello", read_file(path));
  EXPECT_EQ(1u, list_directory(dir()).size());
}

// 原子替换不能把目标文件的权限悄悄放宽。
TEST_F(FileUtilsTest, AtomicWriteKeepsExistingFilePermissions) {
  const std::string path = file("secret.txt");
  write_file(path, "old");
  ASSERT_EQ(0, ::chmod(path.c_str(), 0600));

  write_file_atomic(path, "new");
  EXPECT_EQ("new", read_file(path));
  EXPECT_EQ(0600, FileMode(path));
}

// 失败时目标内容保持不变，临时文件也要清干净。
TEST_F(FileUtilsTest, FailedAtomicWriteKeepsOriginalContent) {
  const std::string path = file("a.txt");
  write_file(path, "original");

  // 目标是个目录，rename 必然失败。
  const std::string directory_path = file("subdir");
  ASSERT_EQ(0, ::mkdir(directory_path.c_str(), 0755));

  ExpectSystemError(directory_path, "cannot replace file", "",
                     [&] { write_file_atomic(directory_path, "x"); });

  EXPECT_EQ("original", read_file(path));
  EXPECT_EQ(2u, list_directory(dir()).size());
}

// 新建文件的权限要和普通写一致（都是 0666 交给 umask 裁剪）。
TEST_F(FileUtilsTest, AtomicWriteNewFileUsesSamePermissionsAsWriteFile) {
  const std::string plain = file("plain.txt");
  const std::string atomic = file("atomic.txt");
  write_file(plain, "x");
  write_file_atomic(atomic, "x");

  EXPECT_EQ(FileMode(plain), FileMode(atomic));
}

// 目标没有父目录时，临时文件落在当前目录。
TEST_F(FileUtilsTest, AtomicWriteWithoutParentDirectoryUsesCurrentDirectory) {
  const std::string name = "file_utils_relative_write_test.tmp";
  const fs::path target = fs::current_path() / name;
  std::error_code ec;
  fs::remove(target, ec);

  try {
    write_file_atomic(name, "hello");
  } catch (const std::system_error&) {
    fs::remove(target, ec);
    GTEST_SKIP() << "current directory is not writable";
  }

  EXPECT_EQ("hello", read_file(target.string()));
  fs::remove(target, ec);
}

// 并发原子写：读者永远只能看到某一次写入的完整内容，不会看到半截。
TEST_F(FileUtilsTest, ConcurrentAtomicWritesNeverExposePartialContent) {
  const std::string path = file("shared.bin");
  constexpr int kWriters = 8;
  constexpr int kRounds = 20;

  std::vector<std::string> payloads;
  payloads.reserve(kWriters);
  for (int i = 0; i < kWriters; ++i) {
    payloads.emplace_back(64 * 1024, static_cast<char>('A' + i));
  }

  std::atomic<bool> stop{false};
  std::atomic<bool> reader_saw_partial{false};

  std::thread reader([&] {
    while (!stop.load()) {
      try {
        const std::string content = read_file(path);
        if (content.empty()) continue;  // 还没写过
        if (std::find(payloads.begin(), payloads.end(), content) ==
            payloads.end()) {
          reader_saw_partial = true;
          return;
        }
      } catch (const std::system_error&) {
        // 文件还没创建，继续等。
      }
    }
  });

  std::vector<std::thread> writers;
  for (int i = 0; i < kWriters; ++i) {
    writers.emplace_back([&, i] {
      for (int round = 0; round < kRounds; ++round) {
        write_file_atomic(path, payloads[i]);
      }
    });
  }
  for (std::thread& writer : writers) writer.join();

  stop = true;
  reader.join();

  EXPECT_FALSE(reader_saw_partial.load());

  const std::string final_content = read_file(path);
  EXPECT_NE(payloads.end(),
            std::find(payloads.begin(), payloads.end(), final_content));
}

// ==================== 列举 ====================

TEST_F(FileUtilsTest, ListsDirectoryEntryNames) {
  write_file(file("a.txt"), "a");
  write_file(file("b.txt"), "b");

  std::vector<std::string> names = list_directory(dir());
  std::sort(names.begin(), names.end());

  ASSERT_EQ(2u, names.size());
  EXPECT_EQ("a.txt", names[0]);
  EXPECT_EQ("b.txt", names[1]);
}

TEST_F(FileUtilsTest, ListingSkipsDotAndDotDot) {
  write_file(file("a.txt"), "a");

  for (const std::string& name : list_directory(dir())) {
    EXPECT_NE(".", name);
    EXPECT_NE("..", name);
  }
}

TEST_F(FileUtilsTest, ListingEmptyDirectoryYieldsNoEntries) {
  EXPECT_TRUE(list_directory(dir()).empty());
}

TEST_F(FileUtilsTest, ListDirectoryOfFileThrowsWithPath) {
  const std::string path = file("a.txt");
  write_file(path, "a");

  ExpectSystemError(path, "cannot list directory", "Not a directory",
                     [&] { (void)list_directory(path); });
}

TEST_F(FileUtilsTest, ListDirectoryOfMissingPathThrowsWithPath) {
  const std::string path = file("no_such_dir");
  ExpectSystemError(path, "cannot list directory", "No such file or directory",
                     [&] { (void)list_directory(path); });
}
