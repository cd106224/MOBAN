#pragma once

#include <cstdint>

namespace Muduo {
class Timer;

class TimerId {
 public:
  TimerId() : timer_(nullptr), sequenct_(0) {}

  TimerId(Timer* timer, int64_t seq) : timer_(timer), sequenct_(seq) {}

  ~TimerId() = default;

  friend class TimerQueue;

 protected:
  Timer* timer_;
  int64_t sequenct_;
};

}  // namespace Muduo