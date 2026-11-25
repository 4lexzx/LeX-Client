#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace lex::core::settings {

struct Color {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;

  static Color fromArgb(std::uint32_t argb) noexcept;
  std::uint32_t toArgb() const noexcept;

  friend bool operator==(const Color&, const Color&) = default;
};

struct Keybind {
  // 0 = sin asignar. Codigo portable: la plataforma interpreta el numero
  // (en Windows sera un VK_*, en Android un KeyEvent de Java).
  int key = 0;
  bool control = false;
  bool shift = false;
  bool alt = false;

  std::string label() const;
  nlohmann::json toJson() const;
  void fromJson(const nlohmann::json& value);

  friend bool operator==(const Keybind&, const Keybind&) = default;
};

}  // namespace lex::core::settings
