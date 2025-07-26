#pragma once

#include <utility>

/**
 * @brief 提供了与 go 中的 defer 语义类似功能的实现,
 * 以简化在简单场景下的资源清理, 使用者不必再额外自定义 RAII 对象
 *
 * @note 与 go 中不同的是, defer 函数在离开当前作用域时被调用, 而非当前函数
 *
 * 对外提供了两种使用 defer 接口的方式:
 *
 * 1. 使用宏 `DEFER`, 改宏接受函数体作为其参数, 并构造一个 lambda
 * 2. 使用 `dt::make_defer` 声明一个本地对象，其可接受任何可调用对象作为参数,
 *    因此除 lambda 外还可使用 std::function 或 标准函数指针
 *
 * @example
 * ```c++
 * void do_file_io(const std::string& file) {
 *     auto fp1 = std::fopen(filename.c_str(), "r");
 *
 *     // 方法一
 *     DEFER({ std::fclose(fp1); });
 *     // 做些 IO 工作, 此函数返回时会关闭 fp1
 *
 *     auto fp2 = std::fopen(filename.c_str(), "r");
 *
 *     // 方法二
 *     const auto& defer_close = dt::make_defer([&fp2]() { std::fclose(fp2); });
 *     // 做些 IO 工作, 此函数返回时会关闭 fp2
 * }
 * ```
 */

template <typename F>
class defer {
 public:
  // 删除 拷贝/移动 构造函数, 否则会导致 cleanup function 被调用多次
  defer(const defer&) = delete;
  defer(defer&&) = delete;
  defer& operator=(const defer&) = delete;
  defer& operator=(defer&&) = delete;

  // construct the object from the given callable
  explicit defer(F&& f) : cleanup_function_(std::forward<F>(f)) {}

  // when the object goes out of scope call the cleanup function
  ~defer() { cleanup_function_(); }

 private:
  F cleanup_function_;
};

template <typename Function>
detail::defer<Function> make_defer(Function&& f) {
  return detail::defer{std::forward<Function>(f)};
}

#define CONCAT(x, y) x##y
#define DEFER_JOIN(x, y) CONCAT(x, y)
#define DEFER_UNIQUE_NAME(x) DEFER_JOIN(x, __LINE__)

#define DEFER(lambda__)                                           \
  [[maybe_unused]] const auto& DEFER_UNIQUE_NAME(_defer_object) = \
      dt::make_defer([&]() __attribute__((always_inline)) lambda__)
