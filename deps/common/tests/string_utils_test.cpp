#include "utilities/string_utils.h"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

using common::util::ends_with;
using common::util::iends_with;
using common::util::iequals;
using common::util::istarts_with;
using common::util::join;
using common::util::ltrim;
using common::util::replace_all;
using common::util::replace_first;
using common::util::rtrim;
using common::util::split;
using common::util::starts_with;
using common::util::to_bool;
using common::util::to_double;
using common::util::to_int64;
using common::util::to_lower;
using common::util::to_uint64;
using common::util::to_upper;
using common::util::trim;
using common::util::try_to_bool;
using common::util::try_to_double;
using common::util::try_to_int64;
using common::util::try_to_uint64;

// ==================== 裁剪 ====================

TEST(StringUtilsTest, TrimsSurroundingWhitespace) {
  EXPECT_EQ(trim("  hello  "), "hello");
  EXPECT_EQ(trim("\t\n hello \r\n"), "hello");
  EXPECT_EQ(trim("hello"), "hello");
  EXPECT_EQ(trim("   "), "");
  EXPECT_EQ(trim(""), "");
}

TEST(StringUtilsTest, TrimsOnlyAsciiWhitespace) {
  // 垂直制表符和换页符也属于 ASCII 空白，但不是 isspace 在某些 locale
  // 下的行为，必须显式覆盖。
  EXPECT_EQ(trim("\v\f x \f\v"), "x");
  // 非 ASCII 的空白字符（这里是全角空格）不在裁剪范围内。
  EXPECT_EQ(trim("\xe3\x80\x80 x"), "\xe3\x80\x80 x");
}

TEST(StringUtilsTest, TrimsOnlyOneSide) {
  EXPECT_EQ(ltrim("  hello  "), "hello  ");
  EXPECT_EQ(rtrim("  hello  "), "  hello");
  EXPECT_EQ(ltrim("   "), "");
  EXPECT_EQ(rtrim("   "), "");
  EXPECT_EQ(ltrim(""), "");
  EXPECT_EQ(rtrim(""), "");
}

TEST(StringUtilsTest, TrimReturnsViewIntoInput) {
  // 裁剪不拷贝：返回的视图必须仍然指向原串的缓冲区。
  const std::string input = "  hello  ";
  const std::string_view trimmed = trim(input);
  EXPECT_EQ(input.data() + 2, trimmed.data());
  EXPECT_EQ(5u, trimmed.size());
}

// ==================== 切分 ====================

TEST(StringUtilsTest, SplitsOnDelimiter) {
  const std::vector<std::string_view> parts = split("a,b,c", ",");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0], "a");
  EXPECT_EQ(parts[1], "b");
  EXPECT_EQ(parts[2], "c");
}

TEST(StringUtilsTest, SplitKeepsEmptyFields) {
  // 连续分隔符不能被静默丢弃：字段位置本身就是信息。
  const std::vector<std::string_view> parts = split("a,,b", ",");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0], "a");
  EXPECT_EQ(parts[1], "");
  EXPECT_EQ(parts[2], "b");

  const std::vector<std::string_view> trailing = split("a,", ",");
  ASSERT_EQ(trailing.size(), 2u);
  EXPECT_EQ(trailing[1], "");
}

TEST(StringUtilsTest, SplitEmptyInputYieldsOneEmptyField) {
  const std::vector<std::string_view> parts = split("", ",");
  ASSERT_EQ(parts.size(), 1u);
  EXPECT_EQ(parts[0], "");
}

TEST(StringUtilsTest, SplitsOnMultiCharacterDelimiter) {
  const std::vector<std::string_view> parts = split("a::b::c", "::");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0], "a");
  EXPECT_EQ(parts[1], "b");
  EXPECT_EQ(parts[2], "c");
}

TEST(StringUtilsTest, SplitWithoutDelimiterReturnsWholeInput) {
  const std::vector<std::string_view> parts = split("abc", ",");
  ASSERT_EQ(parts.size(), 1u);
  EXPECT_EQ(parts[0], "abc");
}

TEST(StringUtilsTest, SplitThrowsOnEmptyDelimiter) {
  EXPECT_THROW((void)split("abc", ""), std::invalid_argument);
}

