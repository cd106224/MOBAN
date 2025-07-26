#include "src/demangle.h"

#include <gtest/gtest.h>

namespace base {

TEST(DemangleTest, DemangleTest) {
  // 测试正常的C++符号名称解析
  int status = 0;
  std::string result = demangle("_Z3foov", status);
  EXPECT_EQ(status, 0);
  EXPECT_FALSE(result.empty());

  // 测试无效的符号名称
  result = demangle("invalid_symbol_name", status);
  EXPECT_EQ(result, "invalid_symbol_name");

  // 测试内联版本
  result = demangle("_Z3foov");
  EXPECT_FALSE(result.empty());
}

TEST(DemangleTest, TryDemangleTest) {
  // 测试有效的符号
  auto result = try_demangle("_Z3foov");
  EXPECT_TRUE(result);

  // 测试无效的符号
  result = try_demangle("invalid_symbol_name");
  EXPECT_FALSE(result);
}

}  // namespace base
