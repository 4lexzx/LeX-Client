#include "core/util/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lex::core::util {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::setLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  level_ = level;
}

LogLevel Logger::level() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return level_;
}

void Logger::setLogFilePath(std::filesystem::path path) {
  std::lock_guard<std::mutex> lock(mutex_);
  file_.close();
  logFilePath_ = std::move(path);
  if (!logFilePath_.empty()) {
    std::error_code error;
    std::filesystem::create_directories(logFilePath_.parent_path(), error);
    file_.open(logFilePath_, std::ios::app);
  }
}

const std::filesystem::path& Logger::logFilePath() const noexcept {
  return logFilePath_;
}

void Logger::close() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_.is_open()) file_.close();
}

void Logger::log(LogLevel level, std::string_view message,
                 std::string_view source) {
  if (level == LogLevel::None) return;

  std::lock_guard<std::mutex> lock(mutex_);
  if (static_cast<int>(level) < static_cast<int>(level_)) return;

  const std::time_t now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  // std::localtime usa un buffer estatico; el mutex de Logger lo protege.
  // Se evita localtime_s (MSVC) para mantener el nucleo portable a MinGW.
  const std::tm* localTime = std::localtime(&now);
  std::tm safeTime = localTime != nullptr ? *localTime : std::tm{};

  static constexpr std::string_view kLevelNames[] = {"DEBUG", "INFO ",
                                                      "WARN ", "ERROR"};
  const std::size_t index =
      static_cast<std::size_t>(level) < 4 ? static_cast<std::size_t>(level) : 3;

  std::ostringstream line;
  line << '[' << std::put_time(&safeTime, "%H:%M:%S") << "] ["
       << kLevelNames[index] << "] " << message;
  if (!source.empty()) line << "  (" << source << ')';

  const std::string formatted = line.str();
  std::cout << formatted << '\n';
  if (file_.is_open()) {
    file_ << formatted << '\n';
    file_.flush();
  }
}

void Logger::debug(std::string_view message, std::string_view source) {
  log(LogLevel::Debug, message, source);
}

void Logger::info(std::string_view message, std::string_view source) {
  log(LogLevel::Info, message, source);
}

void Logger::warning(std::string_view message, std::string_view source) {
  log(LogLevel::Warning, message, source);
}

void Logger::error(std::string_view message, std::string_view source) {
  log(LogLevel::Error, message, source);
}

}  // namespace lex::core::util