// ==================== 拼接 ====================

TEST(StringUtilsTest, JoinsStringViews) {
  const std::vector<std::string_view> parts = {"a", "b", "c"};
  EXPECT_EQ(join(parts, ","), "a,b,c");
}

TEST(StringUtilsTest, JoinsStringsAndCharPointers) {
  const std::vector<std::string> parts = {"a", "b"};
  EXPECT_EQ(join(parts, "-"), "a-b");

  const std::vector<const char*> raw = {"x", "y"};
  EXPECT_EQ(join(raw, "/"), "x/y");
}

TEST(StringUtilsTest, JoinsEmptyRange) {
  const std::vector<std::string_view> parts;
  EXPECT_EQ(join(parts, ","), "");
}

TEST(StringUtilsTest, JoinsSingleElementWithoutDelimiter) {
  const std::vector<std::string_view> parts = {"only"};
  EXPECT_EQ(join(parts, ","), "only");
}

// ==================== 大小写 ====================

TEST(StringUtilsTest, ConvertsAsciiCase) {
  EXPECT_EQ(to_lower("Hello, World!"), "hello, world!");
  EXPECT_EQ(to_upper("Hello, World!"), "HELLO, WORLD!");
  EXPECT_EQ(to_lower(""), "");
  EXPECT_EQ(to_upper("ABC-123_x"), "ABC-123_X");
}

TEST(StringUtilsTest, CaseConversionLeavesNonAsciiBytesIntact) {
  // 按字节处理，多字节字符的每个字节都不在 ASCII 范围内，原样返回。
  const std::string input = "\xe4\xb8\xad\xe6\x96\x87";
  EXPECT_EQ(to_upper(input), input);
  EXPECT_EQ(to_lower(input), input);
}

TEST(StringUtilsTest, ComparesCaseInsensitively) {
  EXPECT_TRUE(iequals("Hello", "hello"));
  EXPECT_TRUE(iequals("", ""));
  EXPECT_FALSE(iequals("Hello", "hell"));
  EXPECT_FALSE(iequals("Hello", "hello world"));
}

// ==================== 前后缀 ====================

TEST(StringUtilsTest, ChecksPrefixAndSuffix) {
  EXPECT_TRUE(starts_with("hello world", "hello"));
  EXPECT_TRUE(starts_with("hello", ""));
  EXPECT_FALSE(starts_with("hello", "hello world"));
  EXPECT_FALSE(starts_with("hello", "Hello"));

  EXPECT_TRUE(ends_with("hello world", "world"));
  EXPECT_TRUE(ends_with("hello", ""));
  EXPECT_FALSE(ends_with("hello", "hello world"));
  EXPECT_FALSE(ends_with("hello", "Hello"));
}

TEST(StringUtilsTest, ChecksPrefixAndSuffixCaseInsensitively) {
  EXPECT_TRUE(istarts_with("Hello world", "HELLO"));
  EXPECT_TRUE(istarts_with("Hello world", "hello world"));
  EXPECT_FALSE(istarts_with("Hello world", "hello there"));
  EXPECT_TRUE(iends_with("Hello world", "WORLD"));
  EXPECT_TRUE(iends_with("Hello world", "hello world"));
  EXPECT_FALSE(iends_with("Hello world", "there world"));
}

// ==================== 替换 ====================

TEST(StringUtilsTest, ReplacesAllOccurrences) {
  EXPECT_EQ(replace_all("a-b-c", "-", "+"), "a+b+c");
  EXPECT_EQ(replace_all("aaa", "a", "aa"), "aaaaaa");
  EXPECT_EQ(replace_all("abc", "x", "y"), "abc");
  EXPECT_EQ(replace_all("", "a", "b"), "");
}

TEST(StringUtilsTest, ReplacesFirstOccurrenceOnly) {
  EXPECT_EQ(replace_first("a-b-c", "-", "+"), "a+b-c");
  EXPECT_EQ(replace_first("abc", "x", "y"), "abc");
}

TEST(StringUtilsTest, ReplaceThrowsOnEmptyPattern) {
  EXPECT_THROW((void)replace_all("abc", "", "x"), std::invalid_argument);
  EXPECT_THROW((void)replace_first("abc", "", "x"), std::invalid_argument);
}

