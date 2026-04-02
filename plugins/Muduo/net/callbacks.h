#pragma once

#include <functional>

namespace Muduo {
using TimerCallback = std::function<void()>;
}