#pragma once

#include <iostream>

#include "di/host/driver_types.h"

namespace sys {
namespace detail {

inline devError_t notify_runtime_error(const char* msg) {
  std::cerr << msg << std::endl;
  std::abort();
  return devSuccess;
}

}  // namespace detail
}  // namespace sys

#define DEV_STRINGIFY_DETAIL(x) #x
#define DEV_STRINGIFY(x) DEV_STRINGIFY_DETAIL(x)

#define __RuntimeError(api)                       \
  sys::detail::notify_runtime_error(              \
      "Device runtime api detected at: " __FILE__ \
      ":" DEV_STRINGIFY(__LINE__) " : " DEV_STRINGIFY(api))
