#include "core/gui/GuiTheme.h"

#include "core/util/Logger.h"

namespace lex::core::gui {

nlohmann::json GuiTheme::toJson() const {
  return nlohmann::json{{"accent", accent.toArgb()},
                        {"darkMode", darkMode},
                        {"opacity", opacity},
                        {"dimBackground", dimBackground},
                        {"animations", animations},
                        {"animationSpeed", animationSpeed},
                        {"menuKey", menuKey.toJson()}};
}

void GuiTheme::fromJson(const nlohmann::json& value) {
  if (!value.is_object()) return;

  try {
    if (value.contains("accent") && value["accent"].is_number_unsigned()) {
      accent = settings::Color::fromArgb(value["accent"].get<std::uint32_t>());
    }
    darkMode = value.value("darkMode", darkMode);
    opacity = value.value("opacity", opacity);
    // Compatibilidad con perfiles de la Fase 2 que guardaron "blur".
    dimBackground = value.value("dimBackground", value.value("blur", dimBackground));
    animations = value.value("animations", animations);
    animationSpeed = value.value("animationSpeed", animationSpeed);
    if (value.contains("menuKey")) menuKey.fromJson(value["menuKey"]);
  } catch (const std::exception& error) {
    util::Logger::instance().warning(
        std::string("Tema JSON invalido: ") + error.what());
  }
}

}  // namespace lex::core::gui
