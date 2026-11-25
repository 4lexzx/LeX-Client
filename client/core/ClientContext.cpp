#include "core/ClientContext.h"

#include "core/module/ModuleManager.h"
#include "core/util/Logger.h"

namespace lex::core {

void ClientContext::initialize(IPlatform& platform,
                               std::filesystem::path configDirectory,
                               std::string profile) {
  if (initialized_) return;

  platform_ = &platform;
  config_.setBaseDirectory(std::move(configDirectory));
  config_.setProfile(std::move(profile));
  config_.load();

  const auto& loggerPath = config_.baseDirectory() / "lexclient.log";
  util::Logger::instance().setLogFilePath(loggerPath);

  module::ModuleManager& modules = module::ModuleManager::instance();
  modules.attachAll(*platform_, events_, config_);
  modules.loadFromConfig(config_);

  initialized_ = true;
  LEX_LOG_INFO("LeX-Client inicializado en " + std::string(platform.name()));
}

void ClientContext::tick(float deltaSeconds) {
  if (!initialized_) return;

  events::TickEvent tick;
  tick.deltaSeconds = deltaSeconds;
  events_.publish(tick);

  events::RenderEvent render;
  render.deltaSeconds = deltaSeconds;
  events_.publish(render);

  autosaveClock_ += deltaSeconds;
  if (config_.dirty() && autosaveClock_ >= config_.autosaveSeconds()) {
    module::ModuleManager::instance().saveToConfig(config_);
    config_.save();
    autosaveClock_ = 0.0f;
  }
}

void ClientContext::shutdown() {
  if (!initialized_) return;

  events::ShutdownEvent shutdownEvent;
  events_.publish(shutdownEvent);

  module::ModuleManager& modules = module::ModuleManager::instance();
  modules.saveToConfig(config_);
  modules.setAllEnabled(false);
  modules.detachAll();
  config_.save();

  initialized_ = false;
  platform_ = nullptr;
  LEX_LOG_INFO("LeX-Client detenido");
  util::Logger::instance().close();
}

}  // namespace lex::core
