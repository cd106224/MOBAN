#pragma once

#include "unistd.h"

namespace current_thread {
pid_t id() noexcept;  // 缓存的 tid
bool isMain() noexcept;
}  // namespace current_thread