// ==================== 整数解析 ====================

TEST(StringUtilsTest, ParsesIntegers) {
  EXPECT_EQ(to_int64("42"), 42);
  EXPECT_EQ(to_int64("-42"), -42);
  EXPECT_EQ(to_int64("+42"), 42);
  EXPECT_EQ(to_int64("  42  "), 42);
  EXPECT_EQ(to_int64("0"), 0);
  EXPECT_EQ(to_uint64("18446744073709551615"), 18446744073709551615ULL);
}

TEST(StringUtilsTest, RejectsMalformedIntegers) {
  EXPECT_THROW((void)to_int64(""), std::invalid_argument);
  EXPECT_THROW((void)to_int64("12x"), std::invalid_argument);
  EXPECT_THROW((void)to_int64("1 2"), std::invalid_argument);
  EXPECT_THROW((void)to_int64("0x10"), std::invalid_argument);
  EXPECT_THROW((void)to_int64("1.5"), std::invalid_argument);
  EXPECT_THROW((void)to_int64("+"), std::invalid_argument);
}

TEST(StringUtilsTest, DistinguishesOutOfRangeFromMalformed) {
  // 越界和格式非法是两回事：前者数据本身合法，只是放不进目标类型。
  EXPECT_THROW((void)to_int64("99999999999999999999"), std::out_of_range);
  EXPECT_THROW((void)to_uint64("99999999999999999999"), std::out_of_range);
  EXPECT_THROW((void)to_int64("abc"), std::invalid_argument);
}

TEST(StringUtilsTest, RejectsNegativeForUnsigned) {
  // 无符号类型不能把 "-1" 回绕成 2^64-1。
  EXPECT_THROW((void)to_uint64("-1"), std::invalid_argument);
  EXPECT_THROW((void)to_uint64("-0"), std::invalid_argument);
}

TEST(StringUtilsTest, ErrorMessageCarriesContext) {
  try {
    (void)to_int64("abc", "server.listen.port");
    FAIL() << "expected std::invalid_argument";
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("server.listen.port"), std::string::npos) << message;
    EXPECT_NE(message.find("invalid int64"), std::string::npos) << message;
    EXPECT_NE(message.find("abc"), std::string::npos) << message;
  }
}

TEST(StringUtilsTest, TryToIntReturnsNulloptOnFailure) {
  EXPECT_EQ(try_to_int64("42").value(), 42);
  EXPECT_FALSE(try_to_int64("abc").has_value());
  EXPECT_FALSE(try_to_int64("").has_value());
  EXPECT_FALSE(try_to_int64("99999999999999999999").has_value());
  EXPECT_EQ(try_to_uint64("42").value(), 42u);
  EXPECT_FALSE(try_to_uint64("-1").has_value());
}

// ==================== 浮点解析 ====================

TEST(StringUtilsTest, ParsesDoubles) {
  EXPECT_DOUBLE_EQ(to_double("1.5"), 1.5);
  EXPECT_DOUBLE_EQ(to_double("-0.25"), -0.25);
  EXPECT_DOUBLE_EQ(to_double("  2e3  "), 2000.0);
  EXPECT_DOUBLE_EQ(to_double("42"), 42.0);
  EXPECT_DOUBLE_EQ(to_double(".5"), 0.5);
}

TEST(StringUtilsTest, RejectsMalformedDoubles) {
  EXPECT_THROW((void)to_double(""), std::invalid_argument);
  EXPECT_THROW((void)to_double("1.5abc"), std::invalid_argument);
  EXPECT_THROW((void)to_double("1,5"), std::invalid_argument);
  EXPECT_THROW((void)to_double("abc"), std::invalid_argument);
  // inf/nan 不是有限值，按格式非法处理。
  EXPECT_THROW((void)to_double("inf"), std::invalid_argument);
  EXPECT_THROW((void)to_double("nan"), std::invalid_argument);
}

TEST(StringUtilsTest, ReportsDoubleOverflowAsOutOfRange) {
  // 溢出是数值越界，和"格式写错"区分开。
  EXPECT_THROW((void)to_double("1e400"), std::out_of_range);
  EXPECT_THROW((void)to_double("1e-400"), std::out_of_range);
  EXPECT_FALSE(try_to_double("1e400").has_value());
}

