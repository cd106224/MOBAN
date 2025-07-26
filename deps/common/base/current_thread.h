#pragma once

#include "unistd.h"

namespace current_thread {
pid_t id() noexcept;  // 缓存的 tid
bool isMain() noexcept;
void set_thread_name(const char* name) noexcept;
const char* thread_name() noexcept;
}  // namespace current_thread