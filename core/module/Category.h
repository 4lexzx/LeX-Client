#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace lex::core::module {

enum class Category : std::uint8_t {
  Hud = 0,
  Visual,
  Utility,
  Cosmetics,
  Settings,
  Count
};

constexpr std::string_view categoryName(Category category) noexcept {
  switch (category) {
    case Category::Hud:
      return "HUD";
    case Category::Visual:
      return "Visual";
    case Category::Utility:
      return "Utilidad";
    case Category::Cosmetics:
      return "Cosmeticos";
    case Category::Settings:
      return "Ajustes";
    case Category::Count:
      break;
  }
  return "Desconocida";
}

// Id logico del icono; la capa grafica lo resolvera a un asset embebido.
constexpr std::string_view categoryIconId(Category category) noexcept {
  switch (category) {
    case Category::Hud:
      return "icon.category.hud";
    case Category::Visual:
      return "icon.category.visual";
    case Category::Utility:
      return "icon.category.utility";
    case Category::Cosmetics:
      return "icon.category.cosmetics";
    case Category::Settings:
      return "icon.category.settings";
    case Category::Count:
      break;
  }
  return "icon.category.unknown";
}

constexpr std::optional<Category> categoryFromName(
    std::string_view name) noexcept {
  if (name == "HUD") return Category::Hud;
  if (name == "Visual") return Category::Visual;
  if (name == "Utilidad") return Category::Utility;
  if (name == "Cosmeticos") return Category::Cosmetics;
  if (name == "Ajustes") return Category::Settings;
  return std::nullopt;
}

}  // namespace lex::core::module