TEST(StringUtilsTest, AcceptsSubnormalDoubles) {
  // subnormal 是完全可表示的数，只是精度低于最小正规数，不能当越界。
  // 这类值靠 errno 判定会误报：glibc 对 subnormal 结果同样置 ERANGE。
  EXPECT_DOUBLE_EQ(to_double("5e-324"),
                   std::numeric_limits<double>::denorm_min());
  EXPECT_DOUBLE_EQ(to_double("2.2250738585072014e-308"),
                   std::numeric_limits<double>::min());
  EXPECT_GT(to_double("1e-320"), 0.0);
  EXPECT_TRUE(try_to_double("5e-324").has_value());
}

TEST(StringUtilsTest, RejectsHexadecimalFloatLiterals) {
  // strtod 按 C99 语义会接受十六进制浮点，这里不接受。
  EXPECT_THROW((void)to_double("0x1p3"), std::invalid_argument);
  EXPECT_THROW((void)to_double("0X1P3"), std::invalid_argument);
}

TEST(StringUtilsTest, MalformedInputTakesPriorityOverRange) {
  // 串尾多出来的字符才是真正的问题，报格式非法比报越界更有指向性。
  EXPECT_THROW((void)to_int64("99999999999999999999x"), std::invalid_argument);
  EXPECT_THROW((void)to_uint64("99999999999999999999x"), std::invalid_argument);
  EXPECT_THROW((void)to_double("1e400x"), std::invalid_argument);
  // 纯越界（没有多余字符）仍然是越界。
  EXPECT_THROW((void)to_int64("99999999999999999999"), std::out_of_range);
}

TEST(StringUtilsTest, ErrorMessageReportsOutOfRange) {
  try {
    (void)to_double("1e400", "backend.gpu.memory_fraction");
    FAIL() << "expected std::out_of_range";
  } catch (const std::out_of_range& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("backend.gpu.memory_fraction"), std::string::npos)
        << message;
    EXPECT_NE(message.find("out of range"), std::string::npos) << message;
    EXPECT_NE(message.find("1e400"), std::string::npos) << message;
  }
}

TEST(StringUtilsTest, ErrorMessageReportsMalformedDouble) {
  try {
    (void)to_double("1.5abc", "log.level");
    FAIL() << "expected std::invalid_argument";
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("log.level"), std::string::npos) << message;
    EXPECT_NE(message.find("invalid double"), std::string::npos) << message;
    EXPECT_NE(message.find("1.5abc"), std::string::npos) << message;
  }
}

TEST(StringUtilsTest, ErrorMessageReportsMalformedInteger) {
  try {
    (void)to_uint64("-1", "storage.quota_bytes");
    FAIL() << "expected std::invalid_argument";
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("storage.quota_bytes"), std::string::npos)
        << message;
    EXPECT_NE(message.find("invalid uint64"), std::string::npos) << message;
    EXPECT_NE(message.find("-1"), std::string::npos) << message;
  }
}

// ==================== 布尔解析 ====================

TEST(StringUtilsTest, ParsesBooleans) {
  EXPECT_TRUE(to_bool("true"));
  EXPECT_TRUE(to_bool("TRUE"));
  EXPECT_TRUE(to_bool("1"));
  EXPECT_TRUE(to_bool("yes"));
  EXPECT_TRUE(to_bool("On"));
  EXPECT_FALSE(to_bool("false"));
  EXPECT_FALSE(to_bool("0"));
  EXPECT_FALSE(to_bool("no"));
  EXPECT_FALSE(to_bool("OFF"));
  // 前后空白按其它解析函数一样的规则裁掉。
  EXPECT_TRUE(to_bool(" true "));
}

TEST(StringUtilsTest, RejectsMalformedBooleans) {
  EXPECT_THROW((void)to_bool(""), std::invalid_argument);
  EXPECT_THROW((void)to_bool("maybe"), std::invalid_argument);
  EXPECT_THROW((void)to_bool("2"), std::invalid_argument);
  EXPECT_FALSE(try_to_bool("maybe").has_value());
  EXPECT_TRUE(try_to_bool("true").value());
}
