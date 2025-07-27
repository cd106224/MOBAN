#include <spdlog/async.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

#include "logging.h"

namespace {

struct OnceRegister {
  OnceRegister() {
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
    auto console_sink =
        std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>();
    console_sink->set_color(spdlog::level::debug, console_sink->reset);
    auto console_logger =
        std::make_shared<spdlog::logger>("console_" + id, console_sink);
    console_logger->set_pattern(runtime_pattern);

    auto runtime_logger =
        spdlog::create_async<spdlog::sinks::daily_file_format_sink_mt>(
            "runtime_" + id, "./logs/logfile/%Y-%m-%d.log", 0, 0, false,
            save_days);
    runtime_logger->set_pattern(runtime_pattern);
    runtime_logger->flush_on(spdlog::level::info);

    common::log::register_logger(common::log::LoggerType::CONSOLE,
                                 console_logger);
    common::log::register_logger(common::log::LoggerType::RUNTIME,
                                 runtime_logger);
  }
};

static OnceRegister g_once;
}  // namespace