#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/gui/GuiTheme.h"
#include "core/module/ModuleManager.h"

namespace lex::core::gui {

// Estado y logica del ClickGUI. No dibuja nada: la capa Windows lo lee.
class ClickGuiModel {
 public:
  enum class Tab : int {
    Hud = 0,
    Visual,
    Utility,
    Cosmetics,
    Settings,
    Favorites,
    All
  };

  explicit ClickGuiModel(module::ModuleManager& manager);

  void update(float deltaSeconds);

  void toggle();
  void setOpen(bool open) noexcept { targetOpen_ = open; }
  bool isOpen() const noexcept { return targetOpen_; }
  bool isVisible() const noexcept {
    return targetOpen_ || openAmount_ > 0.001f;
  }
  float openAmount() const noexcept { return openAmount_; }

  void setSearch(std::string search);
  const std::string& search() const noexcept { return search_; }

  void setTab(Tab tab) noexcept { tab_ = tab; }
  Tab tab() const noexcept { return tab_; }

  std::vector<module::Module*> visibleModules() const;

  void toggleFavorite(std::string_view name);
  bool isFavorite(std::string_view name) const;

  void selectModule(std::string_view name);
  void closeSettings() noexcept { selectedModule_ = nullptr; }
  module::Module* selectedModule() const noexcept { return selectedModule_; }
  bool settingsOpen() const noexcept { return selectedModule_ != nullptr; }

  void setTooltip(std::string text);
  const std::string& tooltip() const noexcept { return tooltip_; }
  float tooltipTime() const noexcept { return tooltipTime_; }

  void resetModule(std::string_view name);

  GuiTheme& theme() noexcept { return theme_; }
  const GuiTheme& theme() const noexcept { return theme_; }

  nlohmann::json toJson() const;
  void fromJson(const nlohmann::json& value);

 private:
  module::ModuleManager& manager_;
  GuiTheme theme_;
  Tab tab_ = Tab::Hud;
  bool targetOpen_ = false;
  float openAmount_ = 0.0f;
  std::string search_;
  std::set<std::string> favorites_;
  module::Module* selectedModule_ = nullptr;
  std::string tooltip_;
  float tooltipTime_ = 0.0f;
};

}  // namespace lex::core::gui
