#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/module/Category.h"
#include "core/settings/SettingGroup.h"
#include "core/settings/Types.h"

namespace lex::core {

class IPlatform;

namespace config {
class ConfigManager;
}

namespace module {

class Module {
 public:
  Module(std::string name, std::string description, Category category,
         std::string iconId);
  virtual ~Module();

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  const std::string& name() const noexcept { return name_; }
  const std::string& description() const noexcept { return description_; }
  Category category() const noexcept { return category_; }
  const std::string& iconId() const noexcept { return iconId_; }

  bool isEnabled() const noexcept { return enabled_; }
  void setEnabled(bool enabled);

  const settings::Keybind& keybind() const noexcept { return keybind_; }
  void setKeybind(const settings::Keybind& keybind) { keybind_ = keybind; }

  settings::SettingGroup& settings() noexcept { return settings_; }
  const settings::SettingGroup& settings() const noexcept { return settings_; }

  IPlatform* platform() const noexcept { return platform_; }

  // El manager inyecta dependencias justo despues del registro.
  void attach(IPlatform& platform, events::EventBus& bus,
              config::ConfigManager& config);
  void detach();

  // Ciclo de vida. El bus solo llama a estos metodos mientras el modulo esta activo.
  virtual void onEnable() {}
  virtual void onDisable() {}
  virtual void onTick(const events::TickEvent&) {}
  virtual void onRender(const events::RenderEvent&) {}
  virtual void onKey(const events::KeyEvent&) {}
  virtual void onMouse(const events::MouseEvent&) {}
  virtual void onWorldJoin(const events::WorldJoinEvent&) {}
  virtual void onShutdown() {}

  // Fase 3 anadira aqui posicion/escala del overlay. Este hueco ya esta en
  // el esquema JSON para no cambiar el formato de config mas adelante.
  virtual nlohmann::json saveExtra() const {
    return nlohmann::json::object();
  }
  virtual void loadExtra(const nlohmann::json&) {}

 private:
  void subscribeEvents();
  void unsubscribeEvents();

  std::string name_;
  std::string description_;
  std::string iconId_;
  Category category_;
  bool enabled_ = false;
  settings::Keybind keybind_;
  settings::SettingGroup settings_;

  IPlatform* platform_ = nullptr;
  events::EventBus* bus_ = nullptr;
  config::ConfigManager* config_ = nullptr;

  events::EventBus::Subscription tickSubscription_;
  events::EventBus::Subscription renderSubscription_;
  events::EventBus::Subscription keySubscription_;
  events::EventBus::Subscription mouseSubscription_;
  events::EventBus::Subscription worldSubscription_;
};

}  // namespace module
}  // namespace lex::core
