#include "string_utils.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <system_error>

namespace common::util {
namespace {

// strtod_l 需要一个显式的 locale_t：程序一旦调用过 setlocale，
// LC_NUMERIC 可能变成逗号小数点，"1.5" 就解析不出来了。locale_t 只
// 创建一次，进程退出时释放。
class CLocale {
 public:
  CLocale() : locale_(newlocale(LC_ALL_MASK, "C", nullptr)) {
    if (locale_ == nullptr) {
      throw std::runtime_error("cannot create the C locale");
    }
  }
  ~CLocale() { freelocale(locale_); }

  CLocale(const CLocale&) = delete;
  CLocale& operator=(const CLocale&) = delete;

  [[nodiscard]] locale_t get() const noexcept { return locale_; }

 private:
  locale_t locale_;
};

locale_t c_locale() {
  static const CLocale holder;
  return holder.get();
}

// 数字解析的失败原因要分类：格式非法说明数据写错了，越界说明数据本身
// 合法但放不进目标类型，两者的处置方式不同。
enum class NumberStatus { kOk, kInvalid, kOutOfRange };

// 判断字面量是不是"零"。用来区分"本来就是 0"和"下溢成了 0"：前者是
// 合法值，后者是精度损失。
bool IsZeroLiteral(std::string_view text) noexcept {
  size_t begin = 0;
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) begin = 1;

  bool has_digit = false;
  for (size_t i = begin; i < text.size(); ++i) {
    const char c = text[i];
    if (c == 'e' || c == 'E') break;  // 指数部分不影响"是不是零"
    if (c == '.') continue;
    if (c < '0' || c > '9') return false;  // 非法字符，交给后面的判定
    if (c != '0') return false;
    has_digit = true;
  }
  return has_digit;
}

// 十六进制浮点不是这里接受的字面量。strtod 按 C99 语义会把它解析成
// 正常数值（"0x1p3" -> 8），必须显式挡掉。
bool LooksLikeHexLiteral(std::string_view text) noexcept {
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
    text.remove_prefix(1);
  }
  return text.size() >= 2 && text[0] == '0' &&
         (text[1] == 'x' || text[1] == 'X');
}

// inf / nan 也不是这里接受的字面量。不能依赖标准库的解析器去拒绝它们：
// libstdc++ 的流解析会拒绝（字符集里没有 i/n），libc++ 内部走 strtod
// 则会解析成 infinity——同一个输入在不同工具链下结果不同。
bool LooksLikeNonFiniteLiteral(std::string_view text) {
  std::string lowered = to_lower(text);
  if (!lowered.empty() && (lowered.front() == '+' || lowered.front() == '-')) {
    lowered.erase(0, 1);
  }
  // "nan(...)" 带负载的形式也一并挡掉。
  return lowered.rfind("inf", 0) == 0 || lowered.rfind("nan", 0) == 0;
}

NumberStatus ParseInt64(std::string_view input, int64_t& out) {
  std::string_view text = trim(input);
  if (text.empty()) return NumberStatus::kInvalid;

  // from_chars 不接受前导 '+'，这里手工剥掉；它对负号是接受的。
  if (text.front() == '+') text.remove_prefix(1);
  if (text.empty()) return NumberStatus::kInvalid;

  int64_t value = 0;
  const char* const begin = text.data();
  const char* const end = begin + text.size();
  const std::from_chars_result result = std::from_chars(begin, end, value, 10);

  // 先看格式：整串必须被消费完。from_chars 在越界时会把 ptr 停在已解析
  // 的数字末尾，所以 "99999999999999999999x" 能把尾部的 'x' 区分出来。
  if (result.ptr != end) return NumberStatus::kInvalid;
  if (result.ec == std::errc::result_out_of_range) {
    return NumberStatus::kOutOfRange;
  }
  if (result.ec != std::errc()) return NumberStatus::kInvalid;

  out = value;
  return NumberStatus::kOk;
}

