#pragma once
#include <base/timestamp.h>
#include <utilities/noncopyable.h>

#include <atomic>

#include "callbacks.h"

namespace Muduo {
class Timer : noncopyable {
 public:
  Timer(TimerCallback cb, const common::base::Timestamp& when, double interval);
  ~Timer() = default;
  void run() const;
  common::base::Timestamp expiration();
  bool repeat() const;
  int64_t sequence() const;
  void restart(common::base::Timestamp now);
  static int64_t numCreated();

 protected:
  const TimerCallback callback_;
  common::base::Timestamp expiration_;
  const double interval_;
  const bool repeat_;
  const int64_t sequence_;

  static std::atomic_int64_t numCreated_;
};
}  // namespace Muduo