/**
 * logging.hpp - 日志系统 (基于 spdlog)
 */

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace tensyoki {

class Logging {
public:
    static void init(const std::string& log_file = "tensyoki.log", 
                     spdlog::level::level_enum console_level = spdlog::level::info,
                     spdlog::level::level_enum file_level = spdlog::level::debug);
    
    static void shutdown();
    
    static std::shared_ptr<spdlog::logger> get_logger(const std::string& name = "tensyoki");
    
    // 设置日志级别
    static void set_console_level(spdlog::level::level_enum level);
    static void set_file_level(spdlog::level::level_enum level);
    static void set_pattern(const std::string& pattern);
    
    // 刷新所有日志
    static void flush();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
    static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> file_sink_;
};

// 便利宏
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(tensyoki::Logging::get_logger(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(tensyoki::Logging::get_logger(), __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_INFO(tensyoki::Logging::get_logger(), __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_WARN(tensyoki::Logging::get_logger(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(tensyoki::Logging::get_logger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(tensyoki::Logging::get_logger(), __VA_ARGS__)

// 条件日志
#define LOG_TRACE_IF(cond, ...) if (cond) LOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG_IF(cond, ...) if (cond) LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO_IF(cond, ...) if (cond) LOG_INFO(__VA_ARGS__)
#define LOG_WARN_IF(cond, ...) if (cond) LOG_WARN(__VA_ARGS__)
#define LOG_ERROR_IF(cond, ...) if (cond) LOG_ERROR(__VA_ARGS__)

} // namespace tensyoki