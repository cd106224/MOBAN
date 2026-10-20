#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 本模块的函数全部是无状态的纯函数：不持有可变状态，不访问文件或全局
// 变量，调用之间没有顺序依赖。**所有函数都可以被多线程并发调用。**
namespace common::util {

// ==================== 裁剪 ====================
//
// 这里的"空白"指 ASCII 空白：' '、'\t'、'\n'、'\v'、'\f'、'\r'。
// 不用 locale 相关的 isspace：它受全局 locale 影响，且对负值 char
// 是未定义行为。
//
// 返回的 string_view 指向 input 内部，不发生拷贝。调用方必须保证
// input 的底层存储比返回值活得久（不要对临时 std::string 调用）。

namespace detail {

[[nodiscard]] inline bool is_ascii_whitespace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
         c == '\r';
}

[[nodiscard]] inline char to_lower_ascii(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] inline char to_upper_ascii(char c) noexcept {
  return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

// join 的元素可以是 std::string、std::string_view 或 const char*。
// const char* 重载把 nullptr 当成空串：把空指针直接交给 string_view 的
// 构造函数是未定义行为，给一个空视图更安全。
[[nodiscard]] inline std::string_view as_string_view(
    std::string_view value) noexcept {
  return value;
}

[[nodiscard]] inline std::string_view as_string_view(
    const std::string& value) noexcept {
  return value;
}

[[nodiscard]] inline std::string_view as_string_view(
    const char* value) noexcept {
  return value == nullptr ? std::string_view() : std::string_view(value);
}

}  // namespace detail

[[nodiscard]] inline std::string_view ltrim(std::string_view input) noexcept {
  size_t begin = 0;
  while (begin < input.size() && detail::is_ascii_whitespace(input[begin])) {
    ++begin;
  }
  return input.substr(begin);
}

[[nodiscard]] inline std::string_view rtrim(std::string_view input) noexcept {
  size_t end = input.size();
  while (end > 0 && detail::is_ascii_whitespace(input[end - 1])) --end;
  return input.substr(0, end);
}

[[nodiscard]] inline std::string_view trim(std::string_view input) noexcept {
  size_t begin = 0;
  size_t end = input.size();
  while (begin < end && detail::is_ascii_whitespace(input[begin])) ++begin;
  while (end > begin && detail::is_ascii_whitespace(input[end - 1])) --end;
  return input.substr(begin, end - begin);
}

// ==================== 切分与拼接 ====================

// 按 delimiter 切分。返回的视图指向 input 内部，生命周期要求同 trim。
//
// 语义约定：
// - delimiter 为空时抛 std::invalid_argument（空分隔符没有切分语义）。
// - 连续分隔符产生空字段（"a,,b" -> {"a", "", "b"}），不静默丢弃。
// - 空输入返回单个空字段，保证"字段数 = 分隔符数 + 1"。
[[nodiscard]] std::vector<std::string_view> split(std::string_view input,
                                                  std::string_view delimiter);

// 用 delimiter 连接各段。Range 的元素可以是 std::string、
// std::string_view 或 const char*。
template <typename Range>
[[nodiscard]] std::string join(const Range& parts, std::string_view delimiter) {
  size_t total = 0;
  size_t count = 0;
  for (const auto& part : parts) {
    total += detail::as_string_view(part).size();
    ++count;
  }
  if (count > 0) total += delimiter.size() * (count - 1);

  std::string result;
  result.reserve(total);
  bool first = true;
  for (const auto& part : parts) {
    if (!first) result.append(delimiter);
    result.append(detail::as_string_view(part));
    first = false;
  }
  return result;
}

// ==================== 大小写 ====================
//
// 只做 ASCII 大小写转换。协议字段、配置项这类标识符基本都是 ASCII，
// 按字节处理可以避免多字节字符被拆坏，也避免引入 locale 依赖。

[[nodiscard]] std::string to_lower(std::string_view input);
[[nodiscard]] std::string to_upper(std::string_view input);

// 大小写无关的比较。
[[nodiscard]] inline bool iequals(std::string_view lhs,
                                  std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (detail::to_lower_ascii(lhs[i]) != detail::to_lower_ascii(rhs[i])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool istarts_with(std::string_view input,
                                       std::string_view prefix) noexcept {
  if (input.size() < prefix.size()) return false;
  return iequals(input.substr(0, prefix.size()), prefix);
}

[[nodiscard]] inline bool iends_with(std::string_view input,
                                     std::string_view suffix) noexcept {
  if (input.size() < suffix.size()) return false;
  return iequals(input.substr(input.size() - suffix.size()), suffix);
}

// ==================== 前后缀 ====================

[[nodiscard]] inline bool starts_with(std::string_view input,
                                      std::string_view prefix) noexcept {
  if (input.size() < prefix.size()) return false;
  return input.compare(0, prefix.size(), prefix) == 0;
}

[[nodiscard]] inline bool ends_with(std::string_view input,
                                    std::string_view suffix) noexcept {
  if (input.size() < suffix.size()) return false;
  return input.compare(input.size() - suffix.size(), suffix.size(), suffix) ==
         0;
}

// ==================== 替换 ====================
//
// from 为空时抛 std::invalid_argument：替换空串没有可定义的语义，
// 静默返回原串只会掩盖调用方的 bug。

[[nodiscard]] std::string replace_all(std::string_view input,
                                      std::string_view from,
                                      std::string_view to);
[[nodiscard]] std::string replace_first(std::string_view input,
                                        std::string_view from,
                                        std::string_view to);

// ==================== 数字与布尔转换 ====================
//
// 严格解析：整串必须是一个完整的数。首尾空白会被裁掉，串内空白不允许。
// 格式非法抛 std::invalid_argument，数值越界抛 std::out_of_range——
// 两者的处置方式不同，调用方需要区分。context 是可选的定位信息
// （配置文件里传字段路径），会出现在异常信息里。
//
// 判定顺序固定为"格式优先，范围其次"：
//   - 串尾有没消费掉的字符（"12x"、"1e400x"）-> 格式非法。
//     "99999999999999999999x" 报格式非法而不是越界：多出来的 'x' 才是
//     真正的问题，报越界会让人去改数字大小，改完还是错。
//   - 格式合法但数值放不进目标类型 -> 越界。
//
// 浮点数的具体规则：
//   - 只接受十进制字面量。十六进制浮点（"0x1p3"）、inf、nan 一律按
//     格式非法处理。
//   - subnormal（渐进下溢区里的数，如 "5e-324"）是完全可表示的数，
//     按正常值返回，不算越界。
//   - 上溢（"1e400"）和下溢到 0（"1e-400"）按越界处理。
[[nodiscard]] int64_t to_int64(std::string_view input,
                               std::string_view context = {});
[[nodiscard]] uint64_t to_uint64(std::string_view input,
                                 std::string_view context = {});
[[nodiscard]] double to_double(std::string_view input,
                               std::string_view context = {});
[[nodiscard]] bool to_bool(std::string_view input,
                           std::string_view context = {});

// 不抛异常的版本：失败返回 std::nullopt，调用方必须显式处理失败。
// 需要区分"格式非法"和"越界"时用上面抛异常的版本，异常类型已经带上了
// 这个区别。
// to_bool 接受（大小写不敏感）true/false、1/0、yes/no、on/off。
//
// try_to_int64 / try_to_uint64 不分配内存，可以标 noexcept；try_to_double
// 和 try_to_bool 需要构造临时字符串，分配失败会抛 bad_alloc，所以不标。
[[nodiscard]] std::optional<int64_t> try_to_int64(
    std::string_view input) noexcept;
[[nodiscard]] std::optional<uint64_t> try_to_uint64(
    std::string_view input) noexcept;
[[nodiscard]] std::optional<double> try_to_double(std::string_view input);
[[nodiscard]] std::optional<bool> try_to_bool(std::string_view input);

}  // namespace common::util
