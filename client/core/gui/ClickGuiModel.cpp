#include "core/gui/ClickGuiModel.h"

#include <algorithm>

#include "core/module/Category.h"
#include "core/util/Logger.h"
#include "core/util/Math.h"
#include "core/util/Strings.h"

namespace lex::core::gui {

ClickGuiModel::ClickGuiModel(module::ModuleManager& manager)
    : manager_(manager) {
  theme_.menuKey = GuiTheme::defaultKey();
}

void ClickGuiModel::update(float deltaSeconds) {
  const float smoothing =
      theme_.animations ? 1.0f / std::max(theme_.animationSpeed, 1.0f)
                        : 0.0f;
  const float target = targetOpen_ ? 1.0f : 0.0f;
  if (theme_.animations) {
    openAmount_ = util::damp(openAmount_, target, smoothing, deltaSeconds);
  } else {
    openAmount_ = target;
  }
  if (!targetOpen_ && openAmount_ < 0.002f) openAmount_ = 0.0f;

  if (tooltipTime_ > 0.0f) {
    tooltipTime_ -= deltaSeconds;
    if (tooltipTime_ <= 0.0f) tooltip_.clear();
  }
}

void ClickGuiModel::toggle() {
  targetOpen_ = !targetOpen_;
  if (!targetOpen_) {
    selectedModule_ = nullptr;
    tooltip_.clear();
    tooltipTime_ = 0.0f;
  }
}

void ClickGuiModel::setSearch(std::string search) {
  search_ = util::toLower(util::trim(search));
}

std::vector<module::Module*> ClickGuiModel::visibleModules() const {
  std::vector<module::Module*> result;

  const auto matches = [this](const module::Module* module) {
    if (search_.empty()) return true;
    const std::string haystack =
        util::toLower(module->name() + " " + module->description());
    return haystack.find(search_) != std::string::npos;
  };

  if (tab_ == Tab::All) {
    for (const std::unique_ptr<module::Module>& module : manager_.modules()) {
      if (matches(module.get())) result.push_back(module.get());
    }
    return result;
  }

  if (tab_ == Tab::Favorites) {
    for (const std::unique_ptr<module::Module>& module : manager_.modules()) {
      if (isFavorite(module->name()) && matches(module.get())) {
        result.push_back(module.get());
      }
    }
    return result;
  }

  if (tab_ == Tab::Settings) return result;

  module::Category category = module::Category::Hud;
  switch (tab_) {
    case Tab::Hud:
      category = module::Category::Hud;
      break;
    case Tab::Visual:
      category = module::Category::Visual;
      break;
    case Tab::Utility:
      category = module::Category::Utility;
      break;
    case Tab::Cosmetics:
      category = module::Category::Cosmetics;
      break;
    default:
      break;
  }

  for (const std::unique_ptr<module::Module>& module : manager_.modules()) {
    if (module->category() == category && matches(module.get())) {
      result.push_back(module.get());
    }
  }
  return result;
}

void ClickGuiModel::toggleFavorite(std::string_view name) {
  const std::string key(name);
  auto it = favorites_.find(key);
  if (it == favorites_.end()) {
    favorites_.insert(key);
  } else {
    favorites_.erase(it);
  }
}

bool ClickGuiModel::isFavorite(std::string_view name) const {
  return favorites_.find(std::string(name)) != favorites_.end();
}

void ClickGuiModel::selectModule(std::string_view name) {
  selectedModule_ = manager_.find(name);
}

void ClickGuiModel::setTooltip(std::string text) {
  tooltip_ = std::move(text);
  tooltipTime_ = 2.5f;
}

void ClickGuiModel::resetModule(std::string_view name) {
  module::Module* module = manager_.find(name);
  if (module == nullptr) return;

  for (const std::unique_ptr<settings::SettingBase>& setting :
       module->settings().settings()) {
    setting->reset();
  }
  module->setKeybind(settings::Keybind{});
  util::Logger::instance().info("Modulo restablecido: " + module->name());
}

nlohmann::json ClickGuiModel::toJson() const {
  nlohmann::json favorites = nlohmann::json::array();
  for (const std::string& favorite : favorites_) favorites.push_back(favorite);

  return nlohmann::json{{"open", targetOpen_},
                        {"tab", static_cast<int>(tab_)},
                        {"search", search_},
                        {"favorites", favorites},
                        {"theme", theme_.toJson()}};
}

void ClickGuiModel::fromJson(const nlohmann::json& value) {
  if (!value.is_object()) return;

  targetOpen_ = value.value("open", false);
  const int rawTab = value.value("tab", static_cast<int>(Tab::Hud));
  if (rawTab >= static_cast<int>(Tab::Hud) &&
      rawTab <= static_cast<int>(Tab::All)) {
    tab_ = static_cast<Tab>(rawTab);
  }
  search_ = value.value("search", std::string{});
  openAmount_ = targetOpen_ ? 1.0f : 0.0f;

  favorites_.clear();
  if (value.contains("favorites") && value["favorites"].is_array()) {
    for (const nlohmann::json& favorite : value["favorites"]) {
      if (favorite.is_string()) favorites_.insert(favorite.get<std::string>());
    }
  }
  if (value.contains("theme")) theme_.fromJson(value["theme"]);
}

}  // namespace lex::core::gui
