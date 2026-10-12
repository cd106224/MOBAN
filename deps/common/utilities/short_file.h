#pragma once

// 递归找最后一个 / 或者 \

static constexpr const char* past_last_slash(const char* str,
                                             const char* last_pos) {
  return *str == '\0'                    ? last_pos
         : (*str == '/' || *str == '\\') ? past_last_slash(str + 1, str + 1)
                                         : past_last_slash(str + 1, last_pos);
}

static constexpr const char* past_last_slash(const char* str) {
  return past_last_slash(str, str);
}

// 重点：去掉 ({})，用函数直接返回，不需要GNU扩展
#define SHORT_FILE past_last_slash(__FILE__)

// 使用示例
// printf("%s:%d\n", SHORT_FILE, __LINE__);
