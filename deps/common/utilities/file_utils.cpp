#include "file_utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <new>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace common::util {
namespace {

namespace fs = std::filesystem;

constexpr size_t kReadBufferSize = 64 * 1024;

// Linux 上单个文件名的上限（NAME_MAX）。临时文件名必须留在这个预算内，
// 否则长文件名的目标会因为临时名超长而无法原子写。
constexpr size_t kMaxFileNameLength = 255;

// 写入路径需要裸文件描述符（fsync、O_EXCL 独占创建），标准库没有对应
// 接口，先在本地用 RAII 管住；utilities/unique_fd 落地后换成它。
class ScopedFd {
 public:
  explicit ScopedFd(int fd) noexcept : fd_(fd) {}
  ~ScopedFd() { reset(); }

  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;

  ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  ScopedFd& operator=(ScopedFd&& other) noexcept {
    if (this != &other) {
      reset();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept { return fd_; }

  // 关闭并交出所有权。关闭失败返回 false，errno 留给调用方。
  bool close() noexcept {
    if (fd_ < 0) return true;
    const int fd = fd_;
    fd_ = -1;
    return ::close(fd) == 0;
  }

 private:
  void reset() noexcept {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  int fd_ = -1;
};

std::error_code ErrnoCode() noexcept {
  return std::error_code(errno, std::generic_category());
}

// 统一的异常出口：system_error 会把错误原因接在消息后面，最终形如
// "cannot read file '/tmp/x': No such file or directory"。
[[noreturn]] void ThrowFileError(const std::error_code& ec,
                                 std::string_view action,
                                 const std::string& path) {
  std::string message{action};
  message.append(" '");
  message.append(path);
  message.append("'");
  throw std::system_error(ec, message);
}

// 内存不足也统一成 std::system_error，调用方只需要处理一种异常类型。
[[noreturn]] void ThrowOutOfMemory(std::string_view action,
                                   const std::string& path) {
  ThrowFileError(std::make_error_code(std::errc::not_enough_memory), action,
                 path);
}

// 判断类函数的统一入口：查询失败一律按"不存在"处理。构造路径需要
// 分配内存，内存不足时同样按"查不到"处理——这类函数只回答"在不在"，
// 抛异常反而让调用方多写一层处理。
template <typename Predicate>
bool QueryPath(const std::string& path, Predicate&& predicate) noexcept {
  try {
    const fs::path target{path};
    std::error_code ec;
    return predicate(target, ec) && !ec;
  } catch (const std::bad_alloc&) {
    return false;
  }
}

// 读取的目标缓冲区。字符串和字节序列两种结果类型共用一套 I/O 逻辑，
// 差别只在"往哪放"。
struct StringSink {
  std::string value;

  void reserve(size_t size) { value.reserve(size); }
  void append(const char* data, size_t size) { value.append(data, size); }
};

struct BytesSink {
  std::vector<uint8_t> value;

  void reserve(size_t size) { value.reserve(size); }
  void append(const char* data, size_t size) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    value.insert(value.end(), bytes, bytes + size);
  }
};

// 读循环。
//
// 用 open(2) + read(2) 而不是 ifstream，原因有两个：errno 不丢，
// 错误信息能说明到底是权限不足还是别的；能读设备文件、管道和
// /proc 下的伪文件（ifstream 配 file_size 预读会在这些文件上直接失败）。
template <typename Sink>
void ReadInto(const std::string& path, Sink& sink) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) ThrowFileError(ErrnoCode(), "cannot open file", path);

