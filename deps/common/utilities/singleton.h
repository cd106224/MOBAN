#pragma once

#include "noncopyable.h"

/**
 * @brief 单例模板类
 *
 * 通过 CRTP 模式实现单例, 线程安全的懒汉式初始化（C++11 magic static）。
 *
 * @example
 * ```cpp
 * class Logger : public Singleton<Logger> {
 *   friend Singleton<Logger>;
 *  private:
 *   Logger() = default;
 * };
 *
 * Logger& logger = Logger::instance();
 * ```
 */
template <typename T>
class Singleton : private noncopyable {
 public:
  static T& instance() {
    static T inst;
    return inst;
  }

 protected:
  Singleton() = default;
  virtual ~Singleton() = default;
};
