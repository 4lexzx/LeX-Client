#include "core/settings/Types.h"

#include <algorithm>
#include <string>

#include <nlohmann/json.hpp>

namespace lex::core::settings {

Color Color::fromArgb(std::uint32_t argb) noexcept {
  const auto channel = [](std::uint32_t v) noexcept {
    return static_cast<float>(v) / 255.0f;
  };
  return Color{channel((argb >> 16) & 0xFFu), channel((argb >> 8) & 0xFFu),
               channel(argb & 0xFFu), channel((argb >> 24) & 0xFFu)};
}

std::uint32_t Color::toArgb() const noexcept {
  const auto quantize = [](float v) noexcept {
    return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
  };
  return (quantize(a) << 24) | (quantize(r) << 16) | (quantize(g) << 8) |
         quantize(b);
}

std::string Keybind::label() const {
  std::string result;
  if (control) result += "Ctrl+";
  if (shift) result += "Shift+";
  if (alt) result += "Alt+";
  if (key == 0) {
    result += "Sin asignar";
  } else {
    result += "Code(" + std::to_string(key) + ")";
  }
  return result;
}

nlohmann::json Keybind::toJson() const {
  return nlohmann::json{{"key", key},
                        {"control", control},
                        {"shift", shift},
                        {"alt", alt}};
}

void Keybind::fromJson(const nlohmann::json& value) {
  if (!value.is_object()) {
    *this = Keybind{};
    return;
  }
  key = value.value("key", 0);
  control = value.value("control", false);
  shift = value.value("shift", false);
  alt = value.value("alt", false);
}

}  // namespace lex::core::settings
