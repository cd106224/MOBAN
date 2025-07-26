#include "demangle.h"

#include <cxxabi.h>

static DemangleResult try_demangle(const char* name, int& status) {
  return DemangleResult(abi::__cxa_demangle(name, nullptr, nullptr, &status));
}

DemangleResult try_demangle(const char* name) {
  int status = 0;
  return try_demangle(name, status);
}

std::string demangle(const char* name, int& status) {
  const auto result = try_demangle(name, status);
  if (result) {
    return std::string{result.get()};
  }

  return name;
}
