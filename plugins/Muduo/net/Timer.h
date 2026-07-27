#pragma once
#include <base/timestamp.h>

#include <atomic>
#include <boost/core/noncopyable.hpp>

#include "callbacks.h"

namespace Muduo {
class Timer : boost::noncopyable {
 public:
  Timer(TimerCallback  cb, Timestamp when, double interval);
  ~Timer() = default;
  void run() const;
  Timestamp expiration();
  bool repeat() const;
  int64_t sequence() const;
  void restart(Timestamp now);
  static int64_t numCreated();

 protected:
  const TimerCallback callback_;
  Timestamp expiration_;
  const double interval_;
  const bool repeat_;
  const int64_t sequence_;

  static std::atomic_int64_t numCreated_;
};
}  // namespace Muduo