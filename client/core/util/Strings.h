#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace lex::core::util {

inline std::string trim(std::string_view text) {
  const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  std::size_t first = 0;
  while (first < text.size() && isSpace(static_cast<unsigned char>(text[first]))) {
    ++first;
  }
  std::size_t last = text.size();
  while (last > first && isSpace(static_cast<unsigned char>(text[last - 1]))) {
    --last;
  }
  return std::string(text.substr(first, last - first));
}

inline std::string toLower(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

inline std::vector<std::string> split(std::string_view text, char delimiter) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find(delimiter, start);
    if (end == std::string_view::npos) {
      parts.emplace_back(text.substr(start));
      break;
    }
    parts.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

}  // namespace lex::core::util
