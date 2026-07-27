#include "logging.h"

#include <spdlog/async.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif
#include <spdlog/sinks/daily_file_sink.h>

#include <filesystem>

namespace {
std::vector<std::shared_ptr<spdlog::logger>>& loggers() {
  static std::vector<std::shared_ptr<spdlog::logger>> loggers(
      common::log::LoggerType::NUM_LOGGERS, nullptr);
  return loggers;
}

std::vector<std::shared_ptr<spdlog::logger>>& disused_loggers() {
  /// disused loggers 用以延长“被替换”的logger的生命周期，降低线程安全风险
  /// 但同一类型只会保持一个弃用对象，故不建议重复调用`register_logger`注册同类型对象
  static std::vector<std::shared_ptr<spdlog::logger>> loggers(
      common::log::LoggerType::NUM_LOGGERS, nullptr);
  return loggers;
}
}  // namespace

namespace common::log {

namespace {
struct LogInitializer {
  LogInitializer() {
    static int logger_name_id = 0;
    std::string id = std::to_string(++logger_name_id);

#if !NDEBUG
    constexpr const char* runtime_pattern =
        "%Y-%m-%d %H:%M:%S.%e %t %^%-5l%$ %s:%# %v";
#else
    constexpr const char* runtime_pattern =
        "%Y-%m-%d %H:%M:%S.%e %t %^%-5l%$ %v";
#endif
    int save_days = 30;
#ifdef _WIN32
    auto console_sink =
        std::make_shared<spdlog::sinks::wincolor_stderr_sink_mt>();
#else
    auto console_sink =
        std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>();
    console_sink->set_color(spdlog::level::debug, console_sink->reset);
#endif
    auto console_logger =
        std::make_shared<spdlog::logger>("console_" + id, console_sink);
    console_logger->set_pattern(runtime_pattern);

    auto runtime_logger =
        spdlog::create_async<spdlog::sinks::daily_file_format_sink_mt>(
            "runtime_" + id, "./logs/logfile/%Y-%m-%d.log", 0, 0, false,
            save_days);
    runtime_logger->set_pattern(runtime_pattern);
    runtime_logger->flush_on(spdlog::level::info);

    register_logger(LoggerType::CONSOLE, console_logger);
    register_logger(LoggerType::RUNTIME, runtime_logger);
  }
};
static LogInitializer g_log_init;
}  // namespace

spdlog::logger* get_logger(const LoggerType type) {
  return loggers()[type].get();
}

void register_logger(LoggerType type, std::shared_ptr<spdlog::logger> logger) {
  disused_loggers()[type] = loggers()[type];  // 存旧
  loggers()[type] = logger;                   // 换新
}

void set_level(spdlog::level::level_enum level) {
  auto& loggers = ::loggers();
  if (loggers[LoggerType::CONSOLE] != nullptr) {
    loggers[LoggerType::CONSOLE]->set_level(level);
  }
  if (loggers[LoggerType::RUNTIME] != nullptr) {
    loggers[LoggerType::RUNTIME]->set_level(level);
  }
}

void flush() {
  for (auto& logger : ::loggers()) {
    if (logger != nullptr) {
      logger->flush();
    }
  }
}

void clean_logfiles(
    const std::string& directory, const std::size_t max_files,
    const std::function<bool(std::string_view)>& is_file_needed) {
  std::filesystem::path dir{directory};
  if (!std::filesystem::is_directory(dir)) return;

  std::vector<std::filesystem::directory_entry> entries;

  // 收集满足条件的普通文件
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file() &&
        (!is_file_needed || is_file_needed(entry.path().filename().string()))) {
      entries.emplace_back(entry);
    }
  }

  // 文件数超限就按“文件名升序”删除最早的
  if (entries.size() > max_files) {
    std::sort(entries.begin(), entries.end(),
              [](const std::filesystem::directory_entry& a,
                 const std::filesystem::directory_entry& b) {
                return a.path().filename() < b.path().filename();
              });

    for (std::size_t i = 0; i < entries.size() - max_files; ++i) {
      std::filesystem::remove(entries[i]);
    }
  }
}

}  // namespace common::log