#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// 文件系统工具：存在性判断、读写、目录列举、原子替换。
//
// 约定：
// - 路径统一用 const std::string& 传入。std::string 保证以 NUL 结尾，
//   所以路径可以直接交给 open(2) / stat(2) 这类 C 接口，不需要在实现
//   里再物化一份。模块不保存任何路径状态。
// - 判断类函数（exists / is_*）不抛异常：查询失败（权限不足、路径非法、
//   中间目录不存在）一律当作"不存在"返回 false。这与 std::filesystem
//   的约定一致，也因为"查不到"和"不在"对调用方的意义相同——真要区分，
//   用 file_size / read_file 那类会抛异常的函数。
// - 真正要读写的函数，失败一律抛 std::system_error，错误信息里带完整
//   路径和底层 errno 原因，便于定位是哪个文件出的问题。
// - 本模块只处理已经成型的路径，不做拼接和规范化，那是 path_utils 的事。
//
// 全部函数不持有可变状态，**可以并发调用**；同一路径上的并发写不在保证
// 范围内（并发写请用 write_file_atomic，它对单个目标文件是原子的）。

namespace common::util {

// ==================== 判断 ====================

// 路径是否存在。跟随符号链接：指向不存在目标的软链接返回 false。
[[nodiscard]] bool exists(const std::string& path) noexcept;

// 是否是目录。跟随符号链接：指向目录的软链接返回 true。
[[nodiscard]] bool is_directory(const std::string& path) noexcept;

// 是否是普通文件。跟随符号链接。
[[nodiscard]] bool is_regular_file(const std::string& path) noexcept;

// 路径本身是不是符号链接。不跟随：即使软链接指向不存在的目标也为 true。
[[nodiscard]] bool is_symlink(const std::string& path) noexcept;

// ==================== 大小 ====================

// 文件大小（字节）。跟随符号链接。路径不存在、不是普通文件、读不到
// 元信息时抛 std::system_error。
//
// 注意：按 std::filesystem 的语义，只有普通文件才有大小。设备文件、管道、
// /proc 下的伪文件会抛 std::system_error（Operation not supported），
// 这不是 bug。想知道设备/伪文件的内容请用 read_file。
[[nodiscard]] uint64_t file_size(const std::string& path);

// ==================== 读 ====================

// 读整个文件。std::string 能装任意字节，文本和二进制内容都能用它承载。
// 文件不存在或读失败抛 std::system_error。
//
// 不限于普通文件：设备文件（/dev/null）、/proc 下的伪文件都能读，读多少
// 取决于有多少数据可读。目录会读失败（Is a directory）。
//
// 命名管道（FIFO）也能读，但按 POSIX 语义，open 会一直阻塞到有写端打开
// 为止——这是 FIFO 本身的语义，不是本函数的问题。
//
// 整个文件读进内存，大文件请自行评估内存占用。
[[nodiscard]] std::string read_file(const std::string& path);

// 读整个文件为字节序列。语义同 read_file，只是结果类型不同。
[[nodiscard]] std::vector<uint8_t> read_file_bytes(const std::string& path);

// ==================== 写 ====================

// 写文件，存在则覆盖。父目录必须已存在（不会自动创建）。
// 打开失败或写失败抛 std::system_error。
//
// 注意：这不是原子写。写到一半失败会留下半截内容，需要"要么旧内容、
// 要么完整新内容"请用 write_file_atomic。
//
// 目标已存在时沿用原文件的权限；新建文件按 0666 交给 umask 裁剪。
// 不做 fsync：内容可能还在页缓存里，需要掉电持久性请用 write_file_atomic。
void write_file(const std::string& path, std::string_view content);

// 写文件为字节序列。语义同 write_file。
void write_file_bytes(const std::string& path,
                      const std::vector<uint8_t>& content);

// 原子替换文件内容。
//
// 做法是在目标同目录下写一个临时文件，fsync 落盘后用 rename(2) 替换
// 目标。同目录是必须的：rename 只有在同一文件系统内才是原子的。
// 任何一步失败都不会让目标处于半写状态，临时文件会被清理掉。
//
// 保证的是"读取者要么看到旧内容、要么看到完整的新内容"，并且内容在
// rename 之前已经 fsync 落盘，掉电后不会退化成空文件。rename 之后还会
// 对父目录做一次 fsync（best effort），让目录项本身也落盘；这一步失败
// 不影响函数返回值，但此时掉电持久性不再保证。
//
// 目标不存在时是创建。目标已存在时沿用原文件的权限，不会因为替换而把
// 0600 的文件悄悄放宽成 0644。
//
// 并发安全：对同一个目标路径，多个线程同时调用不会互相破坏，各自的临时
// 文件互相独立，最终目标文件是其中一次写入的完整内容。
void write_file_atomic(const std::string& path, std::string_view content);

// ==================== 列举 ====================

// 列举目录下的条目名。只返回文件名（不含目录前缀），不递归，不含 "."
// 和 ".."。返回顺序是文件系统顺序，不保证排序，调用方需要有序结果请
// 自行排序。
//
// 路径不是目录或打不开时抛 std::system_error。
[[nodiscard]] std::vector<std::string> list_directory(const std::string& path);

}  // namespace common::util
