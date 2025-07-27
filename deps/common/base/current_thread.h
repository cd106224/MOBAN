#pragma once

#include <string>

namespace current_thread {
pid_t id() noexcept;  // 缓存的 tid
bool isMain() noexcept;
void set_thread_name(const std::string &name) noexcept;
std::string thread_name() noexcept;
}  // namespace current_thread