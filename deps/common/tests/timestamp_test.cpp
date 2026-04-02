#include "base/Timestamp.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

class TimestampTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// ==================== 构造函数测试 ====================
TEST(TimestampTest, DefaultConstructor) {
  Timestamp ts;
  EXPECT_FALSE(ts.valid());
  EXPECT_EQ(ts.microSecondsSinceEpoch(), 0);
}

TEST(TimestampTest, ParameterizedConstructor) {
  Timestamp ts(1234567890123);  // 1234567.890123 秒

  EXPECT_TRUE(ts.valid());
  EXPECT_EQ(ts.microSecondsSinceEpoch(), 1234567890123);

  time_t seconds = ts.secondsSicneEpoch();
  EXPECT_EQ(seconds, 1234567);
}

// ==================== now() 测试 ====================
TEST(TimestampTest, NowReturnsValidTimestamp) {
  Timestamp ts = Timestamp::now();

  EXPECT_TRUE(ts.valid());
  EXPECT_GT(ts.microSecondsSinceEpoch(), 0);

  // 验证是当前时间（允许 1 秒误差）
  auto now = std::chrono::system_clock::now();
  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch())
                    .count();

  int64_t diff = std::abs(now_us - ts.microSecondsSinceEpoch());
  EXPECT_LT(diff, Timestamp::kMicroSecondsPerSecond);  // 相差小于 1 秒
}

TEST(TimestampTest, NowIsMonotonic) {
  Timestamp ts1 = Timestamp::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  Timestamp ts2 = Timestamp::now();

  EXPECT_LT(ts1, ts2);
  EXPECT_GT(ts2.microSecondsSinceEpoch() - ts1.microSecondsSinceEpoch(), 0);
}

// ==================== invalid() 测试 ====================
TEST(TimestampTest, InvalidReturnsInvalidTimestamp) {
  Timestamp invalid_ts = Timestamp::invalid();
  EXPECT_FALSE(invalid_ts.valid());
  EXPECT_EQ(invalid_ts.microSecondsSinceEpoch(), 0);
}

// ==================== valid() 测试 ====================
TEST(TimestampTest, ValidFunction) {
  EXPECT_FALSE(Timestamp().valid());              // 默认构造
  EXPECT_FALSE(Timestamp::invalid().valid());     // invalid()
  EXPECT_TRUE(Timestamp(1).valid());              // 正数
  EXPECT_TRUE(!Timestamp(0).valid());             // 零（Epoch）
  EXPECT_TRUE(!Timestamp(-1).valid());            // 负数（1970年前）
  EXPECT_TRUE(Timestamp(9999999999999).valid());  // 极大值
}

// ==================== swap() 测试 ====================
TEST(TimestampTest, SwapFunction) {
  Timestamp ts1(1000000);
  Timestamp ts2(2000000);

  ts1.swap(ts2);

  EXPECT_EQ(ts1.microSecondsSinceEpoch(), 2000000);
  EXPECT_EQ(ts2.microSecondsSinceEpoch(), 1000000);
}

TEST(TimestampTest, SelfSwap) {
  Timestamp ts(1234567);
  ts.swap(ts);
  EXPECT_EQ(ts.microSecondsSinceEpoch(), 1234567);
}

// ==================== toString() 测试 ====================
TEST(TimestampTest, ToStringFormat) {
  // 1.5 秒 = 1500000 微秒
  Timestamp ts(1500000);
  std::string str = ts.toString();
  EXPECT_EQ(str, "1.500000");

  // Epoch
  Timestamp ts0(0);
  EXPECT_EQ(ts0.toString(), "0.000000");

  // 带微秒
  Timestamp ts_us(1234567890123);
  EXPECT_EQ(ts_us.toString(), "1234567.890123");
}

TEST(TimestampTest, ToStringPadding) {
  // 1 微秒 = 0.000001 秒
  Timestamp ts(1);
  EXPECT_EQ(ts.toString(), "0.000001");

  // 100 微秒 = 0.000100 秒
  Timestamp ts2(100);
  EXPECT_EQ(ts2.toString(), "0.000100");
}

// ==================== toFormattedString() 测试 ====================
TEST(TimestampTest, ToFormattedStringFormat) {
  // Epoch 本身
  Timestamp ts0(0);
  std::string formatted = ts0.toFormattedString();
  // 注意：取决于 gmtime_r 的时区，这里假设 UTC
  // 1970-01-01 00:00:00.000000
  EXPECT_EQ(formatted.substr(0, 4), "1970");
  EXPECT_EQ(formatted.substr(9, 8), "00:00:00");
}

