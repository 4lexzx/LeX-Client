#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace lex::core::util {

enum class LogLevel : std::uint8_t { Debug = 0, Info = 1, Warning = 2, Error = 3, None = 4 };

class Logger {
 public:
  static Logger& instance();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void setLevel(LogLevel level);
  LogLevel level() const noexcept;

  void setLogFilePath(std::filesystem::path path);
  const std::filesystem::path& logFilePath() const noexcept;
  void close();

  void log(LogLevel level, std::string_view message, std::string_view source = {});
  void debug(std::string_view message, std::string_view source = {});
  void info(std::string_view message, std::string_view source = {});
  void warning(std::string_view message, std::string_view source = {});
  void error(std::string_view message, std::string_view source = {});

 private:
  Logger() = default;
  ~Logger() = default;

  mutable std::mutex mutex_;
  LogLevel level_ = LogLevel::Info;
  std::filesystem::path logFilePath_;
  std::ofstream file_;
};

}  // namespace lex::core::util

#define LEX_LOG_DEBUG(message) \
  ::lex::core::util::Logger::instance().debug((message), __FILE__)
#define LEX_LOG_INFO(message) \
  ::lex::core::util::Logger::instance().info((message), __FILE__)
#define LEX_LOG_WARNING(message) \
  ::lex::core::util::Logger::instance().warning((message), __FILE__)
#define LEX_LOG_ERROR(message) \
  ::lex::core::util::Logger::instance().error((message), __FILE__)