  ScopedFd file(fd);
  try {
    // 大小用来预留容量。只对普通文件有意义：设备、管道、/proc 伪文件
    // 的 st_size 是 0 或不可用，预留不了也不影响正确性。
    struct stat st{};
    if (::fstat(file.get(), &st) == 0 && S_ISREG(st.st_mode) &&
        st.st_size > 0) {
      sink.reserve(static_cast<size_t>(st.st_size));
    }

    char buffer[kReadBufferSize];
    for (;;) {
      const ssize_t result = ::read(file.get(), buffer, sizeof buffer);
      if (result < 0) {
        // 被信号打断不是错误，重试。
        if (errno == EINTR) continue;
        ThrowFileError(ErrnoCode(), "cannot read file", path);
      }
      if (result == 0) break;
      sink.append(buffer, static_cast<size_t>(result));
    }
  } catch (const std::bad_alloc&) {
    ThrowOutOfMemory("cannot read file", path);
  }
}

void WriteAll(int fd, std::string_view content, const std::string& path) {
  size_t written = 0;
  while (written < content.size()) {
    const ssize_t result =
        ::write(fd, content.data() + written, content.size() - written);
    if (result < 0) {
      // 被信号打断不是错误，重试。
      if (errno == EINTR) continue;
      ThrowFileError(ErrnoCode(), "cannot write file", path);
    }
    if (result == 0) {
      ThrowFileError(std::make_error_code(std::errc::io_error),
                     "cannot write file", path);
    }
    written += static_cast<size_t>(result);
  }
}

// 目标的权限：只有普通文件才继承。符号链接、目录一律用新建文件的默认
// 权限——符号链接被原子替换掉之后本来就该是个普通文件。
//
// setuid / setgid 位不继承：原子替换写入的是新内容，保留 setuid 会让
// 新内容以特权身份运行。
bool TargetMode(const std::string& path, mode_t& mode) noexcept {
  struct stat st{};
  if (::lstat(path.c_str(), &st) != 0) return false;
  if (!S_ISREG(st.st_mode)) return false;
  mode = st.st_mode & 0777;
  return true;
}

// 临时文件名的随机后缀。名字要不可预测：临时文件落在目标同目录，
// 同目录下的其他用户能列目录，撞上名字就能把我们的临时文件换掉。
uint64_t RandomBits() noexcept {
  try {
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
  } catch (const std::exception&) {
    // random_device 不可用时退回 pid + 序号，安全性靠后面的
    // dev/ino 校验兜底。
    return 0;
  }
}

std::string HexString(uint64_t value) {
  char buffer[17];
  std::snprintf(buffer, sizeof buffer, "%016llx",
                static_cast<unsigned long long>(value));
  return std::string(buffer);
}

// 临时文件的清理守卫：析构时如果文件还没被 rename 走就删掉，避免失败
// 路径上留下垃圾。
class TempFileGuard {
 public:
  explicit TempFileGuard(std::string path) : path_(std::move(path)) {}
  ~TempFileGuard() {
    if (!path_.empty()) ::unlink(path_.c_str());
  }

  TempFileGuard(const TempFileGuard&) = delete;
  TempFileGuard& operator=(const TempFileGuard&) = delete;

  void release() noexcept { path_.clear(); }

