#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/settings/Setting.h"

namespace lex::core::settings {

class SettingGroup {
 public:
  explicit SettingGroup(std::string name = "General")
      : name_(std::move(name)) {}

  template <typename S>
  S& addOwned(std::unique_ptr<S> setting) {
    S& reference = *setting;
    settings_.push_back(std::move(setting));
    return reference;
  }

  template <typename S, typename... Args>
  S& add(Args&&... args) {
    return addOwned(std::make_unique<S>(std::forward<Args>(args)...));
  }

  const std::vector<std::unique_ptr<SettingBase>>& settings() const noexcept {
    return settings_;
  }

  std::vector<std::unique_ptr<SettingBase>>& settings() noexcept {
    return settings_;
  }

  SettingBase* find(std::string_view name) {
    for (const std::unique_ptr<SettingBase>& setting : settings_) {
      if (setting->name() == name) return setting.get();
    }
    return nullptr;
  }

  const SettingBase* find(std::string_view name) const {
    for (const std::unique_ptr<SettingBase>& setting : settings_) {
      if (setting->name() == name) return setting.get();
    }
    return nullptr;
  }

  const std::string& name() const noexcept { return name_; }

 private:
  std::string name_;
  std::vector<std::unique_ptr<SettingBase>> settings_;
};

}  // namespace lex::core::settings
