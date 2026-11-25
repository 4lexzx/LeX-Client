#include "core/module/ModuleManager.h"

#include <exception>
#include <nlohmann/json.hpp>

#include "core/config/ConfigManager.h"
#include "core/events/EventBus.h"
#include "core/platform/IPlatform.h"
#include "core/util/Logger.h"

namespace lex::core::module {

ModuleManager& ModuleManager::instance() {
  static ModuleManager manager;
  return manager;
}

void ModuleManager::registerModule(std::unique_ptr<Module> module) {
  if (!module) return;
  const std::string name = module->name();
  if (byName_.find(name) != byName_.end()) {
    LEX_LOG_WARNING("Modulo duplicado ignorado: " + name);
    return;
  }
  byName_.emplace(name, module.get());
  modules_.push_back(std::move(module));
}

Module* ModuleManager::find(std::string_view name) const {
  auto it = byName_.find(std::string(name));
  return it == byName_.end() ? nullptr : it->second;
}

std::vector<Module*> ModuleManager::modulesByCategory(Category category) const {
  std::vector<Module*> result;
  for (const std::unique_ptr<Module>& module : modules_) {
    if (module->category() == category) result.push_back(module.get());
  }
  return result;
}

void ModuleManager::attachAll(IPlatform& platform, events::EventBus& bus,
                              config::ConfigManager& config) {
  for (const std::unique_ptr<Module>& module : modules_) {
    module->attach(platform, bus, config);
  }
}

void ModuleManager::detachAll() {
  for (const std::unique_ptr<Module>& module : modules_) {
    module->detach();
  }
}

void ModuleManager::setAllEnabled(bool enabled) {
  for (const std::unique_ptr<Module>& module : modules_) {
    module->setEnabled(enabled);
  }
}

void ModuleManager::saveToConfig(config::ConfigManager& config) const {
  nlohmann::json modules = nlohmann::json::object();

  for (const std::unique_ptr<Module>& module : modules_) {
    nlohmann::json node = nlohmann::json::object();
    node["enabled"] = module->isEnabled();
    node["keybind"] = module->keybind().toJson();
    node["extra"] = module->saveExtra();

    nlohmann::json settings = nlohmann::json::object();
    for (const std::unique_ptr<settings::SettingBase>& setting :
         module->settings().settings()) {
      settings[setting->name()] = setting->toJson();
    }
    node["settings"] = settings;
    modules[module->name()] = node;
  }

  nlohmann::json& root = config.root();
  root["version"] = 1;
  root["profile"] = config.profile();
  root["modules"] = modules;
  // No se marca como guardado aqui: ConfigManager::save() es el unico que
  // limpia el flag despues de escribir el archivo en disco.
}

void ModuleManager::loadFromConfig(config::ConfigManager& config) {
  const nlohmann::json& root = config.root();
  if (!root.is_object() || !root.contains("modules") ||
      !root["modules"].is_object()) {
    return;
  }

  const nlohmann::json& allModules = root["modules"];
  for (const std::unique_ptr<Module>& module : modules_) {
    auto nodeIt = allModules.find(module->name());
    if (nodeIt == allModules.end() || !nodeIt->is_object()) continue;
    const nlohmann::json& node = *nodeIt;

    if (node.contains("settings") && node["settings"].is_object()) {
      for (const std::unique_ptr<settings::SettingBase>& setting :
           module->settings().settings()) {
        auto settingIt = node["settings"].find(setting->name());
        if (settingIt == node["settings"].end()) continue;
        try {
          setting->fromJson(*settingIt);
        } catch (const std::exception& error) {
          LEX_LOG_WARNING("Setting invalido '" + setting->name() + "' de '" +
                          module->name() + "': " + error.what());
        }
      }
    }

    if (node.contains("keybind")) {
      settings::Keybind keybind;
      keybind.fromJson(node["keybind"]);
      module->setKeybind(keybind);
    }

    if (node.contains("extra")) module->loadExtra(node["extra"]);

    module->setEnabled(node.value("enabled", false));
  }
}

}  // namespace lex::core::module