NumberStatus ParseUint64(std::string_view input, uint64_t& out) {
  std::string_view text = trim(input);
  if (text.empty()) return NumberStatus::kInvalid;

  if (text.front() == '+') text.remove_prefix(1);
  if (text.empty()) return NumberStatus::kInvalid;
  // 无符号类型不接受负号。标准规定 from_chars 的负号只对有符号类型成立
  // （"a minus sign is the only sign that may appear, and only if value
  // has a signed type"），这里仍显式挡一道：让这条规则在代码里可见，
  // 不依赖库实现替我们拒绝，也避免 "-1" 被读成 2^64-1。
  if (text.front() == '-') return NumberStatus::kInvalid;

  uint64_t value = 0;
  const char* const begin = text.data();
  const char* const end = begin + text.size();
  const std::from_chars_result result = std::from_chars(begin, end, value, 10);

  if (result.ptr != end) return NumberStatus::kInvalid;
  if (result.ec == std::errc::result_out_of_range) {
    return NumberStatus::kOutOfRange;
  }
  if (result.ec != std::errc()) return NumberStatus::kInvalid;

  out = value;
  return NumberStatus::kOk;
}

NumberStatus ParseDouble(std::string_view input, double& out) {
  const std::string text(trim(input));
  if (text.empty()) return NumberStatus::kInvalid;

  if (LooksLikeHexLiteral(text)) return NumberStatus::kInvalid;
  if (LooksLikeNonFiniteLiteral(text)) return NumberStatus::kInvalid;

  char* end = nullptr;
  // TODO: 工具链升到 GCC 11+ 后，std::from_chars 的浮点版本可以替掉
  // strtod_l：它不分配、不依赖 locale，语义完全由标准规定。本机还是
  // GCC 9.4，用 #if 分叉会让本地和 CI 的解析行为不一致，所以先不动。
  const double value = strtod_l(text.c_str(), &end, c_locale());

  // 串尾有没消费掉的字符（"1.5abc"、"1e400x"），不是合法的字面量。
  // 格式判断优先于范围判断：'1e400x' 里真正的问题是那个 'x'。
  if (end != text.c_str() + text.size()) return NumberStatus::kInvalid;

  // 上溢：strtod 返回无穷大。
  if (std::isinf(value)) return NumberStatus::kOutOfRange;
  // 下溢到 0：值本身合法，但精度已经丢失。
  // 注意不能靠 errno 判定下溢：glibc 对 subnormal 结果也会置 ERANGE，
  // 而 subnormal（如 "5e-324"）是完全可表示的合法值。
  if (value == 0.0 && !IsZeroLiteral(text)) return NumberStatus::kOutOfRange;

  out = value;
  return NumberStatus::kOk;
}

NumberStatus ParseBool(std::string_view input, bool& out) {
  const std::string lowered = to_lower(trim(input));

  if (lowered == "true" || lowered == "1" || lowered == "yes" ||
      lowered == "on") {
    out = true;
    return NumberStatus::kOk;
  }
  if (lowered == "false" || lowered == "0" || lowered == "no" ||
      lowered == "off") {
    out = false;
    return NumberStatus::kOk;
  }
  return NumberStatus::kInvalid;
}

// 统一的异常出口：把定位信息、类型名和原始串拼进消息里。
[[noreturn]] void ThrowNumberError(NumberStatus status,
                                   std::string_view context,
                                   std::string_view type_name,
                                   std::string_view input) {
  std::string message;
  if (!context.empty()) {
    message.append(context);
    message.append(": ");
  }
  if (status == NumberStatus::kOutOfRange) {
    message.append("value out of range for ");
    message.append(type_name);
    message.append(": '");
  } else {
    message.append("invalid ");
    message.append(type_name);
    message.append(" value: '");
  }
  message.append(input);
  message.append("'");

  if (status == NumberStatus::kOutOfRange) {
    throw std::out_of_range(message);
  }
  throw std::invalid_argument(message);
}

}  // namespace

