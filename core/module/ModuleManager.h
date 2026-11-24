#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/module/Category.h"
#include "core/module/Module.h"

namespace lex::core {

class IPlatform;

namespace events {
class EventBus;
}

namespace config {
class ConfigManager;
}

namespace module {

class ModuleManager {
 public:
  static ModuleManager& instance();

  ModuleManager(const ModuleManager&) = delete;
  ModuleManager& operator=(const ModuleManager&) = delete;

  void registerModule(std::unique_ptr<Module> module);
  Module* find(std::string_view name) const;

  const std::vector<std::unique_ptr<Module>>& modules() const noexcept {
    return modules_;
  }

  std::vector<Module*> modulesByCategory(Category category) const;

  void attachAll(IPlatform& platform, events::EventBus& bus,
                 config::ConfigManager& config);
  void detachAll();
  void setAllEnabled(bool enabled);

  void saveToConfig(config::ConfigManager& config) const;
  void loadFromConfig(config::ConfigManager& config);

  std::size_t count() const noexcept { return modules_.size(); }

 private:
  ModuleManager() = default;

  std::vector<std::unique_ptr<Module>> modules_;
  std::unordered_map<std::string, Module*> byName_;
};

}  // namespace module
}  // namespace lex::core

// Autorregistro: basta con poner LEX_REGISTER_MODULE(MiModulo) al final del .cpp.
#define LEX_REGISTER_MODULE(ModuleType)                                       \
  namespace {                                                                 \
  struct LexModuleRegistrar_##ModuleType {                                    \
    LexModuleRegistrar_##ModuleType() {                                       \
      ::lex::core::module::ModuleManager::instance().registerModule(          \
          std::make_unique<ModuleType>());                                    \
    }                                                                         \
  };                                                                           \
  static LexModuleRegistrar_##ModuleType s_lexModuleRegistrar_##ModuleType;    \
  }
