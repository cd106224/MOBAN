#include "logging.h"

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

void clean_logfiles(const std::string& directory, const std::size_t max_files,
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