 private:
  std::string path_;
};

// 临时文件：fd 和磁盘上的路径成对存在。
struct TempFile {
  ScopedFd fd;
  std::string path;
};

// 在 directory 下创建一个独占的临时文件。名字带目标文件名、进程号、
// 序号和随机数，配合 O_EXCL 保证并发下不会撞名；万一撞了就换下一个
// 序号重试。
//
// 权限按 0666 创建，让内核按 umask 裁剪——调用方随后会用 fchmod
// 改成最终权限（目标已存在时沿用目标权限）。
TempFile CreateTempFile(const std::string& directory, const std::string& name,
                        const std::string& target) {
  static std::atomic<uint64_t> counter{0};
  const std::string prefix = "." + name + ".tmp.";

  for (int attempt = 0; attempt < 128; ++attempt) {
    const std::string suffix =
        std::to_string(static_cast<uint64_t>(::getpid())) + "." +
        std::to_string(counter.fetch_add(1)) + "." + HexString(RandomBits());

    // 整个文件名不能超过 NAME_MAX，否则 open 会以 ENAMETOOLONG 失败。
    // 目标文件名很长时截断前缀，牺牲可读性换正确性。
    const size_t budget =
        kMaxFileNameLength - suffix.size() - 1;  // -1 留给结尾的 NUL
    std::string head = prefix;
    if (head.size() > budget) head.resize(budget);

    const std::string candidate = directory + "/" + head + suffix;

    const int fd = ::open(candidate.c_str(),
                          O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd >= 0) return TempFile{ScopedFd(fd), candidate};
    if (errno != EEXIST) {
      ThrowFileError(ErrnoCode(), "cannot create temporary file for", target);
    }
  }
  ThrowFileError(std::make_error_code(std::errc::file_exists),
                 "cannot create temporary file for", target);
}

// 确认临时路径指向的仍是我们打开的那个文件。同目录下的其他用户可以在
// 我们写文件期间把它换掉（unlink + 换成自己的文件），那样 rename 就会
// 把别人的内容搬到目标位置。比对 dev/ino 能把这种情况挡下来。
void VerifyTempFile(int fd, const std::string& temp_path,
                    const std::string& target) {
  struct stat fd_st{};
  struct stat path_st{};
  if (::fstat(fd, &fd_st) != 0) {
    ThrowFileError(ErrnoCode(), "cannot stat temporary file for", target);
  }
  if (::lstat(temp_path.c_str(), &path_st) != 0) {
    ThrowFileError(ErrnoCode(), "cannot stat temporary file for", target);
  }
  if (fd_st.st_dev != path_st.st_dev || fd_st.st_ino != path_st.st_ino) {
    ThrowFileError(std::make_error_code(std::errc::io_error),
                   "temporary file was replaced while writing", target);
  }
}

// rename 之后对父目录 fsync，让目录项本身也落盘。失败不影响函数结果：
// 这时文件内容已经替换成功，只是掉电后目录项可能回退。
void SyncDirectory(const std::string& directory) noexcept {
  const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) return;
  ::fsync(fd);
  ::close(fd);
}

}  // namespace

bool exists(const std::string& path) noexcept {
  return QueryPath(path, [](const fs::path& target, std::error_code& ec) {
    return fs::exists(target, ec);
  });
}

bool is_directory(const std::string& path) noexcept {
  return QueryPath(path, [](const fs::path& target, std::error_code& ec) {
    return fs::is_directory(fs::status(target, ec));
  });
}

bool is_regular_file(const std::string& path) noexcept {
  return QueryPath(path, [](const fs::path& target, std::error_code& ec) {
    return fs::is_regular_file(fs::status(target, ec));
  });
}

bool is_symlink(const std::string& path) noexcept {
  return QueryPath(path, [](const fs::path& target, std::error_code& ec) {
    // 不跟随链接：链接指向不存在的目标时也返回 true。
    return fs::is_symlink(fs::symlink_status(target, ec));
  });
}

uint64_t file_size(const std::string& path) {
  std::error_code ec;
  const uintmax_t size = fs::file_size(fs::path{path}, ec);
  if (ec) ThrowFileError(ec, "cannot get size of", path);
  return static_cast<uint64_t>(size);
}

std::string read_file(const std::string& path) {
  StringSink sink;
  ReadInto(path, sink);
  return std::move(sink.value);
}

std::vector<uint8_t> read_file_bytes(const std::string& path) {
  BytesSink sink;
  ReadInto(path, sink);
  return std::move(sink.value);
}

void write_file(const std::string& path, std::string_view content) {
  // 0666 交给 umask 裁剪，是创建文件时最常见的默认权限；目标已存在时
  // O_CREAT 不改权限，原权限保留。
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  if (fd < 0) ThrowFileError(ErrnoCode(), "cannot open file", path);

  ScopedFd file(fd);
  WriteAll(file.get(), content, path);
  if (!file.close()) ThrowFileError(ErrnoCode(), "cannot close file", path);
}