TEST(TimestampTest, ToFormattedStringStructure) {
  Timestamp ts = Timestamp::now();
  std::string formatted = ts.toFormattedString();

  // 格式：YYYYMMDD HH:MM:SS.uuuuuu
  // 长度应该是：8 + 1 + 8 + 1 + 6 = 24？不对，看具体实现
  // 实际是：%4d%02d%02d %02d:%02d:%02d.%06d = 4+2+2+1+2+1+2+1+2+1+6 = 24
  EXPECT_EQ(formatted.length(), 24);
  EXPECT_EQ(formatted[8], ' ');   // 日期和时间之间的空格
  EXPECT_EQ(formatted[17], '.');  // 秒和微秒之间的小数点
}

// ==================== 运算符测试 ====================
TEST(TimestampTest, LessThanOperator) {
  Timestamp ts1(100);
  Timestamp ts2(200);
  Timestamp ts3(100);

  EXPECT_TRUE(ts1 < ts2);
  EXPECT_FALSE(ts2 < ts1);
  EXPECT_FALSE(ts1 < ts3);  // 相等
}

TEST(TimestampTest, EqualOperator) {
  Timestamp ts1(100);
  Timestamp ts2(100);
  Timestamp ts3(200);

  EXPECT_TRUE(ts1 == ts2);
  EXPECT_FALSE(ts1 == ts3);
}

TEST(TimestampTest, ComparisonOperators) {
  Timestamp ts1(100);
  Timestamp ts2(200);

  EXPECT_TRUE(ts1 < ts2);
  EXPECT_TRUE(ts2 > ts1);  // 隐式通过 operator< 推导
  EXPECT_TRUE(ts1 <= ts2);
  EXPECT_TRUE(ts2 >= ts1);
  EXPECT_TRUE(ts1 <= ts1);  // 等于自身
  EXPECT_TRUE(ts1 >= ts1);
}

// ==================== timeDifference() 测试 ====================
TEST(TimestampUtility, TimeDifference) {
  Timestamp high(3000000);  // 3.0 秒
  Timestamp low(1000000);   // 1.0 秒

  double diff = timeDifference(high, low);
  EXPECT_DOUBLE_EQ(diff, 2.0);

  // 反过来
  double diff_reverse = timeDifference(low, high);
  EXPECT_DOUBLE_EQ(diff_reverse, -2.0);

  // 相同时间
  double diff_same = timeDifference(high, high);
  EXPECT_DOUBLE_EQ(diff_same, 0.0);
}

TEST(TimestampUtility, TimeDifferenceFractional) {
  Timestamp high(1500000);  // 1.5 秒
  Timestamp low(500000);    // 0.5 秒

  double diff = timeDifference(high, low);
  EXPECT_DOUBLE_EQ(diff, 1.0);
}

// ==================== addTime() 测试 ====================
TEST(TimestampUtility, AddTime) {
  Timestamp base(1000000);  // 1.0 秒

  Timestamp result = addTime(base, 0.5);
  EXPECT_EQ(result.microSecondsSinceEpoch(), 1500000);  // 1.5 秒

  Timestamp result2 = addTime(base, 2.0);
  EXPECT_EQ(result2.microSecondsSinceEpoch(), 3000000);  // 3.0 秒
}

TEST(TimestampUtility, AddTimeNegative) {
  Timestamp base(1000000);  // 1.0 秒

  Timestamp result = addTime(base, -0.5);
  EXPECT_EQ(result.microSecondsSinceEpoch(), 500000);  // 0.5 秒
}

TEST(TimestampUtility, AddTimeZero) {
  Timestamp base(1000000);
  Timestamp result = addTime(base, 0.0);
  EXPECT_EQ(result.microSecondsSinceEpoch(), 1000000);
}

TEST(TimestampUtility, AddTimePrecision) {
  Timestamp base(0);

  // 加 0.123456 秒
  Timestamp result = addTime(base, 0.123456);
  EXPECT_EQ(result.microSecondsSinceEpoch(), 123456);
}

// ==================== 边界测试 ====================
TEST(TimestampBoundary, LargeValue) {
  int64_t large = 3250368000000000LL;  // 约 2073 年，微秒
  Timestamp ts(large);
  EXPECT_TRUE(ts.valid());
  EXPECT_EQ(ts.microSecondsSinceEpoch(), large);
}

TEST(TimestampBoundary, NegativeValue) {
  Timestamp ts(-1000000);  // 1969-12-31 23:59:59
  EXPECT_TRUE(!ts.valid());
  EXPECT_EQ(ts.microSecondsSinceEpoch(), -1000000);
}

TEST(TimestampBoundary, MicroSecondsPerSecond) {
  EXPECT_EQ(Timestamp::kMicroSecondsPerSecond, 1000000);
}

// ==================== 性能/压力测试（可选） ====================
TEST(TimestampStress, CreateManyTimestamps) {
  const int count = 10000;
  std::vector<Timestamp> timestamps;
  timestamps.reserve(count);

  for (int i = 0; i < count; ++i) {
    timestamps.push_back(Timestamp::now());
  }
  for (int i = 1; i < count; ++i) {
    EXPECT_LE(timestamps[i - 1], timestamps[i]);
  }
}