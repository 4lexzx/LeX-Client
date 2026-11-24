#pragma once

#include <imgui.h>

#include "core/settings/SettingVisitor.h"

namespace lex::platform::windows::gui {

// Dibuja automaticamente cualquier Setting segun su tipo concreto.
class SettingEditor final : public core::settings::SettingVisitor {
 public:
  static SettingEditor& instance();

  void draw(core::settings::SettingBase& setting);

  void armKeybind(core::settings::KeybindSetting* setting) noexcept {
    armedKeybind_ = setting;
  }
  core::settings::KeybindSetting* armedKeybind() const noexcept {
    return armedKeybind_;
  }

  bool changed() const noexcept { return changed_; }
  void clearChanged() noexcept { changed_ = false; }

  void visit(core::settings::BoolSetting& setting) override;
  void visit(core::settings::IntSetting& setting) override;
  void visit(core::settings::FloatSetting& setting) override;
  void visit(core::settings::ColorSetting& setting) override;
  void visit(core::settings::EnumSetting& setting) override;
  void visit(core::settings::KeybindSetting& setting) override;

 private:
  SettingEditor() = default;
  core::settings::KeybindSetting* armedKeybind_ = nullptr;
  bool changed_ = false;
};

}  // namespace lex::platform::windows::gui
