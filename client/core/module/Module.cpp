#include "core/module/Module.h"

#include "core/config/ConfigManager.h"
#include "core/platform/IPlatform.h"
#include "core/util/Logger.h"

namespace lex::core::module {

Module::Module(std::string name, std::string description, Category category,
               std::string iconId)
    : name_(std::move(name)),
      description_(std::move(description)),
      iconId_(std::move(iconId)),
      category_(category) {}

Module::~Module() { detach(); }

void Module::attach(IPlatform& platform, events::EventBus& bus,
                    config::ConfigManager& config) {
  platform_ = &platform;
  bus_ = &bus;
  config_ = &config;

  for (const std::unique_ptr<settings::SettingBase>& setting :
       settings_.settings()) {
    setting->addChangeCallback([this]() {
      if (config_ != nullptr) config_->markDirty();
    });
  }

  if (enabled_) subscribeEvents();
}

void Module::detach() {
  unsubscribeEvents();
  platform_ = nullptr;
  bus_ = nullptr;
  config_ = nullptr;
}

void Module::setEnabled(bool enabled) {
  if (enabled_ == enabled) return;

  enabled_ = enabled;
  if (enabled_) {
    subscribeEvents();
    onEnable();
    LEX_LOG_INFO("Modulo activado: " + name_);
  } else {
    unsubscribeEvents();
    onDisable();
    LEX_LOG_INFO("Modulo desactivado: " + name_);
  }

  if (config_ != nullptr) config_->markDirty();
}

void Module::subscribeEvents() {
  if (bus_ == nullptr) return;

  tickSubscription_ =
      bus_->subscribe<events::TickEvent>([this](events::TickEvent& event) {
        onTick(event);
      });
  renderSubscription_ =
      bus_->subscribe<events::RenderEvent>([this](events::RenderEvent& event) {
        onRender(event);
      });
  keySubscription_ =
      bus_->subscribe<events::KeyEvent>([this](events::KeyEvent& event) {
        onKey(event);
      });
  mouseSubscription_ =
      bus_->subscribe<events::MouseEvent>([this](events::MouseEvent& event) {
        onMouse(event);
      });
  worldSubscription_ =
      bus_->subscribe<events::WorldJoinEvent>([this](events::WorldJoinEvent& event) {
        onWorldJoin(event);
      });
}

void Module::unsubscribeEvents() {
  tickSubscription_.reset();
  renderSubscription_.reset();
  keySubscription_.reset();
  mouseSubscription_.reset();
  worldSubscription_.reset();
}

}  // namespace lex::core::module
