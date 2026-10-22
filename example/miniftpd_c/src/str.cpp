#include "str.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

/**
 * @brief 去除\r\n
 * @param str
 */
void str_trim_crlf(char* str) {
  size_t len = strlen(str);
  while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n')) {
    str[--len] = '\0';
  }
}

/**
 * @brief 字符串分割
 * @param str 待分割的字符串
 * @param c 分隔符
 * @return {分隔符之前的部分, 分隔符之后的部分}；没有分隔符时左边为空串
 */
std::pair<std::string, std::string> str_split(const std::string& str, char c) {
  const size_t pos = str.find(c);
  if (pos == std::string::npos) {
    return {std::string(), str};
  }
  return {str.substr(0, pos), str.substr(pos + 1)};
}

/**
 * @brief 判断是否全是空白字符
 * @param str
 * @return
 */
bool str_all_space(const char* str) {
  while (*str) {
    // char 是有符号的，遇到 >= 0x80 的字节（如 UTF-8）会得到负数，
    // 而 isspace 要求入参落在 [0,255] 或 EOF，否则是未定义行为
    if (!isspace(static_cast<unsigned char>(*str))) {
      return false;
    }
    ++str;
  }
  return true;
}

/**
 * @brief   字符串转换为大写格式
 * @param str
 */
void str_upper(char* str) {
  while (*str) {
    if (*str >= 'a' && *str <= 'z') {
      *str = static_cast<char>('A' - 'a' + *str);
    }
    ++str;
  }
}

/**
 * @brief 将字符串转换为long long`
 * @param str
 * @return
 */
long long str_to_longlong(const char* str) { return atoll(str); }

/**
 * @brief 将字符串(八进制)转换为无符号整形
 * @param str
 * @return
 */
unsigned int str_octal_to_uint(const char* str) {
  return std::stoul(str, nullptr, 8);
}
