#pragma once

#include <boost/operators.hpp>
#include <string>

class Timestamp : boost::totally_ordered1<Timestamp> {
 public:
  Timestamp();

  explicit Timestamp(int64_t microSecondsSinceEpoch);

  virtual ~Timestamp();

  void swap(Timestamp& that);

  std::string toString() const;

  std::string toFormattedString() const;

  bool valid() const;

  int64_t microSecondsSinceEpoch() const;

  time_t secondsSicneEpoch() const;

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
  static constexpr int kMicroSecondsPerSecond = 1000 * 1000;  ///< 一秒对应的微秒数
 protected:
  int64_t m_microSecondsSinceEpoch;  ///< 微秒数
};

inline bool operator<(const Timestamp& _left, const Timestamp& _right) {
  return _left.microSecondsSinceEpoch() < _right.microSecondsSinceEpoch();
}

inline bool operator==(const Timestamp& _left, const Timestamp& _right) {
  return _left.microSecondsSinceEpoch() == _right.microSecondsSinceEpoch();
}

/**
 * @brief 返回秒数
 * @param high
 * @param low
 * @return
 */
inline double timeDifference(const Timestamp& high, const Timestamp& low) {
  int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

/**
 * @brief 给timestamp加上seconds秒
 * @param timestamp
 * @param seconds
 * @return
 */
inline Timestamp addTime(const Timestamp& timestamp, double seconds) {
  auto delta =
      static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}