void write_file_bytes(const std::string& path,
                      const std::vector<uint8_t>& content) {
  // 空数组的 data() 可能是空指针，先兜成空串再构造视图。
  const char* data =
      content.empty() ? "" : reinterpret_cast<const char*>(content.data());
  write_file(path, std::string_view(data, content.size()));
}

void write_file_atomic(const std::string& path, std::string_view content) {
  // 临时文件必须和目标同目录：rename 只有在同一文件系统内才是原子的。
  const fs::path target{path};
  const fs::path directory =
      target.has_parent_path() ? target.parent_path() : fs::path{"."};
  const std::string directory_path = directory.string();

  mode_t target_mode = 0;
  const bool has_target_mode = TargetMode(path, target_mode);

  TempFile temp =
      CreateTempFile(directory_path, target.filename().string(), path);
  TempFileGuard guard(temp.path);

  // 临时文件以 0666 创建，内核按 umask 裁剪，这个权限就是"目标不存在时"
  // 新文件该有的权限。必须在收权限之前取。
  mode_t default_mode = 0;
  struct stat created_st{};
  if (::fstat(temp.fd.get(), &created_st) != 0) {
    ThrowFileError(ErrnoCode(), "cannot stat temporary file for", path);
  }
  default_mode = created_st.st_mode & 0777;

  // 写内容之前先把权限收到 0600：临时文件名带随机数，别人猜不到，
  // 但同目录的用户能列目录，收到 0600 可以避免半截内容被读到。
  if (::fchmod(temp.fd.get(), 0600) != 0) {
    ThrowFileError(ErrnoCode(), "cannot set permissions on", path);
  }

  WriteAll(temp.fd.get(), content, path);

  // 目标已存在时沿用原权限；否则用新建文件的默认权限。
  const mode_t final_mode = has_target_mode ? target_mode : default_mode;
  if (::fchmod(temp.fd.get(), final_mode) != 0) {
    ThrowFileError(ErrnoCode(), "cannot set permissions on", path);
  }

  // 先落盘再改名：否则 rename 之后立刻掉电，文件内容可能还是空的。
  if (::fsync(temp.fd.get()) != 0) {
    ThrowFileError(ErrnoCode(), "cannot flush file", path);
  }

  // 确认临时文件没被换掉再改名。
  VerifyTempFile(temp.fd.get(), temp.path, path);

  if (!temp.fd.close()) {
    ThrowFileError(ErrnoCode(), "cannot close file", path);
  }
  // rename 在同一文件系统内是原子的：读者要么看到旧文件，要么看到完整
  // 的新文件，不会看到写了一半的内容。
  if (::rename(temp.path.c_str(), path.c_str()) != 0) {
    ThrowFileError(ErrnoCode(), "cannot replace file", path);
  }
  guard.release();

  // 目录项也落盘，掉电后新文件名不会回退成旧文件。
  SyncDirectory(directory_path);
}

std::vector<std::string> list_directory(const std::string& path) {
  const fs::path directory{path};

  std::error_code ec;
  const fs::file_status status = fs::status(directory, ec);
  if (ec) ThrowFileError(ec, "cannot list directory", path);
  if (!fs::is_directory(status)) {
    ThrowFileError(std::make_error_code(std::errc::not_a_directory),
                   "cannot list directory", path);
  }

  std::vector<std::string> names;
  fs::directory_iterator it(directory, fs::directory_options::none, ec);
  if (ec) ThrowFileError(ec, "cannot list directory", path);

  const fs::directory_iterator end;
  while (it != end) {
    names.push_back(it->path().filename().string());
    it.increment(ec);
    // 迭代过程中出错时迭代器会变成 end，错误只能在这里看到。
    if (ec) ThrowFileError(ec, "cannot list directory", path);
  }
  return names;
}

}  // namespace common::util
