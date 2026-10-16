#pragma once

#include <string>
#include <utility>

/**
 * @brief 去除\r\n
 * @param str
 */
void str_trim_crlf(char* str);

/**
 * @brief 按第一个分隔符 c 分割字符串，只切一刀
 * @param str 待分割的字符串
 * @param c 分隔符
 * @return {分隔符之前的部分, 分隔符之后的部分}；没有分隔符时左边为空串
 */
std::pair<std::string, std::string> str_split(const std::string& str, char c);

/**
 * @brief 判断是否全是空白字符
 * @param str
 * @return
 */
bool str_all_space(const char* str);

/**
 * @brief   字符串转换为大写格式
 * @param str
 */
void str_upper(char* str);

/**
 * @brief 将字符串转换为long long`
 * @param str
 * @return
 */
long long str_to_longlong(const char* str);

/**
 * @brief 将字符串(八进制)转换为无符号整形
 * @param str
 * @return
 */
unsigned int str_octal_to_uint(const char* str);
