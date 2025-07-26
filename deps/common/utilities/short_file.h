#pragma once

using cstr = const char* const;

static constexpr cstr past_last_slash(cstr str, cstr last_slash)
{
    return *str == '\0'  ? last_slash
           : *str == '/' ? past_last_slash(str + 1, str + 1)
                         : past_last_slash(str + 1, last_slash);
}

static constexpr cstr past_last_slash(cstr str) { return past_last_slash(str, str); }

/**
 * @brief 获取当前文件名的宏
 *
 * 在做一些日志输出的工作时，想要获取当前文件名，而不是冗长的文件路径。
 * 我们希望能够在编译期而不是在运行期做这个事情，避免额外的性能消耗。
 *
 * 摘自https://www.cnblogs.com/shijiashuai/p/14410321.html
 */
#define __SHORT_FILE__                                  \
    ({                                                  \
        constexpr cstr sf__{past_last_slash(__FILE__)}; \
        sf__;                                           \
    })
