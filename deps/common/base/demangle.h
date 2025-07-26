#pragma once

#include <memory>
#include <string>

/**
 * @brief 对 c++ 符号名称去混淆化
 *
 * 如果去混淆失败，返回原始名称并将 \p status 设置为非零
 */
std::string demangle(const char* name, int& status);

inline std::string demangle(const char* name) {
  int status = 0;
  return demangle(name, status);
}

// abi::__cxa_demangle 返回了一个 C 字符串, 该字符串需要使用 free() 进行删除
struct FreeingDeleter {
  template <typename PointerType>
  void operator()(PointerType ptr) {
    std::free(ptr);
  }
};

using DemangleResult = std::unique_ptr<char, FreeingDeleter>;

DemangleResult try_demangle(const char* name);