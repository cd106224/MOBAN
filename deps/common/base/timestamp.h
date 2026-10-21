#pragma once

#include <boost/operators.hpp>
#include <cstdint>
#include <ctime>
#include <string>

namespace common::base {

class Timestamp : boost::totally_ordered1<Timestamp> {
 public:
  Timestamp();

  explicit Timestamp(int64_t micro_seconds_since_epoch);

  virtual ~Timestamp();

  void swap(Timestamp& that);

  std::string to_string() const;

  std::string to_formatted_string() const;

  bool valid() const;

  int64_t micro_seconds_since_epoch() const;

  time_t seconds_since_epoch() const;

  /**
   * @brief 获取当前时间
   * @return
   */
  static Timestamp now();

  /**
   * @brief 获得无效值
   * @return
   */
  static Timestamp invalid();

 public:
  static constexpr int kMicroSecondsPerSecond =
      1000 * 1000;  ///< 一秒对应的微秒数
 protected:
  int64_t micro_seconds_since_epoch_;  ///< 微秒数
};

inline bool operator<(const Timestamp& _left, const Timestamp& _right) {
  return _left.micro_seconds_since_epoch() < _right.micro_seconds_since_epoch();
}

inline bool operator==(const Timestamp& _left, const Timestamp& _right) {
  return _left.micro_seconds_since_epoch() ==
         _right.micro_seconds_since_epoch();
}

/**
 * @brief 返回秒数
 * @param high
 * @param low
 * @return
 */
inline double time_difference(const Timestamp& high, const Timestamp& low) {
  int64_t diff =
      high.micro_seconds_since_epoch() - low.micro_seconds_since_epoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

/**
 * @brief 给timestamp加上seconds秒
 * @param timestamp
 * @param seconds
 * @return
 */
inline Timestamp add_time(const Timestamp& timestamp, double seconds) {
  auto delta =
      static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.micro_seconds_since_epoch() + delta);
}

}  // namespace common::base