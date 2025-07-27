#pragma once

#include <spdlog/spdlog.h>

namespace common::log {
enum LoggerType {
  CONSOLE = 0,  // 控制台输出
  RUNTIME,      // 运行日志

  NUM_LOGGERS
};

spdlog::logger* get_logger(LoggerType type);
void register_logger(LoggerType type, std::shared_ptr<spdlog::logger> logger);
void set_level(spdlog::level::level_enum level);
void flush();

template <typename... Args>
void trace(spdlog::source_loc loc, Args&&... args) {
  get_logger(LoggerType::CONSOLE)
      ->log(loc, spdlog::level::trace, std::forward<Args>(args)...);
  get_logger(LoggerType::RUNTIME)
      ->log(loc, spdlog::level::trace, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(spdlog::source_loc loc, Args&&... args) {
  get_logger(LoggerType::CONSOLE)
      ->log(loc, spdlog::level::debug, std::forward<Args>(args)...);
  get_logger(LoggerType::RUNTIME)
      ->log(loc, spdlog::level::debug, std::forward<Args>(args)...);
}

template <typename... Args>
void info(spdlog::source_loc loc, Args&&... args) {
  get_logger(LoggerType::CONSOLE)
      ->log(loc, spdlog::level::info, std::forward<Args>(args)...);
  get_logger(LoggerType::RUNTIME)
      ->log(loc, spdlog::level::info, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(spdlog::source_loc loc, Args&&... args) {
  get_logger(LoggerType::CONSOLE)
      ->log(loc, spdlog::level::warn, std::forward<Args>(args)...);
  get_logger(LoggerType::RUNTIME)
      ->log(loc, spdlog::level::warn, std::forward<Args>(args)...);
}

template <typename... Args>
void error(spdlog::source_loc loc, Args&&... args) {
  get_logger(LoggerType::CONSOLE)
      ->log(loc, spdlog::level::err, std::forward<Args>(args)...);
  get_logger(LoggerType::RUNTIME)
      ->log(loc, spdlog::level::err, std::forward<Args>(args)...);
}

template <typename... Args>
void fatal(spdlog::source_loc loc, Args&&... args) {
  get_logger(LoggerType::CONSOLE)
      ->log(loc, spdlog::level::critical, std::forward<Args>(args)...);
  get_logger(LoggerType::RUNTIME)
      ->log(loc, spdlog::level::critical, std::forward<Args>(args)...);
}

void clean_logfiles(
    const std::string& directory, const std::size_t max_files,
    const std::function<bool(std::string_view)>& is_file_needed = {});
}  // namespace common::log

#if !defined(NDEBUG)
#define LOG_TRACE(...)                                                \
  common::log::trace(spdlog::source_loc{__FILE__, __LINE__, nullptr}, \
                     __VA_ARGS__)
#define LOG_DEBUG(...)                                                \
  common::log::debug(spdlog::source_loc{__FILE__, __LINE__, nullptr}, \
                     __VA_ARGS__)
#define LOG_INFO(...)                                                \
  common::log::info(spdlog::source_loc{__FILE__, __LINE__, nullptr}, \
                    __VA_ARGS__)
#define LOG_WARN(...)                                                \
  common::log::warn(spdlog::source_loc{__FILE__, __LINE__, nullptr}, \
                    __VA_ARGS__)
#define LOG_ERROR(...)                                                \
  common::log::error(spdlog::source_loc{__FILE__, __LINE__, nullptr}, \
                     __VA_ARGS__)
#define LOG_FATAL(...)                                                \
  common::log::fatal(spdlog::source_loc{__FILE__, __LINE__, nullptr}, \
                     __VA_ARGS__)
#else
#define LOG_TRACE(...) common::log::trace(spdlog::source_loc{}, __VA_ARGS__)
#define LOG_DEBUG(...) common::log::debug(spdlog::source_loc{}, __VA_ARGS__)
#define LOG_INFO(...) common::log::info(spdlog::source_loc{}, __VA_ARGS__)
#define LOG_WARN(...) common::log::warn(spdlog::source_loc{}, __VA_ARGS__)
#define LOG_ERROR(...) common::log::error(spdlog::source_loc{}, __VA_ARGS__)
#define LOG_FATAL(...) common::log::fatal(spdlog::source_loc{}, __VA_ARGS__)
#endif