std::vector<std::string_view> split(std::string_view input,
                                    std::string_view delimiter) {
  if (delimiter.empty()) {
    throw std::invalid_argument("split: delimiter must not be empty");
  }

  std::vector<std::string_view> parts;
  size_t start = 0;
  while (true) {
    const size_t position = input.find(delimiter, start);
    if (position == std::string_view::npos) {
      parts.push_back(input.substr(start));
      break;
    }
    parts.push_back(input.substr(start, position - start));
    start = position + delimiter.size();
  }
  return parts;
}

std::string to_lower(std::string_view input) {
  std::string result(input);
  for (char& c : result) c = detail::to_lower_ascii(c);
  return result;
}

std::string to_upper(std::string_view input) {
  std::string result(input);
  for (char& c : result) c = detail::to_upper_ascii(c);
  return result;
}

std::string replace_all(std::string_view input, std::string_view from,
                        std::string_view to) {
  if (from.empty()) {
    throw std::invalid_argument("replace_all: 'from' must not be empty");
  }

  // 先数一遍出现次数，按最终长度预留空间：否则替换次数一多就要反复
  // 扩容搬迁。
  size_t occurrences = 0;
  for (size_t position = input.find(from); position != std::string_view::npos;
       position = input.find(from, position + from.size())) {
    ++occurrences;
  }
  std::string result;
  result.reserve(input.size() + occurrences * to.size());

  size_t start = 0;
  while (true) {
    const size_t position = input.find(from, start);
    if (position == std::string_view::npos) break;
    result.append(input.substr(start, position - start));
    result.append(to);
    start = position + from.size();
  }
  result.append(input.substr(start));
  return result;
}

std::string replace_first(std::string_view input, std::string_view from,
                          std::string_view to) {
  if (from.empty()) {
    throw std::invalid_argument("replace_first: 'from' must not be empty");
  }

  const size_t position = input.find(from);
  if (position == std::string_view::npos) return std::string(input);

  std::string result;
  result.reserve(input.size() - from.size() + to.size());
  result.append(input.substr(0, position));
  result.append(to);
  result.append(input.substr(position + from.size()));
  return result;
}

int64_t to_int64(std::string_view input, std::string_view context) {
  int64_t value = 0;
  const NumberStatus status = ParseInt64(input, value);
  if (status != NumberStatus::kOk) {
    ThrowNumberError(status, context, "int64", input);
  }
  return value;
}

uint64_t to_uint64(std::string_view input, std::string_view context) {
  uint64_t value = 0;
  const NumberStatus status = ParseUint64(input, value);
  if (status != NumberStatus::kOk) {
    ThrowNumberError(status, context, "uint64", input);
  }
  return value;
}

double to_double(std::string_view input, std::string_view context) {
  double value = 0;
  const NumberStatus status = ParseDouble(input, value);
  if (status != NumberStatus::kOk) {
    ThrowNumberError(status, context, "double", input);
  }
  return value;
}

bool to_bool(std::string_view input, std::string_view context) {
  bool value = false;
  const NumberStatus status = ParseBool(input, value);
  if (status != NumberStatus::kOk) {
    ThrowNumberError(status, context, "bool", input);
  }
  return value;
}

std::optional<int64_t> try_to_int64(std::string_view input) noexcept {
  int64_t value = 0;
  if (ParseInt64(input, value) != NumberStatus::kOk) return std::nullopt;
  return value;
}

std::optional<uint64_t> try_to_uint64(std::string_view input) noexcept {
  uint64_t value = 0;
  if (ParseUint64(input, value) != NumberStatus::kOk) return std::nullopt;
  return value;
}

std::optional<double> try_to_double(std::string_view input) {
  double value = 0;
  if (ParseDouble(input, value) != NumberStatus::kOk) return std::nullopt;
  return value;
}

std::optional<bool> try_to_bool(std::string_view input) {
  bool value = false;
  if (ParseBool(input, value) != NumberStatus::kOk) return std::nullopt;
  return value;
}

}  // namespace common::util
