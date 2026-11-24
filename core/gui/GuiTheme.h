#pragma once

#include <nlohmann/json.hpp>

#include "core/settings/Types.h"

namespace lex::core::gui {

// Tema del ClickGUI. Vive en core: no sabe nada de ImGui ni de Windows.
struct GuiTheme {
  settings::Color accent{0.36f, 0.64f, 1.0f, 1.0f};
  bool darkMode = true;
  float opacity = 0.94f;
  bool blur = true;
  bool animations = true;
  float animationSpeed = 12.0f;

  // Codigo portable: en Windows se interpreta como VK_RSHIFT (0xA1).
  settings::Keybind menuKey;

  nlohmann::json toJson() const;
  void fromJson(const nlohmann::json& value);
};

}  // namespace lex::core::gui
