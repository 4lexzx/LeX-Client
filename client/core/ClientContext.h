#pragma once

#include <filesystem>
#include <string>

#include "core/config/ConfigManager.h"
#include "core/events/EventBus.h"
#include "core/platform/IPlatform.h"

namespace lex::core {

// Compone el nucleo. La plataforma se inyecta; el contexto no sabe si es
// Windows, Android o un stub de tests.
class ClientContext {
 public:
  ClientContext() = default;
  ClientContext(const ClientContext&) = delete;
  ClientContext& operator=(const ClientContext&) = delete;

  void initialize(IPlatform& platform, std::filesystem::path configDirectory,
                  std::string profile = "default");
  void tick(float deltaSeconds);
  void shutdown();

  bool initialized() const noexcept { return initialized_; }
  events::EventBus& events() noexcept { return events_; }
  config::ConfigManager& config() noexcept { return config_; }
  IPlatform& platform() noexcept { return *platform_; }

 private:
  events::EventBus events_;
  config::ConfigManager config_{"config"};
  IPlatform* platform_ = nullptr;
  bool initialized_ = false;
  float autosaveClock_ = 0.0f;
};

}  // namespace lex::core
